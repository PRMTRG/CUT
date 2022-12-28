#ifndef WATCHDOG_H
#define WATCHDOG_H

void * watchdog_run(void *arg);

/*
 * Signal that the thread is still active.
 * If a watched thread doesn't report activity for more than 2 seconds
 * the program will exit.
 * Using this function automatically adds the thread to the list of watched threads.
 *
 * name must be a valid C string containing the name of the thread that
 * will be logged if the thread hangs.
 */
void watchdog_signal_active(const char *name);

#endif /* WATCHDOG_H */
