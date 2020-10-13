cmake_minimum_required(VERSION 3.8)
project(tinyutf8 
    VERSION 4.0.2 
    DESCRIPTION "Unicode (UTF-8) capable std::string"
    HOMEPAGE_URL "https://github.com/DuffsDevice/tiny-utf8"
    LANGUAGES CXX
)

# Make sure that custom modules are found
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

##############################################
# Declare dependencies

include(GNUInstallDirs)
include(CTest)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(IS_TOPLEVEL_PROJECT TRUE)
else()
  set(IS_TOPLEVEL_PROJECT FALSE)
endif()

option(TINYUTF8_BUILD_TESTING "Build and run TinyUTF8 tests " ${IS_TOPLEVEL_PROJECT})
option(TINYUTF8_BUILD_DOC "Generate TinyUTF8 documentation" ${IS_TOPLEVEL_PROJECT})

# Set conformance with C++11 (with no compiler/vendor extensions)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

##############################################
# Create target and set properties

add_library(${PROJECT_NAME} INTERFACE)

# Add an alias so that library can be used inside the build tree, e.g. when testing
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(
    ${PROJECT_NAME}
	INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(
    ${PROJECT_NAME} 
    INTERFACE 
        cxx_std_11
)

##############################################
## Add test

if(BUILD_TESTING AND TINYUTF8_BUILD_TESTING)
  add_subdirectory(test)
endif()

##############################################
## Add documentation

if(TINYUTF8_BUILD_DOC)
  add_subdirectory(docs)
endif()

##############################################
# Installation instructions

install(
    TARGETS ${PROJECT_NAME}
    # locations are provided by GNUInstallDirs
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

# Export the targets to a script
install(
    EXPORT 
        ${PROJECT_NAME}_Targets
    FILE 
        ${PROJECT_NAME}Targets.cmake
    NAMESPACE 
        ${PROJECT_NAME}::
    DESTINATION 
        ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(
    FILES 
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION 
        ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(
    DIRECTORY 
        ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME} 
    DESTINATION 
        include)

##############################################
## Packaging the library

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENCE")

include(CPack)
