/*
 * tinylog
 * Copyright (C) 2018-2019 Ruilin Peng (Nick) <pymumu@gmail.com>
 * https://github.com/pymumu/tinylog
 */

#ifndef TLOG_H
#define TLOG_H
#include <stdarg.h>

#ifdef __cplusplus
#include <string>
#include <memory>
#include <sstream>
#include <iostream>
#include <functional>
extern "C" {
#endif /*__cplusplus */

typedef enum {
    TLOG_DEBUG = 0,
    TLOG_INFO = 1,
    TLOG_NOTICE = 2,
    TLOG_WARN = 3,
    TLOG_ERROR = 4,
    TLOG_FATAL = 5,
    TLOG_END = 6
} tlog_level;

struct tlog_time {
    int year;
    unsigned int usec;
    unsigned char mon;
    unsigned char mday;
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
} __attribute__((packed));

#ifndef TLOG_MAX_LINE_LEN
#define TLOG_MAX_LINE_LEN (1024)
#endif

/* TLOG FLAGS LIST */
/* set tlog not compress file when archive */
#define TLOG_NOCOMPRESS (1 << 0)

/* Set the segmentation mode to process the log, Used by the callback function to return a full log*/
#define TLOG_SEGMENT (1 << 1)

/*
 multiwrite: enable multi process write mode.
            NOTICE: maxlogsize in all prcesses must be same when enable this mode.
 */
#define TLOG_MULTI_WRITE (1 << 2)

/* Not Block if buffer is insufficient. */
#define TLOG_NONBLOCK (1 << 3)

/* enable log to screen */
#define TLOG_SCREEN (1 << 4)

struct tlog_loginfo {
    tlog_level level;
    const char *file;
    const char *func;
    int line;
    struct tlog_time time;
} __attribute__((packed));

/*
Function: Print log
level: Current log Levels
format: Log formats
*/
#ifndef BASE_FILE_NAME
#define BASE_FILE_NAME __FILE__
#endif
#define tlog(level, format, ...) tlog_ext(level, BASE_FILE_NAME, __LINE__, __func__, NULL, format, ##__VA_ARGS__)

#ifdef NEED_PRINTF
extern int tlog_ext(tlog_level level, const char *file, int line, const char *func, void *userptr, const char *format, ...)
    __attribute__((format(printf, 6, 7))) __attribute__((nonnull (6)));
extern int tlog_vext(tlog_level level, const char *file, int line, const char *func, void *userptr, const char *format, va_list ap);

/* write buff to log file */
extern int tlog_write_log(char *buff, int bufflen);

/* set log level */
extern int tlog_setlevel(tlog_level level);

/* get log level */
extern tlog_level tlog_getlevel(void);

/* enalbe log to screen */
extern void tlog_setlogscreen(int enable);

/* enalbe early log to screen */
extern void tlog_set_early_printf(int enable);

/* Get log level in string */
extern const char *tlog_get_level_string(tlog_level level);

/*
Function: Initialize log module
logfile: log file.
maxlogsize: The maximum size of a single log file.
maxlogcount: Number of archived logs.
buffsize: Buffer size, zero for default (128K)
flag: read tlog flags
 */
extern int tlog_init(const char *logfile, int maxlogsize, int maxlogcount, int buffsize, unsigned int flag);

/* flush pending log message, and exit tlog */
extern void tlog_exit(void);
/*
customize log output format
steps:
1. define format function, function must be defined as tlog_format_func, use snprintf or vsnprintf format log to buffer
2. call tlog_reg_format_func to register format function.

read _tlog_format for example.
*/
typedef int (*tlog_format_func)(char *buff, int maxlen, struct tlog_loginfo *info, void *userptr, const char *format, va_list ap);
extern int tlog_reg_format_func(tlog_format_func func);

/* register log output callback 
 Note: info is invalid when flag TLOG_SEGMENT is not set.
 */
typedef int (*tlog_log_output_func)(struct tlog_loginfo *info, const char *buff, int bufflen, void *private_data);
extern int tlog_reg_log_output_func(tlog_log_output_func output, void *private_data);

struct tlog_log;
typedef struct tlog_log tlog_log;
/*
Function: open a new log stream, handler should close by tlog_close
logfile: log file.
maxlogsize: The maximum size of a single log file.
maxlogcount: Number of archived logs.
buffsize: Buffer size, zero for default (128K)
flag: read tlog flags
return: log stream handler.
*/
extern tlog_log *tlog_open(const char *logfile, int maxlogsize, int maxlogcount, int buffsize, unsigned int flag);

/* write buff to log file */
extern int tlog_write(struct tlog_log *log, const char *buff, int bufflen);

/* close log stream */
extern void tlog_close(tlog_log *log);

/*
Function: Print log to log stream
log: log stream
format: Log formats
*/
extern int tlog_printf(tlog_log *log, const char *format, ...) __attribute__((format(printf, 2, 3))) __attribute__((nonnull (1, 2)));

/*
Function: Print log to log stream with ap
log: log stream
format: Log formats
va_list: args list
*/
extern int tlog_vprintf(tlog_log *log, const char *format, va_list ap);

/* enalbe log to screen */
extern void tlog_logscreen(tlog_log *log, int enable);

/* register output callback */
typedef int (*tlog_output_func)(struct tlog_log *log, const char *buff, int bufflen);
extern int tlog_reg_output_func(tlog_log *log, tlog_output_func output);

/* set private data */
extern void tlog_set_private(tlog_log *log, void *private_data);

/* get private data */
extern void *tlog_get_private(tlog_log *log);

/* get local time */
extern int tlog_localtime(struct tlog_time *tm);

#else

static inline int tlog_ext(tlog_level level, const char *file, int line, const char *func, void *userptr, const char *format, ...) {return 0;}
static inline int tlog_vext(tlog_level level, const char *file, int line, const char *func, void *userptr, const char *format, va_list ap) {return 0;}

/* write buff to log file */
extern int tlog_write_log(char *buff, int bufflen);

/* set log level */
static inline int tlog_setlevel(tlog_level level) {return 0;}

/* enalbe log to screen */
static inline void tlog_setlogscreen(int enable) {}

/* enalbe early log to screen */
extern void tlog_set_early_printf(int enable);

/* Get log level in string */
extern const char *tlog_get_level_string(tlog_level level);

/*
Function: Initialize log module
logfile: log file.
maxlogsize: The maximum size of a single log file.
maxlogcount: Number of archived logs.
buffsize: Buffer size, zero for default (128K)
flag: read tlog flags
 */
static inline int tlog_init(const char *logfile, int maxlogsize, int maxlogcount, int buffsize, unsigned int flag) {}

/* flush pending log message, and exit tlog */
static inline void tlog_exit(void) {}
/*
customize log output format
steps:
1. define format function, function must be defined as tlog_format_func, use snprintf or vsnprintf format log to buffer
2. call tlog_reg_format_func to register format function.

read _tlog_format for example.
*/
typedef int (*tlog_format_func)(char *buff, int maxlen, struct tlog_loginfo *info, void *userptr, const char *format, va_list ap);
extern int tlog_reg_format_func(tlog_format_func func);

/* register log output callback 
 Note: info is invalid when flag TLOG_SEGMENT is not set.
 */
typedef int (*tlog_log_output_func)(struct tlog_loginfo *info, const char *buff, int bufflen, void *private_data);
extern int tlog_reg_log_output_func(tlog_log_output_func output, void *private_data);

struct tlog_log;
typedef struct tlog_log tlog_log;
/*
Function: open a new log stream, handler should close by tlog_close
logfile: log file.
maxlogsize: The maximum size of a single log file.
maxlogcount: Number of archived logs.
buffsize: Buffer size, zero for default (128K)
flag: read tlog flags
return: log stream handler.
 */
static inline tlog_log *tlog_open(const char *logfile, int maxlogsize, int maxlogcount, int buffsize, unsigned int flag) { return 0;}

/* write buff to log file */
extern int tlog_write(struct tlog_log *log, const char *buff, int bufflen);

/* close log stream */
extern void tlog_close(tlog_log *log);

/*
Function: Print log to log stream
log: log stream
format: Log formats
*/
static inline int tlog_printf(tlog_log *log, const char *format, ...) {return 0;}

/*
Function: Print log to log stream with ap
log: log stream
format: Log formats
va_list: args list
*/
extern int tlog_vprintf(tlog_log *log, const char *format, va_list ap);

/* enalbe log to screen */
extern void tlog_logscreen(tlog_log *log, int enable);

/* register output callback */
typedef int (*tlog_output_func)(struct tlog_log *log, const char *buff, int bufflen);
extern int tlog_reg_output_func(tlog_log *log, tlog_output_func output);

/* set private data */
extern void tlog_set_private(tlog_log *log, void *private_data);

/* get private data */
extern void *tlog_get_private(tlog_log *log);

/* get local time */
static inline int tlog_localtime(struct tlog_time *tm) {return 0;}


#endif

#ifdef __cplusplus
class Tlog {
    using Stream = std::ostringstream;
    using Buffer = std::unique_ptr<Stream, std::function<void(Stream*)>>;
public:
    Tlog(){}
    ~Tlog(){}

    static Tlog &Instance() {
        static Tlog logger;
        return logger;
    }

    Buffer LogStream(tlog_level level, const char *file, int line, const char *func, void *userptr) {
        return Buffer(new Stream, [=](Stream *st) {
            tlog_ext(level, file, line, func, userptr, "%s", st->str().c_str());
        });
    }
};

class TlogOut {
    using Stream = std::ostringstream;
    using Buffer = std::unique_ptr<Stream, std::function<void(Stream*)>>;
public:
    TlogOut(){}
    ~TlogOut(){}

    static TlogOut &Instance() {
        static TlogOut logger;
        return logger;
    }

    Buffer Out(tlog_log *log) {
        return Buffer(new Stream, [=](Stream *st) {
            tlog_printf(log, "%s", st->str().c_str());
        });
    }
};

#define Tlog_logger (Tlog::Instance())
#define Tlog_stream(level) if (tlog_getlevel() <= level) *Tlog_logger.LogStream(level, BASE_FILE_NAME, __LINE__, __func__, NULL) 
#define tlog_debug Tlog_stream(TLOG_DEBUG)
#define tlog_info Tlog_stream(TLOG_INFO)
#define tlog_notice Tlog_stream(TLOG_NOTICE)
#define tlog_warn Tlog_stream(TLOG_WARN)
#define tlog_error Tlog_stream(TLOG_ERROR)
#define tlog_fatal Tlog_stream(TLOG_FATAL)

#define Tlog_out_logger (TlogOut::Instance())
#define tlog_out(stream)  (*Tlog_out_logger.Out(stream))

} /*__cplusplus */
#else
#define tlog_debug(...) tlog(TLOG_DEBUG, ##__VA_ARGS__)
#define tlog_info(...) tlog(TLOG_INFO, ##__VA_ARGS__)
#define tlog_notice(...) tlog(TLOG_NOTICE, ##__VA_ARGS__)
#define tlog_warn(...) tlog(TLOG_WARN, ##__VA_ARGS__)
#define tlog_error(...) tlog(TLOG_ERROR, ##__VA_ARGS__)
#define tlog_fatal(...) tlog(TLOG_FATAL, ##__VA_ARGS__)
#endif 
#endif // !TLOG_H
