add_executable(doTests test_main.cc test_configKeyValuePairs.cc test_ConfigurationParameters.cc)

target_include_directories(doTests PUBLIC "${PROJECT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(doTests PUBLIC configKeys configParams stdc++ m)

add_test (NAME RunTests COMMAND doTests)
