find_program(CLANG_FORMAT NAMES clang-format-9)
add_custom_target(
  clangformat
  COMMAND ${CLANG_FORMAT} -i ${HEADERS} ${SOURCES})
