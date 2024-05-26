cmake_minimum_required(VERSION 3.16.3)

project(project)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)              # 配置编译选项
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)   # 设置可执行文件输出路径
# set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)    # 设置库文件输出路径

include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(./src)
# add_subdirectory(./test)
add_subdirectory(./example)
