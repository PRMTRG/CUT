#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

void * logger_run(void *arg);

/*
 * Submit a message to be written to the log file.
 * message must be a valid pointer to a null-terminated C string.
 * A newline character is appended to the logged message.
 * If dont_block_on_fail is false and the logger queue is full or
 * the logger thread isn't running this function will block until
 * the message can be submitted.
 */
void logger_log_message(bool dont_block_on_fail, const char *message);

/*
 * Log formatted message and the calling function name.
 * This macro can only handle messages up to 511 characters in total length.
 */
#define elog(dont_block_on_fail, ...) do {                                    \
    char _msg[512];                                                           \
    size_t _len = 0;                                                          \
    int _ret;                                                                 \
    _ret = snprintf(_msg, sizeof(_msg) - _len, "%s: ", __func__);             \
    if (_ret < 0) { abort(); }                                                \
    _len += (size_t)_ret;                                                     \
    if (_len >= sizeof(_msg)) { abort(); }                                    \
    _ret = snprintf(&_msg[_len], sizeof(_msg) - _len, __VA_ARGS__);           \
    if (_ret < 0) { abort(); }                                                \
    _len += (size_t)_ret;                                                     \
    if (_len >= sizeof(_msg)) { abort(); }                                    \
    logger_log_message(dont_block_on_fail, _msg);                             \
} while (0)

#endif /* LOGGER_H */
