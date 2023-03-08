MAKE = make --no-print-directory

all: executable.target library.target

executable.target: library.target
	$(MAKE) -C executable

library.target: 
	$(MAKE) -C library

install: executable.target library.target
	$(MAKE) -C executable install
	$(MAKE) -C library install

uninstall:
	$(MAKE) -C executable uninstall
	$(MAKE) -C library uninstall

clean:
	$(MAKE) -C executable clean
	$(MAKE) -C library clean
