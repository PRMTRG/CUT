#ifndef READER_H
#define READER_H

typedef struct {
    int max_cpu_entries;
    bool use_watchdog;
} ReaderArgs;

void * reader_run(void *arg);

#endif /* READER_H */
