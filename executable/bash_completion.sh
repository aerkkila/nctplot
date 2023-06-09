_nctplot() {
    local last=' '
    _init_completion
    COMPREPLY=($(compgen -o plusdirs -f -X '!*.nc' "${COMP_WORDS[COMP_CWORD]}"))
    if [ ${#COMPREPLY[@]} = 1 ]; then
	[ -d "$COMPREPLY" ] && last=/
	COMPREPLY=$(printf %s%s "$COMPREPLY" "$last")
    fi
}
complete -o nospace -F _nctplot nctplot
