
testfactorial() {
    value=`./euboea examples/factorial.e`
    assertEquals "factorial.e: It's not 1999 :-(" '120' "${value}"
}

testfibonacii() {
    value=`./euboea examples/fib.e`
    assertEquals "fib.e: Test failed." '832040' "${value}"
}

testmixed() {
    value=`./euboea examples/mixed.e`
    assertEquals "mixed.e: Test failed." 'Constantinople fell in 1453' "${value}"
}

. ./tests/shunit2
