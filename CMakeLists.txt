cmake_minimum_required(VERSION 3.0)

set (CMAKE_C_COMPILER "gcc")
set (CMAKE_CXX_COMPILER "g++")

project(Dbx1000)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "-Wno-deprecated-declarations -pthread -fopenmp" CACHE INTERNAL "compiler options" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG " -fno-omit-frame-pointer -fsanitize=address -static-libasan -DDEBUG -O0 -g")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -DDEBUG -O2 -g -fno-omit-frame-pointer -fsanitize=address -static-libasan")
set(CMAKE_CXX_FLAGS_RELEASE " -DNDEBUG -O3 -g")

option(USE_CACHE_METER "Use PCM measurements" OFF)
option(USE_WAIT_FOR_INPUT "Use Wait for Input" OFF)
option(REPLICATE_INTERNALS "Replicate internal data structures" ON)

#if(CMAKE_BUILD_TYPE MATCHES DEBUG)
#    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g" CACHE INTERNAL "compiler options" FORCE)
#else()
#    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3" CACHE INTERNAL "compiler options" FORCE)
#endif()

set(LIB_THREAD "numa pthread")

add_definitions(-DNOGRAPHITE=1)

if(DEFINED CONFIG_FILE)
    SET (CONFIGURATION "${CONFIG_FILE}" CACHE STRING "Configuration to use" FORCE)
else()
    #SET (CONFIGURATION "config-original.h" CACHE STRING "Configuration to use" FORCE)
    #SET (CONFIGURATION "config-hw_clock.h" CACHE STRING "Configuration to use" FORCE)
    SET (CONFIGURATION "config-2optimised.h" CACHE STRING "Configuration to use" FORCE)
    #SET (CONFIGURATION "config-optimized.h" CACHE STRING "Configuration to use" FORCE)
endif()
add_definitions("-DCONFIG_FILE=\"variant_config/${CONFIGURATION}\"")

# include header files
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/benchmarks/ ${PROJECT_SOURCE_DIR}/benchmarks/tpcc/ ${PROJECT_SOURCE_DIR}/benchmarks/ycsb/ ${PROJECT_SOURCE_DIR}/concurrency_control/ ${PROJECT_SOURCE_DIR}/storage/ ${PROJECT_SOURCE_DIR}/system/ ${PROJECT_SOURCE_DIR}/system/utils/ ${PROJECT_SOURCE_DIR}/concurrency_control/hekaton/ ${PROJECT_SOURCE_DIR}/concurrency_control/dl_detect/ ${PROJECT_SOURCE_DIR}/concurrency_control/occ/ ${PROJECT_SOURCE_DIR}/concurrency_control/silo/ ${PROJECT_SOURCE_DIR}/concurrency_control/tictoc/ ${PROJECT_SOURCE_DIR}/concurrency_control/vll/)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}" )
set(INTELAMPLIFIER_INSTALL_DIR "/opt/intel/vtune_profiler/")
find_package(IntelAmplifier)
if(INTELAMPLIFIER_FOUND)
else()
    message(WARNING "VTune libraries not found")
endif()

#set(TBB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/tbb")
#include(${TBB_ROOT}/cmake/TBBBuild.cmake)
#set(MAKE_ARGS "tbb_build_prefix=tbb")
#tbb_build(TBB_ROOT ${TBB_ROOT} CONFIG_DIR TBB_DIR MAKE_ARGS ${MAKE_ARGS} tbb_cpf=1)
#find_package(TBB REQUIRED tbb_preview)
add_subdirectory(libs/tbb)

if(USE_CACHE_METER)
    add_definitions(-DUSE_CACHE_METER)
    add_definitions(-DPCM_USE_PERF)
    add_subdirectory(libs/pcm)
    set(PCM_LIB pcm)
else()
    set(PCM_LIB "")
endif()

if(USE_WAIT_FOR_INPUT)
    add_definitions(-DUSE_WAIT_ENTER)
endif()

if(REPLICATE_INTERNALS)
    add_definitions(-DREPLICATE_INTERNALS)
endif()

#file(GLOB_RECURSE SRC_FILES benchmarks/*.cpp concurrency_control/*.cpp storage/*.cpp system/*.cpp config.cpp)

set(SRC_FILES
system/main.cpp
system/helper.cpp
system/stats.cpp
system/global.cpp
system/wl.cpp
system/txn.cpp
system/thread.cpp
system/manager.cpp
system/utils/mem_alloc.cpp

system/query.cpp
system/parser.cpp
config.cpp
#benchmarks/ycsb_query.cpp
#benchmarks/ycsb_wl.cpp
#benchmarks/test_wl.cpp
#benchmarks/test_txn.cpp
#benchmarks/ycsb_txn.cpp
benchmarks/tpcc/tpcc_query.cpp
benchmarks/tpcc/tpcc_txn.cpp
benchmarks/tpcc/tpcc_helper.cpp
benchmarks/tpcc/tpcc_wl.cpp
benchmarks/tpcc/tpcc_wl_size.cpp
concurrency_control/tictoc/tictoc.cpp
concurrency_control/occ/occ.cpp
concurrency_control/row_vll.cpp
concurrency_control/row_mvcc.cpp
concurrency_control/plock.cpp
concurrency_control/vll/vll.cpp
concurrency_control/hekaton/hekaton.cpp
concurrency_control/row_lock.cpp
concurrency_control/row_silo.cpp
concurrency_control/silo/silo.cpp
concurrency_control/row_hekaton.cpp
concurrency_control/dl_detect/dl_detect.cpp
concurrency_control/row_ts.cpp
concurrency_control/row_tictoc.cpp
concurrency_control/row_occ.cpp
storage/row.cpp
storage/catalog.cpp
storage/index_hash.cpp
storage/index_btree.cpp
storage/table.cpp
)

add_subdirectory(test)

#foreach(WH IN ITEMS 128	256	384	512	640	768	896	1024 64 32 16 8)
#foreach(WH IN ITEMS 4 28 1024 1568)
#foreach(WH IN ITEMS 1024 1568)

IF(NOT DEFINED WHS)
SET(WHS 1 4 128 512 1024)
ENDIF()

IF(NOT DEFINED CCS)
SET(CCS WAIT_DIE NO_WAIT DL_DETECT MVCC HEKATON HSTORE OCC TICTOC SILO)
ENDIF()

IF(NOT DEFINED TPCC_SIZES)
SET(TPCC_SIZES SMALL FULL)
ENDIF()

IF(NOT DEFINED BACKOFF_DELAYS)
SET(BACKOFF_DELAYS 0)
ENDIF()

add_custom_target(rundb_all)

foreach(WH IN ITEMS ${WHS})
    foreach(CC IN ITEMS ${CCS}) #for each CC Algo
        foreach(TPCC_SIZE IN ITEMS ${TPCC_SIZES})
            #foreach(BACKOFF IN ITEMS 0 15 63 255 511 1023 2047)
            foreach(BACKOFF IN ITEMS ${BACKOFF_DELAYS})
                set(EXEC "rundb_${WH}_${CC}_${TPCC_SIZE}_${BACKOFF}")
                add_executable(${EXEC} ${SRC_FILES})
                add_dependencies(rundb_all ${EXEC})

                target_link_libraries(${EXEC} tbb ${${LIB_THREAD}} libpthread.so ${INTELAMPLIFIER_LIBITTNOTIFY} numa -ldl ${PCM_LIB})
                target_compile_definitions(${EXEC} PRIVATE NUM_WH=${WH} CC_ALG=${CC} PART_CNT=${WH})
                target_compile_options(${EXEC} PRIVATE -mtune=native)
                #target_compile_options(${EXEC} PRIVATE -march=native)
                #target_compile_options(${EXEC} PRIVATE -march=skylake-avx512)
                target_include_directories(${EXEC} PRIVATE ${INTELAMPLIFIER_INCLUDE_DIRS})

                if(TPCC_SIZE MATCHES "FULL")
                    target_compile_definitions(${EXEC} PRIVATE TPCC_SMALL=false)
                elseif(TPCC_SIZE MATCHES "SMALL")
                    target_compile_definitions(${EXEC} PRIVATE TPCC_SMALL=true)
                else()
                    message(FATAL_ERROR "${TPCC_SIZE} does not match!")
                endif()

                if(BACKOFF EQUAL 0)
                    target_compile_definitions(${EXEC} PRIVATE BOUNDED_SPINNING=false SPIN_WAIT_BACKOFF=false)
                else()
                    target_compile_definitions(${EXEC} PRIVATE BOUNDED_SPINNING=true BOUNDED_SPINNING_CYCLES=${BACKOFF} SPIN_WAIT_BACKOFF=true SPIN_WAIT_BACKOFF_MAX=${BACKOFF})
                endif()
            endforeach() # BACKOFF
        endforeach()
    endforeach()
endforeach()
