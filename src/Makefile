INSTALL_PREFIX = /usr/local

.PHONY: all
all: FactorSelector Main SynchronizationInstrumentor TracerInstrumentor

.PHONY: FactorSelector
FactorSelector:
	make -C FactorSelector

.PHONY: Main
Main:
	make -C Main

.PHONY: SynchronizationInstrumentor
SynchronizationInstrumentor:
	make -C SynchronizationInstrumentor

.PHONY: TracerInstrumentor
TracerInstrumentor:
	make -C TracerInstrumentor

.PHONY: install
install: all
	make -C FactorSelector install INSTALL_PREFIX=$(INSTALL_PREFIX)
	make -C Main install INSTALL_PREFIX=$(INSTALL_PREFIX)
	make -C SynchronizationInstrumentor install INSTALL_PREFIX=$(INSTALL_PREFIX)
	make -C TracerInstrumentor install INSTALL_PREFIX=$(INSTALL_PREFIX)
