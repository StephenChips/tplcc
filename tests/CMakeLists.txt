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
)

target_link_libraries(
  tests-main
  GTest::gtest_main
)

add_test(tests-main tests-main)