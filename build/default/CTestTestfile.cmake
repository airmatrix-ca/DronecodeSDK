# CMake generated Testfile for 
# Source directory: /home/airmatrix/Documents/SDKs/DronecodeSDK
# Build directory: /home/airmatrix/Documents/SDKs/DronecodeSDK/build/default
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_tests "unit_tests_runner")
subdirs("third_party/tinyxml2")
subdirs("third_party/zlib")
subdirs("third_party/cpp_rsc/src")
subdirs("core")
subdirs("plugins")
subdirs("third_party/gtest")
subdirs("integration_tests")
