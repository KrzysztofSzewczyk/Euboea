
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

. ./tests/shunit2
