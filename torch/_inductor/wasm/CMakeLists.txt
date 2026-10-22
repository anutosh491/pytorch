cmake_minimum_required(VERSION 3.20)
project(torch_inductor_compiler LANGUAGES CXX)

find_package(LLVM 23.1.2 EXACT REQUIRED CONFIG)
find_package(Clang 23.1.2 EXACT REQUIRED CONFIG)
find_package(LLD 23.1.2 EXACT REQUIRED CONFIG)

add_library(torch_inductor_compiler SHARED CompilerService.cpp)
target_compile_features(torch_inductor_compiler PRIVATE cxx_std_20)
target_include_directories(torch_inductor_compiler PRIVATE
  ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_options(torch_inductor_compiler PRIVATE -fno-rtti)
target_link_libraries(torch_inductor_compiler PRIVATE
  clangTooling clangFrontendTool lldWasm)
target_link_options(torch_inductor_compiler PRIVATE
  -Wl,-Bsymbolic
  -Wl,--export=torch_inductor_clang_compile
  -Wl,--export=torch_inductor_lld_link
  -Wl,--export=torch_inductor_clang_version)
set_target_properties(torch_inductor_compiler PROPERTIES PREFIX "" SUFFIX ".so")

install(TARGETS torch_inductor_compiler LIBRARY DESTINATION .)
