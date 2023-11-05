include ../config.mk
include xdg-shell.mk

helper.o: helper.c xdg-shell.c xdg-shell.h *.c *.h
	$(CC) $(CFLAGS) -c $<

xdg-shell.mk:
	echo xdgshell = `find /usr/ -name xdg-shell.xml -print -quit 2>/dev/null` > $@

xdg-shell.h: $(xdgshell)
	wayland-scanner client-header $< $@

xdg-shell.c: $(xdgshell)
	wayland-scanner private-code $< $@

wltest.out: helper.c xdg-shell.c xdg-shell.h *.c *.h
	$(CC) $(CFLAGS) -o $@ $< -Dwayland_test -lwayland-client -lxkbcommon

wltest_benchmark.out: helper.c xdg-shell.c xdg-shell.h *.c *.h
	$(CC) $(CFLAGS) -o $@ $< -Dwayland_test -Dbenchmark -lwayland-client -lxkbcommon
