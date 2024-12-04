# CLog日志库

这是一个轻量级的C11跨平台日志库，支持多种日志输出方式（如控制台、文件、滚动文件）和格式化功能。

该库支持同步和异步日志记录，适用于需要详细日志记录和管理的应用程序。

该库只使用C11标准库东西！

## 功能特性

- **多种日志级别**：支持 TRACE、DEBUG、INFO、WARN、ERROR、FATAL 等日志级别。
- **多种输出目标**：支持控制台输出、文件输出和滚动文件输出。
- **异步日志记录**：支持异步模式，使用工作线程处理日志消息。
- **日志格式化**：支持自定义日志格式，包括时间、日志级别、文件名、行号等。
- **颜色支持**：支持为不同日志级别设置颜色（仅限控制台输出）。
- **文件滚动**：支持按大小或时间滚动日志文件。

## 文件结构

- `log.h`：日志库的公共接口和数据结构定义，使用者只需保存这个文件即可。
- `log_internal.h`：日志库的内部结构体和常量定义。
- `log.c`：日志库的核心功能实现，包括日志的创建、销毁、添加输出目标、日志写入和刷新。
- `log_sinks.c`：具体的日志输出目标实现，包括控制台输出、文件输出和滚动文件输出。
- `main.c`：示例程序，展示如何使用日志库的所有功能。

## 演示

控制台默认格式

<img src=".\imgs/b2d2f793-9e3f-4006-8ce2-ac1c31fd80c9.png" title="" alt="b2d2f793-9e3f-4006-8ce2-ac1c31fd80c9" data-align="inline">

调整格式，在时间前面加一个固定文本并且调整颜色：

![b42de7ff-94c9-4371-808e-5d9440ce6417](./imgs/b42de7ff-94c9-4371-808e-5d9440ce6417.png)

输出到文件：

![e9c36c2f-26b9-4694-a606-6b820bf9c308](./imgs/e9c36c2f-26b9-4694-a606-6b820bf9c308.png)

roll文件：

![79849681-2cf5-4d6e-a796-27239a70bf96](./imgs/79849681-2cf5-4d6e-a796-27239a70bf96.png)

## 使用方法

### 1. 不适配MSVC编译器，如果使用Visual Studio，需要如下设置

![612163bf-4675-4c21-822d-31df4ede8a7f](./imgs/612163bf-4675-4c21-822d-31df4ede8a7f.png)

### 2. 创建日志器

通过配置创建日志器，具体配置成员意思请看log.h

```c

    // 创建日志配置
    LogConfig config = {
        .logger_name = "MyLogger",
        .async = true,
    };

    // 创建Logger
    Logger* logger = logger_create(&config);
```

### 3.添加控制台输出Sink

每一个输出目标都是一个Sink，程序通过sink来添加对应的输出，用户也可以自定义sink添加进去。最大支持32个输出目标

```c
 // 创建控制台Sink
 SinkConfig console_config = {
     .min_level = LOG_DEBUG,
     .use_color = true,
     .format = NULL  // 使用全局格式
 };
 LogSink* console_sink = console_sink_create(&console_config);
 if (!console_sink) {
     fprintf(stderr, "Failed to create console sink\n");
     logger_destroy(logger);
     return EXIT_FAILURE;
 }
 logger_add_sink(logger, console_sink);
```

### 4. 添加文件输出Sink

```c
 // 创建文件Sink
 SinkConfig file_config = {
     .min_level = LOG_INFO,
     .use_color = false,
     .format = NULL  // 使用全局格式
 };
 LogSink* file_sink = file_sink_create("logfile.log", &file_config);
 if (!file_sink) {
     fprintf(stderr, "Failed to create file sink\n");
     logger_destroy(logger);
     return EXIT_FAILURE;
 }
 logger_add_sink(logger, file_sink);
```

### 5.添加滚动文件输出Sink

```c
 // 创建滚动文件Sink
 RollConfig roll_config = {
     .strategy = ROLL_BY_SIZE,
     .max_file_size = 1024 * 1024,  // 1MB
     .max_files = 5,
     .time_unit = TIME_UNIT_DAY,
     .time_interval = 1,
     .filename_pattern = "%f_%i.log"
 };
 LogSink* rolling_sink = rolling_file_sink_create("rolling_log", &file_config, &roll_config);
 if (!rolling_sink) {
     fprintf(stderr, "Failed to create rolling file sink\n");
     logger_destroy(logger);
     return EXIT_FAILURE;
 }
 logger_add_sink(logger, rolling_sink);
```

其中filename_pattern 是滚动文件日志系统中的一个配置选项，用于定义生成日志文件名的模式。这个模式允许你指定文件名的格式，包括时间戳、文件索引等信息，以便在日志文件滚动时生成唯一的文件名。

**filename_pattern 的格式说明**

* %f：基础文件名（不包括扩展名）。

* %i：文件索引，用于区分同一时间段内的多个日志文件。

* %e：文件扩展名。

* %t：时间戳，通常格式化为 YYYYMMDD_HHMMSS。

假设基础文件名为 logfile，扩展名为 log，filename_pattern 设置为 %f_%i.%e，则生成的文件名可能是：

* logfile_1.log

* logfile_2.log

* logfile_3.log

如果 filename_pattern 设置为 %f_%t_%i.%e，则生成的文件名可能是：

* logfile_20231115_123456_1.log

* logfile_20231115_123456_2.log

### 6. 配置控制台输出颜色

```c
 ColorConfig myColorConfig = {
     .trace_color = "#5F9EA0",  // 浅蓝色
     .debug_color = "rgb(0,255,0)",  // 青色
     .info_color = "\x1b[32m",   // 绿色 (ANSI)
     .warn_color = "#FFD700",    // 金色
     .error_color = "rgb(255,0,0)",  // 红色
     .fatal_color = "\x1b[35m"   // 紫色 (ANSI)
 };
 // 创建控制台Sink
 SinkConfig console_config = {
     .min_level = LOG_DEBUG,
     .use_color = true,
     .color_config = &myColorConfig,
     .format = NULL,  // 使用全局格式
 };
```

### 7. 定义输出格式

可以给整个Logger添加输出格式，也可以给每个Sink添加输出格式

```c
    // 创建日志格式
    LogFormatElement elements[] = {
        {LOG_FMT_TIME, NULL, "[%Y-%m-%d %H:%M:%S]"},
        {LOG_FMT_TEXT, " [", NULL},
        {LOG_FMT_LOGGER_NAME, NULL, NULL},
        {LOG_FMT_TEXT, "] [", NULL},
        {LOG_FMT_LEVEL, NULL, NULL},
        {LOG_FMT_TEXT, "] ", NULL},
        {LOG_FMT_MESSAGE, NULL, NULL}
    };
    LogFormat* format = log_format_create(elements, sizeof(elements) / sizeof(elements[0]));

    // 创建日志配置
    LogConfig config = {
        .logger_name = "MyLogger",
        .async = true,
        .format = format
    };
```

### 8. 最后使用完成之后需要销毁Logger

```c
// 销毁Logger
logger_destroy(logger);
```

### 9. 使用便捷宏

```c
// 便捷宏
#define LOG_TRACE(logger, ...) logger_log(logger, LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(logger, ...) logger_log(logger, LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(logger, ...) logger_log(logger, LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(logger, ...) logger_log(logger, LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(logger, ...) logger_log(logger, LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(logger, ...) logger_log(logger, LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
```
