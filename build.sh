#!/bin/bash
set -eu

gcc_flags="-Wall -Wextra -std=c99 -pedantic"
clang_flags="-Weverything -Wno-declaration-after-statement -Wno-vla -Wno-extra-semi-stmt -std=c99 -pedantic"
debug_flags="-g"
op_flags="-O2"
common_flags="-pthread -lm"

source_files=(
    "main.c"
    "proc_stat_utils.c"
    "reader.c"
    "analyzer.c"
    "printer.c"
    "thread_utils.c"
    "logger.c"
)

debug=false
tests=false
print=false

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
    echo $comp_cmd -o cut "${source_files[@]}"
else
    $comp_cmd -o cut "${source_files[@]}"
fi
