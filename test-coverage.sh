
testfactorial() {
    value=`./coverage examples/factorial.e`
    assertEquals "factorial.e: Test failed." '120' "${value}"
}

testfibonacii() {
    value=`./coverage examples/fib.e`
    assertEquals "fib.e: Test failed." '832040' "${value}"
}

testmixed() {
    value=`./coverage examples/mixed.e`
    assertEquals "mixed.e: Test failed." 'Constantinople fell in 1453' "${value}"
}

# Let's run examples
cd examples
for f in *.e; do ../coverage $f; done
cd ..

# Now, let's run it on directory
./coverage examples

# Or file
./coverage non-existent-fie.e

. ./tests/shunit2
