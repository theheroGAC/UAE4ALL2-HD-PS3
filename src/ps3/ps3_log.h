#ifndef PS3_LOG_H
#define PS3_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

void ps3_write_log(const char *fmt, ...);
void ps3_log_close(void);

#ifdef __cplusplus
}
#endif

#endif
