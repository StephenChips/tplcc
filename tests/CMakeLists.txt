find_package(GTest CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()


add_executable(tests-main
	"mocking/simple-string-scanner.cpp"
	"test-lexer.cpp"
	"test-preprocessor.cpp" 
	 
	"../tplcc/lexer.cpp"
	"../tplcc/preprocessor.cpp"
	"../tplcc/code-buffer.cpp"
	"../tplcc/encoding.cpp"
 "utils/helpers.h" "utils/helpers.cpp")

target_include_directories(tests-main PUBLIC "..")
target_link_libraries(
  tests-main
  GTest::gtest_main
)

add_test(tests-main tests-main)