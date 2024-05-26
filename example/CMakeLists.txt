file(GLOB_RECURSE CXX_LIST "*.cc" example/)

add_executable(exam ${CXX_LIST})
target_link_libraries(main mysqlclient)