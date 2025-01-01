#!/bin/sh

write_and_exit() {
    printf "static const char* pager_path = \"$1\";\n"
    shift
    printf "static char* const pager_args[] = {"
    while [ "$1" != "" ]; do
	printf "\"$1\", "
	shift
    done
    printf "\"$file_to_open\", NULL};\n"
    exit 0
}

file_to_open="$1"
shift

while [ "$1" != "" ]; do
    # The first word is a program name which can be followed by arguments
    # for loop extracts the program name
    for p in $1; do
	prog=`which $p 2>/dev/null` && write_and_exit $prog $1
	break
    done
    shift
done

echo None of the given programs existed.
exit 1
