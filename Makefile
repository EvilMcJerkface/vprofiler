.PHONY: all
all:
	make -C src all

.PHONY: install
install: all
	make -C src install

.PHONY: clean
clean:
	make -C src clean
