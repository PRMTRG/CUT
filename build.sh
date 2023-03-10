#!/bin/bash
set -eu

gcc_flags="-Wall -Wextra -std=c99 -pedantic"
clang_flags="-Weverything -Wno-declaration-after-statement -Wno-vla -Wno-extra-semi-stmt -Wno-missing-noreturn -Wno-padded -Wno-disabled-macro-expansion -std=c99 -pedantic"
debug_flags="-g"
op_flags="-O2"
common_flags="-D_POSIX_C_SOURCE=1 -pthread"
linker_flags="-lm -lrt"

source_files=(
    "proc_stat_utils.c"
    "reader.c"
    "analyzer.c"
    "printer.c"
    "thread_utils.c"
    "logger.c"
    "watchdog.c"
)

debug=false
tests=false
print=false
valgrind=false # use Valgrind when running tests

for arg in "$@"; do
    case "$arg" in
        "--debug")
            debug=true
        ;;
        "--tests")
            tests=true
        ;;
        "--print")
            print=true
        ;;
        "--valgrind")
            valgrind=true
            debug=true
        ;;
        *)
            echo "Unrecognized option: $arg"
            exit 1
        ;;
    esac
done

if [ -n "${CC:-}" ]; then
    cc="$CC"
else
    cc='gcc'
fi

case "$cc" in
    "gcc")
        comp_cmd="$cc $gcc_flags"
    ;;
    "clang")
        comp_cmd="$cc $clang_flags"
    ;;
    *)
        comp_cmd="$cc"
    ;;
esac

comp_cmd+=" $common_flags"

if [ "$debug" == "true" ]; then
    comp_cmd+=" $debug_flags"
else
    comp_cmd+=" $op_flags"
fi

if [ "$print" == "true" ]; then
    print_cmd="echo"
else
    print_cmd=
fi

$print_cmd $comp_cmd -o cut main.c "${source_files[@]}" $linker_flags

if [ "$tests" == "true" ]; then
    $print_cmd $comp_cmd -Wno-unused-function -Wno-unused-variable -Werror -o tests tests.c "${source_files[@]}" $linker_flags

    if [ "$valgrind" == "true" ]; then
        $print_cmd valgrind --leak-check=yes ./tests
    else
        $print_cmd ./tests
    fi
fi
