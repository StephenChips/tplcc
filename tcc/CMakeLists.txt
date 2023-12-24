# CMakeList.txt : CMake project for tcc, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (tcc "tcc.cpp" "tcc.h" "Lexer.h" "Lexer.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET tcc PROPERTY CXX_STANDARD 11)
endif()

# TODO: Add tests and install targets if needed.
