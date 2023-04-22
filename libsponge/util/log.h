#include <syslog.h>

#define LOG_EMERG   0 // system is unusable
#define LOG_ALERT   1 // action must be taken immediately
#define LOG_CRIT    2 // critical conditions
#define LOG_ERR     3 // error conditions
#define LOG_WARNING 4 // warning conditions
#define LOG_NOTICE  5 // normal, but significant, condition
#define LOG_INFO    6 // informational message
#define LOG_DEBUG   7 // debug-level message

#define sponge_log(level, ...) \
    do{ \
        openlog("sponge", LOG_CONS | LOG_PID, LOG_LOCAL0); \
        syslog(level, __VA_ARGS__); \
        closelog(); \
    }while (0);
