all: annotator selector breaker

annotator: SourceAnnotator.java
	javac SourceAnnotator.java
	chmod +x annotate.sh

selector:
	javac FactorSelector.java
	chmod +x select_factor.sh

breaker:
	chmod +x var_breaker.py

clean:
	rm *.class