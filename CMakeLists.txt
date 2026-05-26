cmake_minimum_required(VERSION 3.10)
project(msgbroker VERSION 0.2.0 LANGUAGES C)

set(MB_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(MB_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(MB_VERSION_PATCH ${PROJECT_VERSION_PATCH})

option(MB_BUILD_TESTS "Build tests" ON)
option(MB_BUILD_EXAMPLES "Build examples" ON)
option(MB_BUILD_PERF "Build performance benchmarks" ON)
option(MB_STATIC_LIB "Build static library" ON)
option(MB_SHARED_LIB "Build shared library" ON)

set(MB_C_STANDARD "99" CACHE STRING "C standard version (99, 11, 17, 23)")
set(CMAKE_C_STANDARD ${MB_C_STANDARD})
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(msgbroker_headers INTERFACE)
target_include_directories(msgbroker_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

configure_file(mb_config.h.in mb_config.h @ONLY)

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Werror)
    add_definitions(-D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE)
    if(CMAKE_C_STANDARD GREATER_EQUAL 23)
        add_compile_options(-std=c2x)
    endif()
else()
    add_compile_options(/W4 /WX)
endif()

set(MB_SOURCES
    src/utils/list.c
    src/utils/queue.c
    src/utils/hash.c
    src/utils/trie.c
    src/utils/wire.c
    src/utils/err.c
    src/utils/alloc.c
    src/utils/random.c
    src/utils/net.c
    src/utils/lb.c
    src/utils/fq.c
    src/pal/mutex.c
    src/pal/condvar.c
    src/pal/thread.c
    src/pal/efd.c
    src/pal/clock.c
    src/pal/sleep.c
    src/pal/semaphore.c
    src/memory/chunk.c
    src/memory/chunkref.c
    src/memory/msg.c
    src/memory/slab.c
    src/memory/arena.c
    src/memory/pool.c
    src/aio/fsm.c
    src/aio/evloop.c
    src/aio/timer.c
    src/aio/timerset.c
    src/aio/worker.c
    src/aio/threadpool.c
    src/aio/coroutine.c
    src/aio/ctx.c
    src/aio/pool.c
    src/core/global.c
    src/core/sock.c
    src/core/ep.c
    src/core/pipe.c
    src/core/poll.c
    src/core/device.c
    src/protocol/sockbase.c
    src/protocol/pair/pair.c
    src/protocol/pair/xpair.c
    src/protocol/pubsub/pub.c
    src/protocol/pubsub/sub.c
    src/protocol/pubsub/xpub.c
    src/protocol/pubsub/xsub.c
    src/protocol/reqrep/req.c
    src/protocol/reqrep/rep.c
    src/protocol/reqrep/xreq.c
    src/protocol/reqrep/xrep.c
    src/protocol/pipeline/push.c
    src/protocol/pipeline/pull.c
    src/protocol/pipeline/xpush.c
    src/protocol/pipeline/xpull.c
    src/protocol/bus/bus.c
    src/protocol/bus/xbus.c
    src/protocol/survey/surveyor.c
    src/protocol/survey/respondent.c
    src/protocol/survey/xsurveyor.c
    src/protocol/survey/xrespondent.c
    src/transport/tcp/tcp.c
    src/transport/tcp/btcp.c
    src/transport/tcp/ctcp.c
    src/transport/ipc/ipc.c
    src/transport/ipc/sipc.c
    src/transport/ipc/bipc.c
    src/transport/ipc/cipc.c
    src/transport/inproc/ins.c
    src/transport/inproc/msgqueue.c
    src/transport/inproc/sinproc.c
    src/transport/inproc/inproc.c
    src/transport/inproc/inproc_binproc.c
    src/transport/inproc/inproc_cinproc.c
    src/transport/tls/tls.c
    src/transport/tls/stls.c
    src/transport/tls/btls.c
    src/transport/tls/ctls.c
    src/transport/ws/ws.c
    src/transport/ws/sws.c
    src/transport/ws/bws.c
    src/transport/ws/cws.c
    src/transport/ws/wss.c
    src/transport/ws/bwss.c
    src/transport/ws/cwss.c
    src/distributed/discovery.c
    src/distributed/gossip.c
    src/distributed/ring.c
    src/distributed/routing.c
    src/distributed/cluster.c
    src/distributed/protocol.c
)

set(MB_HEADERS_PUBLIC
    include/msgbroker/mb.h
    include/msgbroker/mb_pair.h
    include/msgbroker/mb_pubsub.h
    include/msgbroker/mb_reqrep.h
    include/msgbroker/mb_pipeline.h
    include/msgbroker/mb_bus.h
    include/msgbroker/mb_survey.h
    include/msgbroker/mb_tcp.h
    include/msgbroker/mb_ipc.h
    include/msgbroker/mb_inproc.h
    include/msgbroker/mb_tls.h
    include/msgbroker/mb_ws.h
    include/msgbroker/mb_wss.h
    include/msgbroker/mb_cluster.h
)

include(GNUInstallDirs)

find_package(OpenSSL)
if(OpenSSL_FOUND)
    add_definitions(-DMB_HAVE_TLS)
endif()

if(MB_SHARED_LIB)
    add_library(msgbroker SHARED ${MB_SOURCES})
    target_link_libraries(msgbroker PRIVATE msgbroker_headers)
    target_compile_definitions(msgbroker PRIVATE MB_SHARED_LIB)
    set_target_properties(msgbroker PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "${MB_HEADERS_PUBLIC}"
    )
    if(UNIX)
        target_link_libraries(msgbroker PRIVATE pthread)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(msgbroker PRIVATE uring)
    endif()
    if(OpenSSL_FOUND)
        target_link_libraries(msgbroker PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    if(WIN32)
        target_link_libraries(msgbroker PRIVATE ws2_32)
    endif()
    install(TARGETS msgbroker
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/msgbroker
    )
endif()

if(MB_STATIC_LIB)
    add_library(msgbroker_static STATIC ${MB_SOURCES})
    target_link_libraries(msgbroker_static PRIVATE msgbroker_headers)
    target_compile_definitions(msgbroker_static PRIVATE MB_STATIC_LIB)
    set_target_properties(msgbroker_static PROPERTIES OUTPUT_NAME msgbroker)
    if(UNIX)
        target_link_libraries(msgbroker_static PRIVATE pthread)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(msgbroker_static PRIVATE uring)
    endif()
    if(OpenSSL_FOUND)
        target_link_libraries(msgbroker_static PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
    if(WIN32)
        target_link_libraries(msgbroker_static PRIVATE ws2_32)
    endif()
    install(TARGETS msgbroker_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(FILES ${MB_HEADERS_PUBLIC}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/msgbroker
)
install(FILES ${CMAKE_BINARY_DIR}/mb_config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
set(VERSION "${PROJECT_VERSION}")
configure_file(msgbroker.pc.in msgbroker.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/msgbroker.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

if(MB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(MB_BUILD_PERF)
    add_subdirectory(perf)
endif()
