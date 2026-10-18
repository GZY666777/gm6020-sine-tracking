# CMake toolchain file for arm-none-eabi-gcc.
#
# Usage:
#   cmake -S . -B build-gcc -G Ninja ^
#         -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake ^
#         -DARM_TOOLCHAIN_PATH="D:/path/to/arm-none-eabi/bin"

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Avoid trying to link a test executable during compiler detection.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(ARM_TOOLCHAIN_PATH)
    file(TO_CMAKE_PATH "${ARM_TOOLCHAIN_PATH}" _toolchain_dir)
    set(_toolchain_prefix "${_toolchain_dir}/arm-none-eabi-")
else()
    set(_toolchain_prefix "arm-none-eabi-")
endif()

if(CMAKE_HOST_WIN32)
    set(_toolchain_exe ".exe")
else()
    set(_toolchain_exe "")
endif()

set(CMAKE_C_COMPILER   "${_toolchain_prefix}gcc${_toolchain_exe}")
set(CMAKE_ASM_COMPILER "${_toolchain_prefix}gcc${_toolchain_exe}")
set(CMAKE_OBJCOPY      "${_toolchain_prefix}objcopy${_toolchain_exe}")
set(CMAKE_SIZE         "${_toolchain_prefix}size${_toolchain_exe}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
