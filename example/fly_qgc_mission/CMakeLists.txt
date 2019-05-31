cmake_minimum_required(VERSION 2.8.12)


project(fly_qgc_mission)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM gtkmm-3.0)
link_directories( ${GTKMM_LIBRARY_DIRS} )
include_directories( ${GTKMM_INCLUDE_DIRS} )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

if(NOT MSVC)
    add_definitions("-std=c++14 -Wextra -Werror")
    # Line below required if /usr/local/include is not in your default includes
    #include_directories(/usr/local/include)
    # Line below required if /usr/local/lib is not in your default linker path
    #link_directories(/usr/local/lib)
else()
    add_definitions("-std=c++14 -WX -W2")
    include_directories(${CMAKE_SOURCE_DIR}/../../install/include)
    link_directories(${CMAKE_SOURCE_DIR}/../../install/lib)
endif()

add_executable(fly_qgc_mission
    fly_qgc_mission.cpp
)

target_link_libraries(fly_qgc_mission
    dronecode_sdk
    dronecode_sdk_action
    dronecode_sdk_mission
    dronecode_sdk_telemetry
    ${GTKMM_LIBRARIES}
)