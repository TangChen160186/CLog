#pragma once
#include <stdbool.h>
#include <time.h>

/**
 * @enum LogLevel
 * @brief 定义日志的严重级别。
 */
typedef enum LogLevel {
    LOG_TRACE = 0,  /**< 追踪信息，最详细的级别 */
    LOG_DEBUG,      /**< 调试信息 */
    LOG_INFO,       /**< 普通信息 */
    LOG_WARN,       /**< 警告信息 */
    LOG_ERROR,      /**< 错误信息 */
    LOG_FATAL       /**< 致命错误信息 */
} LogLevel;

/**
 * @enum LogFormatItem
 * @brief 定义日志格式的不同组成部分。
 */
typedef enum LogFormatItem {
    LOG_FMT_TIME,        /**< 时间格式项 */
    LOG_FMT_LEVEL,       /**< 日志级别格式项 */
    LOG_FMT_LOGGER_NAME, /**< 日志器名称格式项 */
    LOG_FMT_FILE,        /**< 源文件名格式项 */
    LOG_FMT_LINE,        /**< 行号格式项 */
    LOG_FMT_MESSAGE,     /**< 日志消息格式项 */
    LOG_FMT_TEXT         /**< 固定文本格式项 */
} LogFormatItem;

/**
 * @enum RollStrategy
 * @brief 文件滚动的策略。
 */
typedef enum RollStrategy {
    ROLL_BY_SIZE,    /**< 按大小滚动文件 */
    ROLL_BY_TIME,    /**< 按时间滚动文件 */
    ROLL_BY_BOTH     /**< 同时按大小和时间滚动文件 */
} RollStrategy;

/**
 * @enum TimeUnit
 * @brief 滚动策略的时间单位。
 */
typedef enum TimeUnit {
    TIME_UNIT_MINUTE, /**< 分钟单位 */
    TIME_UNIT_HOUR,   /**< 小时单位 */
    TIME_UNIT_DAY,    /**< 天单位 */
    TIME_UNIT_WEEK,   /**< 周单位 */
    TIME_UNIT_MONTH   /**< 月单位 */
} TimeUnit;

/**
 * @struct ColorConfig
 * @brief 日志级别颜色的配置。
 */
typedef struct ColorConfig {
    const char* trace_color;  /**< 追踪级别的颜色 */
    const char* debug_color;  /**< 调试级别的颜色 */
    const char* info_color;   /**< 普通信息级别的颜色 */
    const char* warn_color;   /**< 警告级别的颜色 */
    const char* error_color;  /**< 错误级别的颜色 */
    const char* fatal_color;  /**< 致命错误级别的颜色 */
} ColorConfig;

/**
 * @struct LogFormatElement
 * @brief 单个日志格式项的配置。
 */
typedef struct LogFormatElement {
    LogFormatItem type;      /**< 格式项的类型 */
    const char* text;        /**< 固定文本（当类型为LOG_FMT_TEXT时使用） */
    const char* time_fmt;    /**< 时间格式（当类型为LOG_FMT_TIME时使用） */
} LogFormatElement;

/**
 * @struct LogFormat
 * @brief 日志格式的配置，由多个格式项组成。
 */
typedef struct LogFormat {
    LogFormatElement* elements;  /**< 格式项数组 */
    int element_count;           /**< 格式项的数量 */
} LogFormat;

/**
 * @struct LogMessage
 * @brief 包含单条日志信息的结构体。
 */
typedef struct LogMessage {
    LogLevel level;          /**< 日志级别 */
    const char* file;        /**< 源文件名 */
    int line;                /**< 行号 */
    time_t time;             /**< 时间戳 */
    char message[4096];      /**< 日志消息内容 */
    struct LogMessage* next; /**< 指向下一条日志消息的指针（用于队列） */
} LogMessage;

/**
 * @typedef LogWriteFunction
 * @brief 日志写入回调函数的函数指针类型。
 */
typedef void (*LogWriteFunction)(LogMessage* message, void* user_data);

/**
 * @struct SinkConfig
 * @brief 日志输出目标的配置。
 */
typedef struct SinkConfig {
    LogLevel min_level;          /**< 此输出目标的最小日志级别 */
    void* user_data;             /**< 输出目标的用户数据 */
    LogWriteFunction write_fn;   /**< 写入日志消息的函数 */
    bool use_color;              /**< 是否在输出中使用颜色 */
    ColorConfig* color_config;   /**< 颜色配置 */
    LogFormat* format;           /**< 格式配置 */
} SinkConfig;

/**
 * @struct LogSink
 * @brief 日志输出目标的完整定义。
 */
typedef struct LogSink {
    SinkConfig config;           /**< 输出目标的配置 */
    void* user_data;             /**< 输出目标的用户数据 */
    void (*destroy)(struct LogSink* sink);  /**< 销毁输出目标的函数 */
    struct Logger* logger;       /**< 指向关联的日志器的指针 */
} LogSink;

/**
 * @struct LogConfig
 * @brief 日志器的配置。
 */
typedef struct LogConfig {
    const char* logger_name;     /**< 日志器的名称 */
    bool async;                  /**< 是否异步记录日志 */
    LogFormat* format;           /**< 全局格式配置 */
} LogConfig;

/**
 * @struct RollConfig
 * @brief 文件滚动的配置。
 */
typedef struct RollConfig {
    RollStrategy strategy;       /**< 滚动策略 */
    size_t max_file_size;        /**< 滚动的最大文件大小 */
    size_t max_files;            /**< 保留的最大文件数量 */
    TimeUnit time_unit;          /**< 滚动的时间单位 */
    unsigned int time_interval;  /**< 滚动的时间间隔 */
    const char* filename_pattern;/**< 生成文件名的模式 */
} RollConfig;

/**
 * @brief 创建一个新的日志器。
 *
 * @param config 指向日志配置的指针。
 * @return 返回创建的日志器指针，如果失败则返回NULL。
 */
struct Logger* logger_create(const LogConfig* config);

/**
 * @brief 销毁日志器并释放相关资源。
 *
 * @param logger 指向要销毁的日志器。
 */
void logger_destroy(struct Logger* logger);

/**
 * @brief 创建一个控制台输出目标。
 *
 * @param config 指向输出目标配置的指针。
 * @return 返回创建的输出目标指针，如果失败则返回NULL。
 */
LogSink* console_sink_create( SinkConfig* config);

/**
 * @brief 创建一个文件输出目标。
 *
 * @param filename 输出文件的名称。
 * @param config 指向输出目标配置的指针。
 * @return 返回创建的输出目标指针，如果失败则返回NULL。
 */
LogSink* file_sink_create(const char* filename,  SinkConfig* config);

/**
 * @brief 创建一个滚动文件输出目标。
 *
 * @param filename 基础文件名。
 * @param config 指向输出目标配置的指针。
 * @param roll_config 指向滚动配置的指针。
 * @return 返回创建的输出目标指针，如果失败则返回NULL。
 */
LogSink* rolling_file_sink_create(const char* filename,  SinkConfig* config,  RollConfig* roll_config);

/**
 * @brief 向日志器添加一个输出目标。
 *
 * @param logger 指向日志器的指针。
 * @param sink 指向要添加的输出目标。
 */
void logger_add_sink(struct Logger* logger, LogSink* sink);

/**
 * @brief 记录一条日志消息。
 *
 * @param logger 指向日志器的指针。
 * @param level 日志级别。
 * @param file 源文件名。
 * @param line 行号。
 * @param fmt 格式化字符串。
 * @param ... 可变参数列表。
 */
void logger_log(struct Logger* logger, LogLevel level, const char* file, int line, const char* fmt, ...);

/**
 * @brief 创建日志格式配置。
 *
 * @param elements 指向格式项数组的指针。
 * @param count 格式项的数量。
 * @return 返回创建的日志格式指针，如果失败则返回NULL。
 */
LogFormat* log_format_create(const LogFormatElement* elements, int count);


// 便捷宏
#define LOG_TRACE(logger, ...) logger_log(logger, LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(logger, ...) logger_log(logger, LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(logger, ...)  logger_log(logger, LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(logger, ...)  logger_log(logger, LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(logger, ...) logger_log(logger, LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(logger, ...) logger_log(logger, LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)


