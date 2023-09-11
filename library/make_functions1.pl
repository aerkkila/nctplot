#!/bin/env perl
# In case you wander, what this does, run this and check the created file function_wrappers.c.

@nctypes  = ('NC_BYTE', 'NC_UBYTE', 'NC_CHAR', 'NC_SHORT', 'NC_USHORT', 'NC_INT', 'NC_UINT',
	     'NC_INT64', 'NC_UINT64', 'NC_FLOAT', 'NC_DOUBLE');
$ntypes = @nctypes;

# functions that begin with nct_var*
$functions = '
void draw1d_@nctype(const nct_var*);
';
@funs = split("\n", substr $functions, 1);

# functions that don't begin with nct_var*
# The first argument in their wrapper functions is nc_type nctype.
$functions_nctype = '
int make_minmax_@nctype();
void draw_row_@nctype(int, size_t, const void*);
';
@funs_nctype = split("\n", substr $functions_nctype, 1);

open out, ">function_wrappers.c";

sub make_function {
    # split the prototype into type, name, args
    $_[0] =~ /(.+[^ ]) +(.+)_\@nctype\((.*)\);/;
    $type = $1; $name = $2; $args = $3;

    # create a function array
    print out "static $type (*__$name"."[])($args) = {\n";
    for($j=0; $j<$ntypes; $j++) {
	print out "    [@nctypes[$j]] = $name"."_@nctypes[$j],\n"; }
    print out "};\n";

    # create a wrapper function
    print out "$type $name("; # now: type foo(
    @arg = split(/,/, $args);
    $len = @arg;
    $len1 = $len - 1;
    if ($_[1] == 0) {
        for($i=0; $i<$len-1; $i++) {
            print out "@arg[$i] _$i, "; }
        print out "@arg[$len1] _$len1) {\n"; # now: type foo(type0 _0, type1 _1, type_n, _n) {
        print out "    return __$name"."[_0->dtype](";
        for($i=0; $i<$len-1; $i++) {
            print out "_$i, "; }
        print out "_$len1);\n}\n\n";
    }
    else {
        print out "nc_type nctype";
        for($i=0; $i<$len-1; $i++) {
            print out ", @arg[$i] _$i"; }
	if ($len) {
	    print out ", @arg[$len1] _$len1"; }
        print out ") {\n"; # now: type foo(type0 _0, type1 _1, type_n, _n) {
        print out "    return __$name"."[nctype](";
        if ($len) {
            print out "_0"; }
        for($i=1; $i<$len; $i++) {
            print out ", _$i"; }
        print out ");\n}\n\n";
    }
}

foreach(@funs) {
    make_function($_, 0);
}

foreach(@funs_nctype) {
    make_function($_, 1)
}

close in;
close out;
