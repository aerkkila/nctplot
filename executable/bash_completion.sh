_nctplot() {
    was_extglob=`shopt -p extglob`
    shopt -s extglob
    local last=' '
    local binary=
    compgenstr='!*.@(nc|hdf|h5)?(.lz4)' # Default is to exclude the pattern. '!' negates this.
    binopts='-b --binary -x --x -y --y -t --datatype' # then any file matches

    for w in ${COMP_WORDS[@]}; do
	for opt in $binopts; do
	    if [ "$w" = "$opt" ]; then
		binary=1
		compgenstr='!*'
		break 2
	    fi
	done
    done

    COMPREPLY=($(compgen -o plusdirs -f -X $compgenstr "${COMP_WORDS[COMP_CWORD]}"))

    if [ ${#COMPREPLY[@]} = 1 ]; then
	[ -d "$COMPREPLY" ] && last=/
	COMPREPLY=$(printf %s%s "$COMPREPLY" "$last")
    fi

    eval $was_extglob
}
complete -o nospace -F _nctplot nctplot
