INSTALL_PREFIX := /usr/local

.PHONY: all
all:
	chmod +x vprofiler

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler
	cp *.py $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/
	cp vprofiler $(DESTDIR)$(INSTALL_PREFIX)/bin/
