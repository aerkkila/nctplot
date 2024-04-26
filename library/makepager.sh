#!/bin/sh

outfile="pager.h"

write_and_exit() {
    printf "/* This file was automatically created by makepager.sh.\n" >$outfile
    printf "   That program was probably called with \$pager from config.mk as arguments. */\n\n" >>$outfile
    printf "static const char* pager_path = \"$1\";\n" >>$outfile
    shift
    printf "static char* const pager_args[] = {" >>$outfile
    while [ "$1" != "" ]; do
	printf "\"$1\", " >>$outfile
	shift
    done
    printf "\"$file_to_open\", NULL};\n" >>$outfile
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
