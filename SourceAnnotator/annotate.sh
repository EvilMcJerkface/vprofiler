#!/bin/sh

if [[ "$#" -ne 3 ]]; then
	echo "Usage: $0 <source code file> <function name> <path of trace_tool.cc>"
fi

java SourceAnnotator $1 $2
num_func=`wc -l < $2`
sed -i -E "s/NUMBER_OF_FUNCTIONS [0-9]+/NUMBER_OF_FUNCTIONS $(echo $num_func)/g" $3
echo "Don't forget to use the updated trace_tool.cc"
