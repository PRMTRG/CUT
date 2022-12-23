#!/bin/bash
set -eu

gcc_flags="-Wall -Wextra -std=c99 -pedantic"
clang_flags="-Weverything -std=c99 -pedantic"
debug_flags="-g"
op_flags="-O2"

source_files=(
    "main.c"
)

debug=false
tests=false

for arg in "$@"; do
    case "$arg" in
        "--debug")
            debug=true
        ;;
        "--tests")
            tests=true
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

if [ "$debug" == "true" ]; then
    comp_cmd+=" $debug_flags"
else
    comp_cmd+=" $op_flags"
fi

$comp_cmd -o cut "${source_files[@]}"
