find_package(GTest CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)

if(WIN32)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()


add_executable(tests-main
	"../tplcc/lexer.cpp"
	"../tplcc/Lexer.h"
	"test-lexer.cpp"
	"../tplcc/scanner.h"
	"test-preprocessor.cpp" 
	"../tplcc/preprocessor.cpp"
	"../tplcc/preprocessor.h"
	"mocking/report-error-stub.h"
	"mocking/simple-string-scanner.cpp"
	"mocking/simple-string-scanner.h"
)

target_include_directories(tests-main PUBLIC "..")
target_link_libraries(
  tests-main
  GTest::gtest_main
)

add_test(tests-main tests-main)