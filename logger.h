#ifndef LOGGER_H
#define LOGGER_H

void * logger_run(void *arg);

/*
 * Submit a message to be written to the log file.
 * message must be a valid pointer to a null-terminated C string.
 * If the message queue is full this function will block until the message can be submitted.
 */
void logger_log_message(const char *message);

#endif /* LOGGER_H */
