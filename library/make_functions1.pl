#!/bin/env perl
# In case you wander, what this does, run this and check the created file function_wrappers.c.

@nctypes  = ('NC_BYTE', 'NC_UBYTE', 'NC_CHAR', 'NC_SHORT', 'NC_USHORT', 'NC_INT', 'NC_UINT',
	     'NC_INT64', 'NC_UINT64', 'NC_FLOAT', 'NC_DOUBLE');
$ntypes = @nctypes;

$functions = '
void draw1d_@nctype(const nct_var*);
void draw2d_@nctype(const nct_var*);
';
@funs = split("\n", substr $functions, 1);

open out, ">function_wrappers.c";
foreach(@funs) {
    $_ =~ /(.+[^ ]) +(.+)_\@nctype\((.+)\);/;
    $type = $1; $name = $2; $args = $3;

    # function array
    print out "static $type (*__$name"."[])($args) = {\n";
    for($j=0; $j<$ntypes; $j++) {
	print out "    [@nctypes[$j]] = $name"."_@nctypes[$j],\n"; }
    print out "};\n";

    # a wrapper function
    print out "$type $name(";
    @arg = split(/,/, $args);
    $len = @arg;
    $len--;
    for($i=0; $i<$len; $i++) {
	print out "@arg[$i] _$i, "; }
    print out "@arg[$len] _$len) {\n";
    print out "    return __$name"."[_0->dtype](";
    for($i=0; $i<$len; $i++) {
	print out "_$i, "; }
    print out "_$len);\n}\n\n";
}

close in;
close out;
