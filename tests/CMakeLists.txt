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
    unit/test_cmsg.c
    unit/test_coro_io.c
    unit/test_timeout.c
    unit/test_tls.c
    unit/test_tls_cert.c
    unit/test_ws.c
    unit/test_wss.c
    unit/test_ipv6_dns.c
    unit/test_reconnect.c
    unit/test_pubsub_filter.c
    unit/test_stats.c
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
    if(OpenSSL_FOUND)
        target_link_libraries(${TEST_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

option(MB_BUILD_FUZZ "Build fuzz testers" OFF)
if(MB_BUILD_FUZZ)
    set(MB_FUZZ_SOURCES
        fuzz/fuzz_sipc.c
        fuzz/fuzz_ws_frame.c
        fuzz/fuzz_msgqueue.c
    )
    foreach(FUZZ_SRC ${MB_FUZZ_SOURCES})
        get_filename_component(FUZZ_NAME ${FUZZ_SRC} NAME_WE)
        add_executable(${FUZZ_NAME} ${FUZZ_SRC})
        target_link_libraries(${FUZZ_NAME} PRIVATE msgbroker_static msgbroker_headers)
        target_compile_options(${FUZZ_NAME} PRIVATE -Wall -Wextra -Wno-error)
        if(UNIX)
            target_link_libraries(${FUZZ_NAME} PRIVATE pthread)
        endif()
        if(UNIX AND NOT APPLE)
            target_link_libraries(${FUZZ_NAME} PRIVATE uring)
        endif()
        if(OpenSSL_FOUND)
            target_link_libraries(${FUZZ_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        endif()
    endforeach()
endif()
