cmake_minimum_required(VERSION 3.5)
set ( PROJECT_NAME "APBdriver-cveri" )
PROJECT( ${PROJECT_NAME} )

set( CMAKE_EXPORT_COMPILE_COMMANDS 1)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O0 -g3 ")
set( HLSLIB "/Users/neoflame99/Workspace/hlslib")
#set( MATCHLIB "${HLSLIB}/matchlib_connections")
set( MATCHLIB "/Users/neoflame99/Workspace/Mgc_inc/include")
set( ACTYPES "${HLSLIB}/ac_types")
set( FC4SC "/opt/fc4sc")
#set( FC4SC "/Users/neoflame99/Workspace/fc4sc")
set( UVMSC "/opt/uvm-systemc")
#set( SIMPLE_EX_INC "$ENV{SIMPLE_EX_HOME}/include" )

message(STATUS "Systemc_home: $ENV{SYSTEMC_HOME}")
message(STATUS "FC4SC: ${FC4SC}")
include_directories(
    $ENV{SYSTEMC_HOME}/include
    ${FC4SC}/includes
    ${UVMSC}/include
    ${HLSLIB}
    ${ACTYPES}/include
    #${MATCHLIB}/include
    ${MATCHLIB}
    ${PROJECT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}
    )
    
link_directories(
    $ENV{SYSTEMC_HOME}/lib
    ${UVMSC}/lib-macosarm64
#    $ENV{SYSTEMC_LIBDIR}
    )
add_executable(
    ${PROJECT_NAME}  # Executable file name
    #tb_APBdriver.cpp
    uvm_APBdriver.cpp
    ${PROJECT_SOURCE_DIR}/../src/apb_driver.cpp
    # other source file list
    )
target_link_libraries(
    ${PROJECT_NAME}
    systemc
    uvm-systemc
    )
    
