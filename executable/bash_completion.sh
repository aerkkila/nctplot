_nctplot() {
    local last=' '
    local binary=
    compgenstr='!*.nc'
    binopts='-b --binary -x --x -y --y'

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
}
complete -o nospace -F _nctplot nctplot
