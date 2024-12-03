#include "log.h"
#include "log_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

// 处理所有队列中的消息
static void process_all_messages(Logger* logger) {
    while (true) {
        LogMessage* msg = NULL;

        // 从队列中获取消息
        mtx_lock(&logger->queue_mutex);
        if (logger->queue_head) {
            msg = logger->queue_head;
            logger->queue_head = msg->next;
            if (!logger->queue_head) {
                logger->queue_tail = NULL;
            }
        }
        mtx_unlock(&logger->queue_mutex);

        if (!msg) break;  // 如果没有更多消息，退出循环

        // 处理消息 (这里不需要再次获取mutex，因为外层已经加锁)
        for (int i = 0; i < logger->sink_count; i++) {
            if (msg->level >= logger->sinks[i]->config.min_level) {
                logger->sinks[i]->config.write_fn(msg, logger->sinks[i]->user_data);
            }
        }
        free(msg);  // 释放消息内存
    }
}

// 工作线程函数
static int worker_thread_func(void* arg) {
    Logger* logger = (Logger*)arg;

    while (logger->running || logger->queue_head) {
        LogMessage* msg = NULL;

        // 获取当前时间，检查是否需要刷新
        time_t current_time = time(NULL);
        if (logger->auto_flush && logger->flush_interval_ms > 0) {
            if (current_time * 1000 - logger->last_flush_time * 1000 >= logger->flush_interval_ms) {
                // 尝试获取互斥锁，如果获取不到就跳过这次刷新
                if (mtx_trylock(&logger->mutex) == thrd_success) {
                    process_all_messages(logger);
                    logger->last_flush_time = current_time;
                    mtx_unlock(&logger->mutex);
                }
            }
        }

        // 从队列中获取消息
        mtx_lock(&logger->queue_mutex);
        if (logger->queue_head) {
            msg = logger->queue_head;
            logger->queue_head = msg->next;
            if (!logger->queue_head) {
                logger->queue_tail = NULL;
            }
        }
        mtx_unlock(&logger->queue_mutex);

        // 处理消息
        if (msg) {
            mtx_lock(&logger->mutex);
            for (int i = 0; i < logger->sink_count; i++) {
                if (msg->level >= logger->sinks[i]->config.min_level) {
                    logger->sinks[i]->config.write_fn(msg, logger->sinks[i]->user_data);
                }
            }
            mtx_unlock(&logger->mutex);
            free(msg);
        }

        // 如果没有消息，短暂休眠以避免CPU占用过高
        if (!msg) {
            thrd_yield();
        }
    }
    return 0;
}

// 创建Logger
Logger* logger_create(const LogConfig* config) {
    Logger* logger = (Logger*)malloc(sizeof(Logger));
    if (!logger) return NULL;

    logger->running = true;
    logger->name = config->logger_name;
    logger->async = config->async;
    logger->sink_count = 0;
    logger->queue_head = NULL;
    logger->queue_tail = NULL;
    logger->auto_flush = config->auto_flush;
    logger->flush_interval_ms = config->flush_interval_ms;
    logger->last_flush_time = time(NULL);

    mtx_init(&logger->mutex, mtx_plain);

    if (logger->async) {
        mtx_init(&logger->queue_mutex, mtx_plain);
        thrd_create(&logger->worker_thread, worker_thread_func, logger);
    }
    logger->format = config->format;
    return logger;
}

// 销毁Logger
void logger_destroy(Logger* logger) {
    if (!logger) return;

    logger->running = false;

    if (logger->async) {
        thrd_join(logger->worker_thread, NULL);
        mtx_destroy(&logger->queue_mutex);
    }

    mtx_destroy(&logger->mutex);

    // 销毁所有sink
    for (int i = 0; i < logger->sink_count; i++) {
        if (logger->sinks[i]->destroy) {
            logger->sinks[i]->destroy(logger->sinks[i]);
        }
    }
    free(logger);
}

// 添加Sink
void logger_add_sink(Logger* logger, LogSink* sink) {
    if (!logger || !sink || logger->sink_count >= MAX_SINKS) return;
    mtx_lock(&logger->mutex);
    logger->sinks[logger->sink_count++] = sink;
    sink->logger = logger;
    sink->config.format = NULL;
    mtx_unlock(&logger->mutex);
}

// 写日志
void logger_log(Logger* logger, LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (!logger || !fmt) return;

    LogMessage* msg = (LogMessage*)malloc(sizeof(LogMessage));
    if (!msg) return;

    msg->level = level;
    msg->file = file;
    msg->line = line;
    msg->time = time(NULL);
    msg->next = NULL;

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg->message, sizeof(msg->message), fmt, args);
    va_end(args);

    if (logger->async) {
        mtx_lock(&logger->queue_mutex);
        if (!logger->queue_tail) {
            logger->queue_head = msg;
        }
        else {
            logger->queue_tail->next = msg;
        }
        logger->queue_tail = msg;
        mtx_unlock(&logger->queue_mutex);
    }
    else {
        mtx_lock(&logger->mutex);
        for (int i = 0; i < logger->sink_count; i++) {
            if (level >= logger->sinks[i]->config.min_level) {
                logger->sinks[i]->config.write_fn(msg, logger->sinks[i]->user_data);
            }
        }
        mtx_unlock(&logger->mutex);
        free(msg);
    }
}

// 刷新日志
void logger_flush(Logger* logger) {
    if (!logger || !logger->async) return;

    // 获取互斥锁，确保不会与工作线程的自动刷新冲突
    mtx_lock(&logger->mutex);
    process_all_messages(logger);
    mtx_unlock(&logger->mutex);
}