#!/bin/sh

echo 'Compiling FactorSelector'
cd FactorSelector
javac FactorSelector.java
echo 'Compiling SourceAnnotator'
cd ../SourceAnnotator
javac SourceAnnotator.java
chmod +x annotate.sh
cd ..
chmod +x VarianceBreaker/var_breaker.py
echo 'Done'