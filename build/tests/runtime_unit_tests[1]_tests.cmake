add_test([=[SanityTest.Basic]=]  /home/yun/design/high-concurrency-runtime/build/tests/runtime_unit_tests [==[--gtest_filter=SanityTest.Basic]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[SanityTest.Basic]=]  PROPERTIES WORKING_DIRECTORY /home/yun/design/high-concurrency-runtime/build/tests SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  runtime_unit_tests_TESTS SanityTest.Basic)
