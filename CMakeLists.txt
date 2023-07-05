cmake_minimum_required(VERSION 3.1)

project(SchemaCodeGen)

#
# common directories
#

set(SchemaCodeGen_ROOT        ${PROJECT_SOURCE_DIR})
set(SchemaCodeGen_EXT_ROOT    ${SchemaCodeGen_ROOT}/ext)


if (WIN32)
set(extra_INCLUDE    ${SchemaCodeGen_ROOT}/ext)
else()
set(extra_INCLUDE    ${SchemaCodeGen_ROOT}/ext)
endif()

#
# common compiler settings
#

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    #add_compile_options(-std=c++11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

    # enable all warnings and treat warnings as errors
    add_compile_options(-Wall -Werror -pedantic -Wno-unused-function)

    # always enable debugging symbols
    add_compile_options(-g)

elseif (MSVC)

    # generic preprocessor definitions
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DLEVELDB_COMPILE_LIBRARY)
    add_definitions(-DLEVELDB_PLATFORM_WINDOWS=1)
    add_definitions(-DLEVELDB_IS_BIG_ENDIAN=0)
    add_definitions(-DHAVE_CRC32C=0)
    add_definitions(-DHAVE_SNAPPY=1)
    add_definitions(-DHAVE_FULLFSYNC=0)
    add_definitions(-DHAVE_FDATASYNC=0)
    add_definitions(-DHAVE_CONFIG_H=1)

    # config-specific preprocessor definitions
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>
    )
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
        $<$<CONFIG:Release>:NDEBUG>
    )
    set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
        $<$<CONFIG:RelWithDebInfo>:NDEBUG>
        $<$<CONFIG:RelWithDebInfo>:DBGREL>
    )

    # enable all warnings and treat warnings as errors
    add_compile_options(/W4 /WX)

    # set static runtime libs in all configurations
    # enable multiprocessor build in release config
    add_compile_options(
        $<$<CONFIG:Release>:/MT>
        $<$<CONFIG:Release>:/MP>
    )
    add_compile_options(
        $<$<CONFIG:Debug>:/MTd>
    )
    add_compile_options(
        $<$<CONFIG:RelWithDebInfo>:/MT>
        $<$<CONFIG:RelWithDebInfo>:/MP>
        $<$<CONFIG:RelWithDebInfo>:/INCREMENTAL:NO>
        $<$<CONFIG:RelWithDebInfo>:/Zi>
        $<$<CONFIG:RelWithDebInfo>:/Zo>
    )
endif()

#
# sources
#

file (GLOB_RECURSE leveldb_EXTERNAL_SOURCES
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    include/*.h
    src/*.cpp
    include/rapidjson/*.h
    include/rapidjson/error/*.h
    include/rapidjson/internal/*.h
    include/rapidjson/msinttypes/*.h
)


set(Shared_SOURCES
)


if (WIN32)
set(Platform_SOURCES
)
else()
set(Platform_SOURCES
)
endif()

set(SchemaCodeGen_SOURCES
	app/main.cpp
)

#message("External sources:\n${SchemaCodeGen_EXTERNAL_SOURCES}")

# deal with subdirectories in external sources
foreach(source IN LISTS SchemaCodeGen_EXTERNAL_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${source_path}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

#message("External sources:\n${leveldb_EXTERNAL_SOURCES}")

# deal with subdirectories in external sources
foreach(source IN LISTS leveldb_EXTERNAL_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${source_path}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

# deal with subdirectories in sources
foreach(source IN LISTS SchemaCodeGen_SOURCES)
    get_filename_component(source_path "${source}" PATH)
    string(REPLACE "/" "\\" source_path_msvc "${source_path}")
    source_group("${source_path_msvc}" FILES "${source}")
endforeach()

#
# executable target
#

add_executable(SchemaCodeGen
    ${SchemaCodeGen_EXTERNAL_SOURCES}
    ${leveldb_EXTERNAL_SOURCES}
    ${SchemaCodeGen_SOURCES}
    ${Platform_SOURCES}
    ${Shared_SOURCES}
)

target_include_directories(SchemaCodeGen PUBLIC
    ${SchemaCodeGen_EXT_ROOT}
    ${SchemaCodeGen_EXT_ROOT}/SchemaCodeGen
    ${SchemaCodeGen_ROOT}/include
    ${SchemaCodeGen_ROOT}/include/rapidjson
    ${extra_INCLUDE}
)

if (WIN32)
    target_link_libraries(SchemaCodeGen
    )
else()
    target_link_libraries(SchemaCodeGen
        -ldl
        -lpthread
    )
endif()


set(SchemaCodeGen_BIN_DIR ${SchemaCodeGen_ROOT}/bin)
if (SchemaCodeGen_BUILD_PLATFORM)
    set(SchemaCodeGen_BIN_DIR ${SchemaCodeGen_BIN_DIR}/${SchemaCodeGen_BUILD_PLATFORM})
endif()
if (CMAKE_BUILD_TYPE)
    set(SchemaCodeGen_BIN_DIR ${SchemaCodeGen_BIN_DIR}/${CMAKE_BUILD_TYPE})
endif()

set_target_properties(SchemaCodeGen
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${SchemaCodeGen_BIN_DIR}
)
