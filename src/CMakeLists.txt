file(GLOB_RECURSE CXX_LIST "*.cpp" src/)

add_executable(main ${CXX_LIST})
# add_library(mymuduo SHARED ${SRC_LIST})