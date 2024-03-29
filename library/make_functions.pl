#!/bin/env perl
use warnings;
$fname0   = 'functions.in.c';
@nctypes  = ('NC_BYTE', 'NC_UBYTE', 'NC_CHAR', 'NC_SHORT', 'NC_USHORT', 'NC_INT', 'NC_UINT',
	     'NC_INT64', 'NC_UINT64', 'NC_FLOAT', 'NC_DOUBLE');
@formats  = ('hhi', 'hhu', 'c', 'hi', 'hu', 'i', 'u', 'lli', 'llu', 'f', 'lf');
@ctypes   = ('char', 'unsigned char', 'char', 'short', 'unsigned short', 'int', 'unsigned',
             'long long', 'long long unsigned', 'float', 'double');
@uctypes  = ('unsigned char', 'unsigned char', 'unsigned char', 'unsigned short', 'unsigned short', 'unsigned', 'unsigned',
             'long long unsigned', 'long long unsigned', 'float', 'double');

$quiet = grep(/^-q$/, @ARGV);

sub make_wrapper_function {
    ($type, $name, $args) = @_;

    # create a function array
    print my_out_wrappers "static $type (*__$name"."[])($args) = {\n";
    for(my $j=0; $j<$ntypes; $j++) {
	print my_out_wrappers "    [$nctypes[$j]] = $name"."_$nctypes[$j],\n"; }
    print my_out_wrappers "};\n";

    # create a wrapper function
    print my_out_wrappers "$type $name("; # now: type foo(
    my @arg = split(/,/, $args);
    my $nargs = @arg;
    my $nargs1 = $nargs - 1;
    our $hasvar = $nargs && ($arg[0] =~ /^(const\s+)?(nct_var)|(nct_att)\*/);
    if ($hasvar) {
        for(my $i=0; $i<$nargs-1; $i++) {
            print my_out_wrappers "$arg[$i] _$i, "; }
        print my_out_wrappers "$arg[$nargs1] _$nargs1) {\n"; # now: type foo(type0 _0, type1 _1, type_n, _n) {
        print my_out_wrappers "    return __$name"."[_0->dtype](";
        for($i=0; $i<$nargs-1; $i++) {
            print my_out_wrappers "_$i, "; }
        print my_out_wrappers "_$nargs1);\n}\n";
    }
    else {
        print my_out_wrappers "nc_type nctype";
        for(my $i=0; $i<$nargs-1; $i++) {
            print my_out_wrappers ", $arg[$i] _$i"; }
	if ($nargs) {
	    print my_out_wrappers ", $arg[$nargs1] _$nargs1"; }
        print my_out_wrappers ") {\n"; # now: type foo(type0 _0, type1 _1, type_n, _n) {
        print my_out_wrappers "    return __$name"."[nctype](";
        if ($nargs) {
            print my_out_wrappers "_0"; }
        for(my $i=1; $i<$nargs; $i++) {
            print my_out_wrappers ", _$i"; }
        print my_out_wrappers ");\n}\n";
    }
}

$fname = $fname0;
$fname =~ s/\.in\.c/.c/;
open out1, ">$fname"; # functions.c
print out1 "#include <string.h>\n#include <nctietue3.h>\n#include <math.h>\n\n";

open file_in, "<$fname0"; # functions.in.c

# Skip a possible comment at the beginning.
$line = <file_in>;
$startpos = 0;
if ($line =~ /^\/\*/) {
    while(1) {
	if ($line =~ /\*\// or eof file_in) {
	    $startpos = tell file_in;
	    last;
	}
	$line = <file_in>;
    }
}

# Skip empty lines at the beginning.
while(<file_in>) {
    if (/^\s*$/) { # fix syntax highlighting: **
        $startpos = tell file_in;
        next;
    }
    last;
}

$ntypes = @nctypes;

open my_out_wrappers, ">function_wrappers.c";
$first = 1;

sub maybe_make_wrapper_function {
    ($type, $name, $args) = ($_[0] =~ /(\S+.*)\s+(\S+)_\@nctype\S*\((.*)\)\s*{/g); # fix highlighting: +
    if (!$type || !$name) {
	return; }
    if (substr($name, 0, 1) =~ '_') {
	return; }
    $type =~ s/^static\s+//;
    if ($type =~ /^ctype\W*$/) {
	return; }
    # the dummy arguments must not be in the generated prototypes
    # the substitution omits them and sets pointers (*) and commas (,) back
    $args_dummy = $args;
    $args =~ s/\s*(\**)\w+\s*(,|$)/$1$2/g;
    if (!$first) {
	print my_out_wrappers "\n"; }
    $first = 0;
    make_wrapper_function($type, $name, $args);
    if (!$quiet) {
	if ($hasvar) {
	    print "$type $name($args_dummy);\n"; }
	else {
	    print "$type $name(nc_type, $args_dummy);\n"; }
    }
}

for ($j=0; $j<$ntypes; $j++) {
    $a = "autogenerated/$fname0";
    $a =~ s/\.in\.c/_$nctypes[$j].c/;

    open out2, ">$a"; # autogenerated/functions_NC_TYPE.c
    seek file_in, $startpos, 0;
    my $yes_wrapper = 1;
    while(<file_in>) {
	if ($_ =~ /\@start_no_wrapper$/) {
	    $yes_wrapper = 0; }
	elsif ($_ =~ /\@end_no_wrapper$/) {
	    $yes_wrapper = 1; }
	if ($j == 0 and $yes_wrapper) {
	    maybe_make_wrapper_function($_); }

	$_ =~ s/\@nctype/$nctypes[$j]/g;
	$_ =~ s/\@form/$formats[$j]/g;
	$_ =~ s/\@ctype/$ctypes[$j]/g;
	$_ =~ s/\@uctype/$uctypes[$j]/g;
	print out2 $_;
    }
    close out2;

    print out1 "#include \"$a\"\n";
}

close my_out_wrappers;
close file_in;

print out1 "\n#include \"function_wrappers.c\"\n";
close out1;
