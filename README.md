# CPU usage tracker

Terminal program that displays the average and per-core CPU usage.
Written using C99 and POSIX threads.

## Building

```bash
bash build.sh
```

## Running

```bash
./cut
```

## Special build options

Compile with debug symbols:

```bash
bash build.sh --debug
```

Use Clang instead of GCC:

```bash
CC=clang bash build.sh
```

Build and run tests:

```bash
bash build.sh --tests # optionally also add --valgrind to use Valgrind when running tests
```

## Architecture

The program uses five threads.

- Reader: Parses the /proc/stat file and sends the data to the Analyzer thread.
- Analyzer: Uses the parsed data to calculate CPU usage and sends the results to the Printer thread.
- Printer: Displays the results in the terminal.
- Logger: Can receive a message from any other thread and save it to a log file.
- Watchdog: Keeps a list of watched threads and if a thread doesn't report activity for more than 2 seconds cancels all watched threads and exits. Also handles the SIGTERM signal to allow for exit with cleanup.
