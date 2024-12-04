#include "log.h"
#include "log_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// 路径处理工具
#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

// 规范化路径分隔符
static void normalize_path(char* path) {
    char* p;
    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = PATH_SEPARATOR;
        }
    }
}

// 日志级别名称和颜色
static const char* level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

// 默认颜色配置
static const char* default_colors[] = {
    "\x1b[94m",  // TRACE: 亮蓝色
    "\x1b[36m",  // DEBUG: 青色
    "\x1b[32m",  // INFO:  绿色
    "\x1b[33m",  // WARN:  黄色
    "\x1b[31m",  // ERROR: 红色
    "\x1b[35m"   // FATAL: 紫色
};
// 预定义的格式模板
static const LogFormatElement DEFAULT_LOG_FORMAT[] = {
    {LOG_FMT_TIME, NULL, "[%Y-%m-%d %H:%M:%S]"},
    {LOG_FMT_TEXT, " [", NULL},
    {LOG_FMT_LOGGER_NAME, NULL, NULL},
    {LOG_FMT_TEXT, "] [", NULL},
    {LOG_FMT_LEVEL, NULL, NULL},
    {LOG_FMT_TEXT, "] ", NULL},
    {LOG_FMT_MESSAGE, NULL, NULL}
};

static const int DEFAULT_LOG_FORMAT_COUNT = sizeof(DEFAULT_LOG_FORMAT) / sizeof(DEFAULT_LOG_FORMAT[0]);
// 颜色格式转换工具
typedef struct {
    unsigned char r, g, b, a;
} Color;

// 将十六进制字符转换为数字
static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

// 解析HTML颜色代码 (#RGB, #RGBA, #RRGGBB, #RRGGBBAA)
static Color parse_html_color(const char* color) {
    Color result = { 0, 0, 0, 255 };  // 默认不透明
    if (!color || color[0] != '#') return result;

    size_t len = strlen(color);
    if (len == 4) {  // #RGB
        result.r = hex_to_int(color[1]) * 17;  // 扩展为两位
        result.g = hex_to_int(color[2]) * 17;
        result.b = hex_to_int(color[3]) * 17;
    }
    else if (len == 5) {  // #RGBA
        result.r = hex_to_int(color[1]) * 17;
        result.g = hex_to_int(color[2]) * 17;
        result.b = hex_to_int(color[3]) * 17;
        result.a = hex_to_int(color[4]) * 17;
    }
    else if (len == 7) {  // #RRGGBB
        result.r = (hex_to_int(color[1]) << 4) + hex_to_int(color[2]);
        result.g = (hex_to_int(color[3]) << 4) + hex_to_int(color[4]);
        result.b = (hex_to_int(color[5]) << 4) + hex_to_int(color[6]);
    }
    else if (len == 9) {  // #RRGGBBAA
        result.r = (hex_to_int(color[1]) << 4) + hex_to_int(color[2]);
        result.g = (hex_to_int(color[3]) << 4) + hex_to_int(color[4]);
        result.b = (hex_to_int(color[5]) << 4) + hex_to_int(color[6]);
        result.a = (hex_to_int(color[7]) << 4) + hex_to_int(color[8]);
    }
    return result;
}

// 解析RGB/RGBA格式 (rgb(r,g,b) 或 rgba(r,g,b,a))
static Color parse_rgb_color(const char* color) {
    Color result = { 0, 0, 0, 255 };
    if (!color) return result;

    int r = 0, g = 0, b = 0;
    float a = 1.0f;

    if (strncmp(color, "rgba(", 5) == 0) {
        sscanf(color, "rgba(%d,%d,%d,%f)", &r, &g, &b, &a);
        result.a = (unsigned char)(a * 255);
    }
    else if (strncmp(color, "rgb(", 4) == 0) {
        sscanf(color, "rgb(%d,%d,%d)", &r, &g, &b);
    }

    result.r = (unsigned char)r;
    result.g = (unsigned char)g;
    result.b = (unsigned char)b;
    return result;
}

// 将颜色转换为ANSI转义序列
static char* color_to_ansi(const char* color_str, char* buffer, size_t buffer_size) {
    if (!color_str || !buffer) return NULL;

    // 如果已经是ANSI转义序列，直接返回
    if (strncmp(color_str, "\x1b[", 2) == 0) {
        strncpy(buffer, color_str, buffer_size);
        return buffer;
    }

    Color color;
    if (color_str[0] == '#') {
        color = parse_html_color(color_str);
    }
    else if (strncmp(color_str, "rgb", 3) == 0) {
        color = parse_rgb_color(color_str);
    }
    else {
        // 不支持的格式，返回默认颜色
        strncpy(buffer, "\x1b[0m", buffer_size);
        return buffer;
    }

    // 生成24位真彩色ANSI转义序列
    snprintf(buffer, buffer_size, "\x1b[38;2;%d;%d;%dm",
        color.r, color.g, color.b);
    return buffer;
}

// 获取日志级别对应的颜色
static const char* get_level_color(LogLevel level, const SinkConfig* config) {
    static char color_buffer[32];  // 用于存储转换后的ANSI颜色代码

    if (!config->use_color) {
        return "";
    }

    if (config->color_config) {
        const char* color = NULL;
        switch (level) {
        case LOG_TRACE: color = config->color_config->trace_color; break;
        case LOG_DEBUG: color = config->color_config->debug_color; break;
        case LOG_INFO:  color = config->color_config->info_color;  break;
        case LOG_WARN:  color = config->color_config->warn_color;  break;
        case LOG_ERROR: color = config->color_config->error_color; break;
        case LOG_FATAL: color = config->color_config->fatal_color; break;
        default: return "";
        }

        if (color) {
            return color_to_ansi(color, color_buffer, sizeof(color_buffer));
        }
    }

    return default_colors[level];
}

// 滚动文件Sink相关结构
typedef struct {
    FILE* file;
    char* base_filename;
    RollConfig roll_config;
    time_t last_roll_time;
    size_t current_size;
    char* current_filename;
    int current_index;  // 当前文件索引
} RollingFileData;


// 文件Sink相关结构
typedef struct {
    FILE* file;
    LogSink* sink;
} FileData;
// Sink销毁函数
static void console_sink_destroy(LogSink* sink) {
    free(sink);
}

static void file_sink_destroy(LogSink* sink) {
    if (sink->user_data) {
        FileData* data = (FileData*)sink->user_data;
        if (data->file)
        {
            fclose(data->file);
        }
        free(data);
    }

    free(sink);
}

static void rolling_sink_destroy(LogSink* sink) {
    if (sink->user_data) {
        RollingFileData* data = (RollingFileData*)sink->user_data;
        if (data->file) {
            fclose(data->file);
        }
        free(data->base_filename);
        free(data->current_filename);
        free(data);
    }
    free(sink);
}

// 格式化日志消息的函数
static void format_log_message(const Logger* logger, const LogMessage* msg,
    const LogFormat* format, char* buffer, size_t buffer_size) {
    char* pos = buffer;
    size_t remaining = buffer_size;

    for (int i = 0; i < format->element_count && remaining > 1; i++) {
        const LogFormatElement* elem = &format->elements[i];
        int written = 0;
        switch (elem->type) {
        case LOG_FMT_TIME: {
            char time_buf[64];
            struct tm* tm_info = localtime(&msg->time);
            strftime(time_buf, sizeof(time_buf),
                elem->time_fmt ? elem->time_fmt : "%Y-%m-%d %H:%M:%S",
                tm_info);
            written = snprintf(pos, remaining, "%s", time_buf);
            break;
        }
        case LOG_FMT_LEVEL:
            written = snprintf(pos, remaining, "%s", level_names[msg->level]);
            break;
        case LOG_FMT_LOGGER_NAME:
            written = snprintf(pos, remaining, "%s", logger->name);
            break;
        case LOG_FMT_FILE:
            written = snprintf(pos, remaining, "%s", msg->file);
            break;
        case LOG_FMT_LINE:
            written = snprintf(pos, remaining, "%d", msg->line);
            break;
        case LOG_FMT_MESSAGE:
            written = snprintf(pos, remaining, "%s", msg->message);
            break;
        case LOG_FMT_TEXT:
            if (elem->text) {
                written = snprintf(pos, remaining, "%s", elem->text);
            }
            break;
        }

        if (written > 0 && written < remaining) {
            pos += written;
            remaining -= written;
        }
    }
    *pos = '\0';  // 确保字符串正确终止
}

// 控制台Sink的写入函数
static void console_write(LogMessage* msg, void* user_data) {
    LogSink* sink = (LogSink*)user_data;
    char buffer[8192];  // 足够大的缓冲区
    // 获取Logger实例
    Logger* logger = sink->logger;

    // 使用sink的格式或全局格式
    const LogFormat* format = sink->config.format ? sink->config.format : logger->format;

    // 如果没有指定格式，使用默认格式
    if (!format) {
        format = log_format_create(DEFAULT_LOG_FORMAT, DEFAULT_LOG_FORMAT_COUNT);
    }

    format_log_message(logger, msg, format, buffer, sizeof(buffer));

    const char* color = get_level_color(msg->level, &sink->config);
    const char* reset = sink->config.use_color ? "\x1b[0m" : "";

    fprintf(stdout, "%s%s%s\n", color, buffer, reset);
    fflush(stdout);
}

// 文件Sink的写入函数
static void file_write(LogMessage* msg, void* user_data) {
    FileData* data = (FileData*)user_data;
    LogSink* sink = data->sink;
    FILE* file = data->file;
    char buffer[8192];
    Logger* logger = sink->logger;
    const LogFormat* format = sink->config.format ? sink->config.format : logger->format;

    if (!format) {
        format = log_format_create(DEFAULT_LOG_FORMAT, DEFAULT_LOG_FORMAT_COUNT);
    }

    format_log_message(logger, msg, format, buffer, sizeof(buffer));
    fprintf(file, "%s\n", buffer);
    fflush(file);
}

// 连接路径
static void path_join(char* dest, size_t dest_size, const char* path1, const char* path2) {
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);

    strncpy(dest, path1, dest_size);

    // 确保路径1末尾有分隔符
    if (len1 > 0 && path1[len1 - 1] != PATH_SEPARATOR && path1[len1 - 1] != '/') {
        strncat(dest, PATH_SEPARATOR_STR, dest_size - len1);
        len1++;
    }

    // 添加路径2，跳过开头的分隔符
    if (len2 > 0 && (path2[0] == PATH_SEPARATOR || path2[0] == '/')) {
        strncat(dest, path2 + 1, dest_size - len1);
    }
    else {
        strncat(dest, path2, dest_size - len1);
    }

    normalize_path(dest);
}

// 获取目录部分
static void get_directory(const char* path, char* dir, size_t dir_size) {
    strncpy(dir, path, dir_size);
    normalize_path(dir);

    char* last_sep = strrchr(dir, PATH_SEPARATOR);
    if (last_sep) {
        *(last_sep + 1) = '\0';
    }
    else {
        dir[0] = '\0';
    }
}
// 获取文件名部分
static void get_filename(const char* path, char* filename, size_t filename_size) {
    char normalize_path[1024];
    strncpy(normalize_path, path, sizeof(normalize_path));
    const char* last_sep = strrchr(normalize_path, PATH_SEPARATOR);
    if (last_sep) {
        strncpy(filename, last_sep + 1, filename_size);
    }
    else {
        strncpy(filename, path, filename_size);
    }
}

// 分离文件路劲和扩展名
static void split_filename(const char* filename, char* name, size_t name_size, char* ext, size_t ext_size) {
    const char* dot = strrchr(filename, '.');
    if (dot) {
        size_t base_len = dot - filename;
        strncpy(name, filename, base_len < name_size ? base_len : name_size);
        name[base_len] = '\0';
        strncpy(ext, dot + 1, ext_size);
    }
    else {
        strncpy(name, filename, name_size);
        ext[0] = '\0';
    }
}

// 获取文件大小
static size_t get_file_size(FILE* file) {
    long current_pos = ftell(file);
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, current_pos, SEEK_SET);
    return size;
}

// 生成新的文件名
static void generate_filename(const char* base_name, const char* pattern,
    char* buffer, size_t buffer_size, time_t time, int index) {
    char dir[256] = { 0 };
    char filename[256] = { 0 };
    char name[256] = { 0 };
    char ext[32] = { 0 };
    char time_str[128] = { 0 };
    char final_pattern[256] = { 0 };

    // 分离路径组件
    get_directory(base_name, dir, sizeof(dir));
    get_filename(base_name, filename, sizeof(filename));
    split_filename(filename, name, sizeof(name), ext, sizeof(ext));

    // 处理时间格式
    struct tm* tm_info = localtime(&time);
    if (tm_info) {
        // 使用默认的时间格式，而不是用户的pattern
        strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", tm_info);
    }
    else {
        // 如果获取时间失败，使用时间戳
        snprintf(time_str, sizeof(time_str), "%ld", (long)time);
    }

    // 替换模式中的特殊标记
    char* pattern_copy = strdup(pattern);
    char* result = final_pattern;
    char* p = pattern_copy;
    size_t remaining = sizeof(final_pattern);
    int written = 0;

    while (*p && remaining > 1) {  // 保留1字节给null终止符
        if (*p == '%' && *(p + 1)) {
            p++;
            switch (*p) {
            case 't':
                written = snprintf(result, remaining, "%s", time_str);
                break;
            case 'i':
                written = snprintf(result, remaining, "%d", index);
                break;
            case 'f':
                written = snprintf(result, remaining, "%s", name);
                break;
            case 'e':
                written = snprintf(result, remaining, "%s", ext);
                break;
            default:
                written = snprintf(result, remaining, "%%%c", *p);
                break;
            }

            if (written >= 0 && written < remaining) {
                result += written;
                remaining -= written;
            }
            else {
                // 缓冲区空间不足
                break;
            }
        }
        else {
            *result++ = *p;
            remaining--;
        }
        p++;
    }
    *result = '\0';
    free(pattern_copy);

    // 构建最终文件名
    if (dir[0]) {
        path_join(buffer, buffer_size, dir, final_pattern);
    }
    else {
        strncpy(buffer, final_pattern, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
    normalize_path(buffer);

    // 确保生成的文件名不为空
    if (buffer[0] == '\0') {
        snprintf(buffer, buffer_size, "%s%s%d.%s",
            dir[0] ? dir : "", name, index, ext[0] ? ext : "log");
    }
}

// 执行文件滚动
static void roll_file(RollingFileData* data) {
    if (data->file) {
        fclose(data->file);
    }

    // 查找下一个可用的文件索引
    int next_index;
    if (data->current_index >= data->roll_config.max_files - 1) {
        next_index = 1;
    }
    else {
        next_index = data->current_index + 1;
    }


    // 构建完整的文件路径
    char full_path[2048];
    generate_filename(data->base_filename,
        data->roll_config.filename_pattern,
        full_path, sizeof(full_path),
        time(NULL), next_index);

    normalize_path(full_path);

    // 打开新文件
    data->file = fopen(full_path, "w");
    if (data->file) {
        data->current_size = 0;
        data->last_roll_time = time(NULL);
        data->current_index = next_index;
        if (data->current_filename) {
            free(data->current_filename);
        }
        data->current_filename = strdup(full_path);
    }
}

// 检查是否需要滚动
static bool should_roll(RollingFileData* data) {
    if (data->roll_config.strategy == ROLL_BY_SIZE ||
        data->roll_config.strategy == ROLL_BY_BOTH) {
        if (data->current_size >= data->roll_config.max_file_size) {
            return true;
        }
    }

    if (data->roll_config.strategy == ROLL_BY_TIME ||
        data->roll_config.strategy == ROLL_BY_BOTH) {
        time_t current_time = time(NULL);
        struct tm* current_tm = localtime(&current_time);
        struct tm* last_tm = localtime(&data->last_roll_time);

        bool should_roll_time = false;
        switch (data->roll_config.time_unit) {
        case TIME_UNIT_MINUTE:
            should_roll_time = (current_time - data->last_roll_time) >=
                (data->roll_config.time_interval * 60);
            break;
        case TIME_UNIT_HOUR:
            should_roll_time = (current_time - data->last_roll_time) >=
                (data->roll_config.time_interval * 3600);
            break;
        case TIME_UNIT_DAY:
            should_roll_time = (current_time - data->last_roll_time) >=
                (data->roll_config.time_interval * 24 * 3600);
            break;
        case TIME_UNIT_WEEK:
            should_roll_time = (current_time - data->last_roll_time) >=
                (data->roll_config.time_interval * 7 * 24 * 3600);
            break;
        case TIME_UNIT_MONTH: {
            int month_diff = (current_tm->tm_year - last_tm->tm_year) * 12 +
                (current_tm->tm_mon - last_tm->tm_mon);
            should_roll_time = month_diff >= data->roll_config.time_interval;
            break;
        }
        }

        if (should_roll_time) {
            return true;
        }
    }

    return false;
}

// 滚动文件Sink的写入函数
static void rolling_file_write(LogMessage* msg, void* user_data) {
    RollingFileData* data = (RollingFileData*)user_data;

    if (should_roll(data)) {
        roll_file(data);
    }

    if (data->file) {
        char time_buf[32];
        struct tm* tm_info = localtime(&msg->time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

        int written = fprintf(data->file, "%s [%s] %s:%d: %s\n",
            time_buf, level_names[msg->level],
            msg->file, msg->line, msg->message);
        fflush(data->file);

        data->current_size += written;
    }
}

// 创建滚动文件Sink
LogSink* rolling_file_sink_create(const char* filename,  SinkConfig* config,
     RollConfig* roll_config) {
    RollingFileData* data = (RollingFileData*)malloc(sizeof(RollingFileData));
    if (!data) return NULL;

    memset(data, 0, sizeof(RollingFileData));
    data->base_filename = strdup(filename);
    data->roll_config = *roll_config;
    data->last_roll_time = time(NULL);
    data->current_size = 0;
    data->current_filename = NULL;
    data->current_index = 0;  // 从0开始，第一次滚动时会变成1

    // 打开初始文件
    data->file = fopen(filename, "a");
    if (!data->file) {
        free(data->base_filename);
        free(data);
        return NULL;
    }

    data->current_size = get_file_size(data->file);
    data->current_filename = strdup(filename);

    LogSink* sink = (LogSink*)malloc(sizeof(LogSink));
    if (!sink) {
        fclose(data->file);
        free(data->base_filename);
        free(data->current_filename);
        free(data);
        return NULL;
    }

    sink->config = *config;
    sink->config.write_fn = rolling_file_write;
    sink->user_data = data;
    sink->destroy = rolling_sink_destroy;

    return sink;
}

// 创建控制台Sink
LogSink* console_sink_create( SinkConfig* config) {
    LogSink* sink = (LogSink*)malloc(sizeof(LogSink));
    if (!sink) return NULL;
    sink->config = *config;
    sink->config.write_fn = console_write;
    sink->user_data = sink;  // 传递sink本身作为user_data，以访问配置
    sink->destroy = console_sink_destroy;
    
    return sink;
}

// 创建文件Sink
LogSink* file_sink_create(const char* filename,  SinkConfig* config) {
    FILE* file = fopen(filename, "a");
    if (!file) return NULL;
    LogSink* sink = (LogSink*)malloc(sizeof(LogSink));
    if (!sink) {
        fclose(file);
        return NULL;
    }
    FileData* data = (FileData*)malloc(sizeof(FileData));
    data->sink = sink;
    data->file = file;
    sink->config = *config;
    sink->config.write_fn = file_write;
    sink->user_data = data;
    sink->destroy = file_sink_destroy;
    return sink;
}


// 创建格式配置
LogFormat* log_format_create(const LogFormatElement* elements, int count) {
    LogFormat* format = (LogFormat*)malloc(sizeof(LogFormat));
    if (!format) return NULL;

    format->elements = (LogFormatElement*)malloc(sizeof(LogFormatElement) * count);
    if (!format->elements) {
        free(format);
        return NULL;
    }

    format->element_count = count;
    for (int i = 0; i < count; i++) {
        format->elements[i] = elements[i];
        if (elements[i].text) {
            format->elements[i].text = strdup(elements[i].text);
        }
        if (elements[i].time_fmt) {
            format->elements[i].time_fmt = strdup(elements[i].time_fmt);
        }
    }

    return format;
}

