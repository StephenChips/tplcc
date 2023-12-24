find_package(GTest CONFIG REQUIRED)

if(WIN32)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

add_executable(tests-main
	"../tcc/Lexer.cpp"
	"../tcc/Lexer.h"
	"Test_Lexer.cpp"
)

target_link_libraries(
  tests-main
  GTest::gtest_main
)

add_test(tests-main tests-main)