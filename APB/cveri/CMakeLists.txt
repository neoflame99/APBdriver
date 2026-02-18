set (PROJECT_NAME APBdriver)

PROJECT( ${PROJECT_NAME} )
CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

set( CMAKE_EXPORT_COMPILE_COMMANDS 1)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O0 -g3 ")
set( HLSLIB "/Users/neoflame99/Workspace/hlslib")
set( MATCHLIB "${HLSLIB}/matchlib_connections")
set( ACTYPES "${HLSLIB}/ac_types")
#set( SIMPLE_EX_INC "$ENV{SIMPLE_EX_HOME}/include" )

include_directories(
    $ENV{SYSTEMC_INCLUDE}
    ${HLSLIB}
    ${ACTYPES}/include
    ${MATCHLIB}/include
    ${PROJECT_SOURCE_DIR}/../src
    ${PROJECT_SOURCE_DIR}
    )
    
link_directories(
#    $ENV{SYSTEMC_HOME}/lib
    $ENV{SYSTEMC_LIBDIR}
    )
add_executable(
    ${PROJECT_NAME}  # Executable file name
    tb_APBdriver.cpp
    # other source file list
    )
target_link_libraries(
    ${PROJECT_NAME}
    systemc
    )
    
