#!/bin/sh

cd examples
for f in *.e; do echo "Running $f ..."; ../coverage $f; done
cd ..

./coverage examples

./coverage non-existent-fie.e
