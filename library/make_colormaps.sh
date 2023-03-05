#!/bin/sh
rm -f colormaps.h

for file in `ls colormaps/colormaps_*.h`; do
    printf "#include \"${file}\"\n" >> colormaps.h
done

printf \
"\n#define COLORMAP(map) cmap_##map, #map,"\
"\nstatic char* colormaps[] = {\n" >> colormaps.h

for map in `ls colormaps/colormaps_*.h |grep -Po '(?<=colormaps_)\w*'`; do
    printf "    COLORMAP(${map})\n" >> colormaps.h
done

printf \
"};\n"\
"#undef COLORMAP\n"\
"\n#define COLORVALUE(imap,value) (colormaps[(imap)*2] + (value)*3)\n" >> colormaps.h
