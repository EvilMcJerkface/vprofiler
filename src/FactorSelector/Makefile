INSTALL_PREFIX = /usr/local

.PHONY: all
all: selector breaker 

selector:
	javac FactorSelector.java
	chmod +x select_factor

breaker:
	chmod +x var_breaker

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler
	cp var_breaker $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/
	cp *.py $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/
	mkdir -p $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/CriticalPathBuilder
	cp CriticalPathBuilder/*.py $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/CriticalPathBuilder
	cp select_factor $(DESTDIR)$(INSTALL_PREFIX)/share/vprofiler/

.PHONY: clean
clean:
	rm *.class
