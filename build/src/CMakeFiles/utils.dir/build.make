# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/e/cxx_study

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/e/cxx_study/build

# Include any dependencies generated for this target.
include src/CMakeFiles/utils.dir/depend.make

# Include the progress variables for this target.
include src/CMakeFiles/utils.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/utils.dir/flags.make

src/CMakeFiles/utils.dir/database/mysql.cpp.o: src/CMakeFiles/utils.dir/flags.make
src/CMakeFiles/utils.dir/database/mysql.cpp.o: ../src/database/mysql.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/e/cxx_study/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/CMakeFiles/utils.dir/database/mysql.cpp.o"
	cd /mnt/e/cxx_study/build/src && /usr/bin/g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/utils.dir/database/mysql.cpp.o -c /mnt/e/cxx_study/src/database/mysql.cpp

src/CMakeFiles/utils.dir/database/mysql.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/utils.dir/database/mysql.cpp.i"
	cd /mnt/e/cxx_study/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mnt/e/cxx_study/src/database/mysql.cpp > CMakeFiles/utils.dir/database/mysql.cpp.i

src/CMakeFiles/utils.dir/database/mysql.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/utils.dir/database/mysql.cpp.s"
	cd /mnt/e/cxx_study/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mnt/e/cxx_study/src/database/mysql.cpp -o CMakeFiles/utils.dir/database/mysql.cpp.s

src/CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.o: src/CMakeFiles/utils.dir/flags.make
src/CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.o: ../src/filetransfer/filetransfer.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/mnt/e/cxx_study/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object src/CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.o"
	cd /mnt/e/cxx_study/build/src && /usr/bin/g++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.o -c /mnt/e/cxx_study/src/filetransfer/filetransfer.cpp

src/CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.i"
	cd /mnt/e/cxx_study/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /mnt/e/cxx_study/src/filetransfer/filetransfer.cpp > CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.i

src/CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.s"
	cd /mnt/e/cxx_study/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /mnt/e/cxx_study/src/filetransfer/filetransfer.cpp -o CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.s

# Object files for target utils
utils_OBJECTS = \
"CMakeFiles/utils.dir/database/mysql.cpp.o" \
"CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.o"

# External object files for target utils
utils_EXTERNAL_OBJECTS =

../lib/libutils.so: src/CMakeFiles/utils.dir/database/mysql.cpp.o
../lib/libutils.so: src/CMakeFiles/utils.dir/filetransfer/filetransfer.cpp.o
../lib/libutils.so: src/CMakeFiles/utils.dir/build.make
../lib/libutils.so: src/CMakeFiles/utils.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/mnt/e/cxx_study/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX shared library ../../lib/libutils.so"
	cd /mnt/e/cxx_study/build/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/utils.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/utils.dir/build: ../lib/libutils.so

.PHONY : src/CMakeFiles/utils.dir/build

src/CMakeFiles/utils.dir/clean:
	cd /mnt/e/cxx_study/build/src && $(CMAKE_COMMAND) -P CMakeFiles/utils.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/utils.dir/clean

src/CMakeFiles/utils.dir/depend:
	cd /mnt/e/cxx_study/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/e/cxx_study /mnt/e/cxx_study/src /mnt/e/cxx_study/build /mnt/e/cxx_study/build/src /mnt/e/cxx_study/build/src/CMakeFiles/utils.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/utils.dir/depend
