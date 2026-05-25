cmake_minimum_required(VERSION 3.10)

set(MB_TEST_SOURCES
    unit/test_atomic.c
    unit/test_mutex.c
    unit/test_condvar.c
    unit/test_thread.c
    unit/test_efd.c
    unit/test_clock.c
    unit/test_semaphore.c
    unit/test_list.c
    unit/test_queue.c
    unit/test_hash.c
    unit/test_trie.c
    unit/test_wire.c
    unit/test_chunk.c
    unit/test_msg.c
    unit/test_slab.c
    unit/test_arena.c
    unit/test_pool.c
    unit/test_fsm.c
    unit/test_evloop.c
    unit/test_timer.c
    unit/test_threadpool.c
    unit/test_coroutine.c
    unit/test_global.c
    unit/test_socket.c
    unit/test_endpoint.c
    unit/test_pipe.c
    unit/test_poll.c
    unit/test_device.c
    unit/test_inproc.c
    unit/test_ipc.c
    unit/test_tcp.c
    unit/test_pipeline.c
    unit/test_reqrep.c
    unit/test_protocols.c
    unit/test_distributed.c
    unit/test_strerror.c
    unit/test_sendmsg.c
)

foreach(TEST_SRC ${MB_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE msgbroker_static msgbroker_headers)
    target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wno-error)
    if(UNIX)
        target_link_libraries(${TEST_NAME} PRIVATE pthread)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(${TEST_NAME} PRIVATE uring)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
