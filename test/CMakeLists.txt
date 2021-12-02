add_executable(latch_test latch_test.cpp)

#target_link_libraries(latch_test db-engine-paradigms-common tbb libpthread.so libjemalloc.so -ldl ${INTELAMPLIFIER_LIBITTNOTIFY} pcm numa)
target_link_libraries(latch_test tbb libpthread.so libjemalloc.so -ldl ${INTELAMPLIFIER_LIBITTNOTIFY} numa)
target_include_directories(latch_test PRIVATE ${INTELAMPLIFIER_INCLUDE_DIRS})

add_executable(timer_test timer_test.cpp)
#target_link_libraries(latch_test db-engine-paradigms-common tbb libpthread.so libjemalloc.so -ldl ${INTELAMPLIFIER_LIBITTNOTIFY} pcm numa)
target_link_libraries(timer_test tbb libpthread.so -ldl ${INTELAMPLIFIER_LIBITTNOTIFY} numa)
target_include_directories(timer_test PRIVATE ${INTELAMPLIFIER_INCLUDE_DIRS})

add_executable(index_test index_test.cpp)
target_link_libraries(index_test tbb libpthread.so -ldl ${INTELAMPLIFIER_LIBITTNOTIFY} numa)
target_include_directories(index_test PRIVATE ${INTELAMPLIFIER_INCLUDE_DIRS})
target_compile_definitions(index_test PRIVATE -DDEBUG)