all: executable.target library.target

executable.target: library.target
	make -C executable

library.target: 
	make -C library

install: executable.target library.target
	make -C executable install
	make -C library install

uninstall:
	make -C executable uninstall
	make -C library uninstall

clean:
	make -C executable clean
	make -C library clean
