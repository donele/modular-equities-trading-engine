cmake_minimum_required(VERSION 3.17)

if ( NOT DEFINED VERSION )
    set( VERSION 0.0 )
endif ( NOT DEFINED VERSION )

project( mrtl VERSION ${VERSION} )

find_library( CFGDB_LIB cfgdb )
find_library( SHTREE_LIB shtree )
find_library( SHQUEUE_LIB shqueue )
find_library( MTIME_LIB mtime )
find_library( MLOG_LIB mlog )
find_library( MBODB_LIB mbodb )

set(CMAKE_INSTALL_RPATH $ORIGIN/../lib64)

include_directories( ${CMAKE_SOURCE_DIR}/include ${CMAKE_INSTALL_PREFIX}/include )
add_compile_options( $<$<CONFIG:Debug>:-DDEBUG> -Wall -fPIC )

add_subdirectory(src/common)
add_subdirectory(src/agent)
add_subdirectory(src/strategy/momentum_sieve)
add_subdirectory(src/strategy/classic)
add_subdirectory(src/model/mart)
#add_subdirectory(src/adjust_predictions)

