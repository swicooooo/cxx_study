file(GLOB_RECURSE CXX_LIST "*.cc" example/)

add_executable(Test ${CXX_LIST})

# 直接使用项目下的动态库时，需要指定全名和先后顺序
add_dependencies(Test utils)
target_link_libraries(Test ${CMAKE_SOURCE_DIR}/lib/libutils.so pthread)