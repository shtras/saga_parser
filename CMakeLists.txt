cmake_minimum_required (VERSION 3.24.2)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (SagaParser VERSION 0.1)

add_library(saga_parser_compiler_flags INTERFACE)
target_compile_features(saga_parser_compiler_flags INTERFACE cxx_std_20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(wxWidgets CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_executable (saga_parser WIN32 main.cpp Parser.cpp Utils.cpp saga_parser.rc)
target_link_libraries(saga_parser PUBLIC saga_parser_compiler_flags spdlog::spdlog nlohmann_json::nlohmann_json wx::core wx::base)

file(COPY config.json DESTINATION .)

install(TARGETS saga_parser RUNTIME_DEPENDENCIES RUNTIME DESTINATION bin)

