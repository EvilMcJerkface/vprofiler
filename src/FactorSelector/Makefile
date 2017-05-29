all: annotator selector breaker vprofiler

annotator: SourceAnnotator.java
	javac SourceAnnotator.java
	chmod +x annotate.sh

selector:
	javac FactorSelector.java
	chmod +x select_factor.sh

breaker:
	chmod +x var_breaker.py

vprofiler: 
	javac -cp commons-cli-1.3.1.jar Utils.java Dispatcher.java VProfiler.java

clean:
	rm *.class
