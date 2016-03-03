#!/bin/sh

if [[ "$#" -lt 5 ]]; then
	echo "Usage: $0 <function name file> <dir of data files generated from ExectuionTimeTracer> <heights file> <k> <root function> [selected functions]"
fi

echo 'Breaking down variance'
./var_breaker.sh -f $1 -d $2 -v variance_tree
echo 'Selecting factors'
java FactorSelector variance_tree $3 $4 $5 $6
