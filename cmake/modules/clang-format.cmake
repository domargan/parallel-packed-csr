find_program(CLANG_FORMAT NAMES clang-format)
add_custom_target(
  clangformat
  COMMAND ${CLANG_FORMAT} -i ${parallel-packed-csr_HEADERS} ${parallel-packed-csr_SOURCES}
    ${parallel-packed-csr_TEST_HEADERS} ${parallel-packed-csr_TEST_SOURCES})
