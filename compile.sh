#!/bin/sh

echo 'Compiling SourceAnnotator'
cd SourceAnnotator
javac SourceAnnotator.java
chmod +x annotate.sh
cd ..
echo 'Compiling FactorSelector'
cd FactorSelector
javac FactorSelector.java
chmod +x var_breaker.py
chmod +x select_factor.sh
cd ..
echo 'Done'