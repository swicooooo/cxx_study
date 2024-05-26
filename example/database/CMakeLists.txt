file(GLOB_RECURSE CXX_LIST "*.cc" example/)

add_executable(dbTest ${CXX_LIST})

# 直接使用项目下的动态库时，需要指定全名和先后顺序
add_dependencies(dbTest utils)
target_link_libraries(dbTest ${CMAKE_SOURCE_DIR}/lib/libutils.so pthread)