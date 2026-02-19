# makefile for lua hierarchy

all:
	$(MAKE) -C src
	$(MAKE) -C clients/lib
	$(MAKE) -C clients/lua

clean:
	$(MAKE) -C src clean
	$(MAKE) -C clients/lib clean
	$(MAKE) -C clients/lua clean