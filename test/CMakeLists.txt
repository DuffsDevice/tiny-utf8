cmake_minimum_required(VERSION 3.8)

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(tinyutf8_test)

target_sources(
	tinyutf8_test
	PRIVATE
		src/test_construction.cpp
		src/test_conversion.cpp
		src/test_iterators.cpp	 
		src/test_manipulation.cpp	
		src/test_noexceptions.cpp
		src/test_search.cpp
		src/mocks/mock_nothrowallocator.cpp
		src/mocks/mock_throwallocator.cpp
		src/helpers/helpers_ssotestutils.cpp
)
 
target_include_directories(
	tinyutf8_test
	PRIVATE 
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_compile_features(tinyutf8_test PRIVATE cxx_std_11)

target_link_libraries(
	tinyutf8_test 
	PRIVATE 
		tinyutf8::tinyutf8 
		GTest::GTest 
		GTest::Main)


set_target_properties(
    tinyutf8_test
    PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

enable_testing()

gtest_discover_tests(tinyutf8_test)
