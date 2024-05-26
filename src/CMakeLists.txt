file(GLOB_RECURSE CXX_LIST "*.cpp" src/)

# add_executable(main ${CXX_LIST})
add_library(utils SHARED ${CXX_LIST})
target_link_libraries(utils mysqlclient mymuduo pthread)