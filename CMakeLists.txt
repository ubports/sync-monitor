project(synq)

cmake_minimum_required(VERSION 2.8.9)

include(FindPkgConfig)
include(GNUInstallDirs)

find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Network REQUIRED)
add_definitions(-DQT_NO_KEYWORDS)

pkg_check_modules(ACCOUNTS REQUIRED accounts-qt5>=1.10)
pkg_check_modules(LIBNOTIFY libnotify)

find_program(INTLTOOL_MERGE_EXECUTABLE intltool-merge)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
               "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}"
                  -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

enable_testing()
add_subdirectory(3rd_party)
add_subdirectory(src)
add_subdirectory(upstart)
add_subdirectory(accounts)
add_subdirectory(templates)
add_subdirectory(data)
add_subdirectory(tests)
add_subdirectory(po)
add_subdirectory(Ubuntu)

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
