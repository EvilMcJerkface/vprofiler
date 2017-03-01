all:
	make -C Annotator
	make -C FactorSelector

clean:
	make -C Annotator clean
	make -C FactorSelector clean
