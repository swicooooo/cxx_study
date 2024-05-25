file(GLOB_RECURSE CXX_LIST "*.cc" test/)
# list(REMOVE_ITEM CXX_LIST ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp)
add_executable(test ${CXX_LIST})
