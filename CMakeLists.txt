# Host (PC) build: unit tests and, later, the console simulator. The firmware is built by PlatformIO.
cmake_minimum_required(VERSION 3.20)
project(IRis CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

file(GLOB_RECURSE IRIS_CORE_SOURCES CONFIGURE_DEPENDS lib/iris_core/src/*.cpp)
add_library(iris_core STATIC ${IRIS_CORE_SOURCES})
target_include_directories(iris_core PUBLIC lib/iris_core/src)
# ArduinoJson is third-party: include it as SYSTEM so our warning flags do not apply to it.
target_include_directories(iris_core SYSTEM PUBLIC third_party/ArduinoJson)

add_library(iris_testkit INTERFACE)
target_include_directories(iris_testkit INTERFACE lib/iris_testkit/src)
target_link_libraries(iris_testkit INTERFACE iris_core)

# IRremoteESP8266 compiled for the PC (its own UNIT_TEST mode) to cross-check the codec mapping.
# Needs the library sources: they are fetched by PlatformIO (pio run) into .pio/libdeps. Skipped if absent.
set(IRIS_IRLIB_DIR "${CMAKE_SOURCE_DIR}/.pio/libdeps/esp32dev/IRremoteESP8266" CACHE PATH "IRremoteESP8266 checkout")
if(EXISTS "${IRIS_IRLIB_DIR}/src/IRsend.cpp" AND EXISTS "${IRIS_IRLIB_DIR}/test/IRsend_test.h")
  file(GLOB IRIS_IRLIB_SOURCES CONFIGURE_DEPENDS "${IRIS_IRLIB_DIR}/src/*.cpp")
  add_library(irremote_host STATIC ${IRIS_IRLIB_SOURCES})
  target_compile_definitions(irremote_host PUBLIC UNIT_TEST _IR_LOCALE_=en-AU)
  target_include_directories(irremote_host SYSTEM PUBLIC "${IRIS_IRLIB_DIR}/src" "${IRIS_IRLIB_DIR}/test")
  target_compile_options(irremote_host PRIVATE -w)

  add_library(iris_ir STATIC lib/iris_ir/src/ir/IrCodec.cpp)
  target_include_directories(iris_ir PUBLIC lib/iris_ir/src)
  target_link_libraries(iris_ir PUBLIC iris_core irremote_host)

  file(GLOB IRIS_IR_TEST_SOURCES CONFIGURE_DEPENDS test/hostlib/*.cpp)
  add_executable(iris_ir_tests ${IRIS_IR_TEST_SOURCES})
  target_include_directories(iris_ir_tests SYSTEM PRIVATE third_party/doctest)
  target_link_libraries(iris_ir_tests PRIVATE iris_ir iris_testkit)
  target_compile_definitions(iris_ir_tests PRIVATE IRIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/test/fixtures")
  add_test(NAME ir_crosscheck COMMAND iris_ir_tests)
else()
  message(STATUS "IRremoteESP8266 sources not found (run 'pio run' once): IR cross-check tests skipped")
endif()

# Console simulator: same app/domain code, fake hardware.
add_library(iris_simulator STATIC simulator/src/Simulator.cpp)
target_include_directories(iris_simulator PUBLIC simulator/src)
target_link_libraries(iris_simulator PUBLIC iris_core iris_testkit)
add_executable(iris_sim simulator/src/main.cpp)
target_link_libraries(iris_sim PRIVATE iris_simulator)

# Tests: test/unit = single components, test/integration = scripted scenarios through the simulator.
enable_testing()
file(GLOB IRIS_TEST_SOURCES CONFIGURE_DEPENDS test/unit/*.cpp test/integration/*.cpp)
add_executable(iris_unit_tests ${IRIS_TEST_SOURCES})
target_include_directories(iris_unit_tests SYSTEM PRIVATE third_party/doctest)
target_link_libraries(iris_unit_tests PRIVATE iris_core iris_testkit iris_simulator)
target_compile_definitions(iris_unit_tests PRIVATE IRIS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/test/fixtures")
# test/unit/alloc_probe.cpp replaces operator new/delete to measure memory. With the shared libstdc++ on Windows,
# library code would still use its own allocator and mixed frees corrupt the heap, so link the runtime statically.
if(MINGW)
  target_link_options(iris_unit_tests PRIVATE -static)
endif()
add_test(NAME all COMMAND iris_unit_tests)
add_test(NAME unit COMMAND iris_unit_tests --test-case-exclude=*scenario*)
add_test(NAME integration COMMAND iris_unit_tests --test-case=*scenario*)
