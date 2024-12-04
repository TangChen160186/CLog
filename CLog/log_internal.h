#pragma once
#define MAX_SINKS 32
#include <threads.h>

/**
 * @struct Logger
 * @brief 定义日志器的结构体，管理日志的状态、输出目标和同步机制。
 */
typedef struct Logger {
    bool running;                /**< 日志器是否正在运行 */
    const char* name;            /**< 日志器的名称 */
    bool async;                  /**< 是否异步记录日志 */
    struct LogSink* sinks[MAX_SINKS]; /**< 日志输出目标数组 */
    int sink_count;              /**< 当前日志输出目标的数量 */

    // 同步相关
    mtx_t mutex;                 /**< 用于保护日志器状态的互斥锁 */

    // 异步相关
    thrd_t worker_thread;        /**< 工作线程，用于异步日志处理 */
    mtx_t queue_mutex;           /**< 用于保护日志队列的互斥锁 */
    mtx_t queue_cnd;           /**< 用于保护日志队列的互斥锁 */
    struct LogMessage* queue_head; /**< 日志消息队列的头指针 */
    struct LogMessage* queue_tail; /**< 日志消息队列的尾指针 */
    // 格式相关
    struct LogFormat* format;    /**< 日志格式配置 */
} Logger;

