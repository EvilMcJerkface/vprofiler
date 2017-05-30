INSTALL_PREFIX := /usr/local

.PHONY: all
all: 
	chmod +x restore

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/Restore
	cp *.py $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/Restore
	cp restore $(DESTDIR)$(INSTALL_PREFIX)/bin/
