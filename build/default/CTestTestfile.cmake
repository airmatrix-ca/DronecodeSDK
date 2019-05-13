# CMake generated Testfile for 
# Source directory: /Users/shusil/Documents/DroneCode/DronecodeSDK
# Build directory: /Users/shusil/Documents/DroneCode/DronecodeSDK/build/default
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "unit_tests_runner")
set_tests_properties(unit_tests PROPERTIES  _BACKTRACE_TRIPLES "/Users/shusil/Documents/DroneCode/DronecodeSDK/cmake/unit_tests.cmake;25;add_test;/Users/shusil/Documents/DroneCode/DronecodeSDK/cmake/unit_tests.cmake;0;;/Users/shusil/Documents/DroneCode/DronecodeSDK/CMakeLists.txt;97;include;/Users/shusil/Documents/DroneCode/DronecodeSDK/CMakeLists.txt;0;")
subdirs("third_party/tinyxml2")
subdirs("third_party/zlib")
subdirs("third_party/cpp_rsc/src")
subdirs("core")
subdirs("plugins")
subdirs("third_party/gtest")
subdirs("integration_tests")
