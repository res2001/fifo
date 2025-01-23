add_definitions(-DFIFO_DEBUG)

#set-thread-affinity
set(SET_THREAD_AFFINITY_INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/set-thread-affinity.h)
set(SET_THREAD_AFFINITY_SOURCES set-thread-affinity.c)
set(SET_THREAD_AFFINITY_HEADERS set-thread-affinity.h)
add_library(set-thread-affinity STATIC ${SET_THREAD_AFFINITY_SOURCES} ${SET_THREAD_AFFINITY_HEADERS})
target_sources(set-thread-affinity INTERFACE ${SET_THREAD_AFFINITY_INTERFACE})
target_link_libraries(set-thread-affinity Threads::Threads)

#test-multithread
set(TEST_FIFO_SOURCES
    test-fifo.c 
    test-queue-fix-allocator.c 
    test-fifo-two-lock-fix.c 
    test-fifo-fake-node-fix.c 
    test-fifo-lock-free-intr.c
    test-fifo-two-lock-intr.c
    test-fifo-lock-intr.c
)
set(TEST_FIFO_HEADERS test-fifo.h)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(TEST_FIFO_LINK timestamp-win last-error-win bit-scan-revers)
endif()
add_executable(test-fifo ${TEST_FIFO_SOURCES} ${TEST_FIFO_HEADERS})
add_dependencies(test-fifo 
    set-thread-affinity 
    fifo-two-lock-fix 
    queue-fix-allocator-ts 
    fifo-fake-node-fix 
    fifo-lock-free-intr 
    fifo-two-lock-intr 
    fifo-lock-intr
    ${TEST_FIFO_LINK}
)
target_link_libraries(test-fifo 
        ${TEST_FIFO_LINK}
        set-thread-affinity 
        fifo-two-lock-fix 
        queue-fix-allocator-ts 
        fifo-fake-node-fix 
        fifo-lock-free-intr 
        fifo-two-lock-intr
        fifo-lock-intr
        PkgConfig::check 
        Threads::Threads
)

#test-fifo-fn
set(TEST_FIFO_FN_SOURCES test-fifo-fn.c)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    #last-error-win
    set(LAST_ERROR_SOURCES last-error-win.h last-error-win.c)
    set(LAST_ERROR_INTERFACE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/last-error-win.h)
    add_library(last-error-win STATIC ${LAST_ERROR_SOURCES})
    target_sources(last-error-win PUBLIC ${LAST_ERROR_INTERFACE_HEADERS})

    #timestamp-win
    set(TIMESTAMP_WIN_SOURCES timestamp-win.c timestamp-win.h)
    add_library(timestamp-win STATIC ${TIMESTAMP_WIN_SOURCES})
    target_sources(timestamp-win INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/timestamp-win.h)

    set(TEST_FIFO_FN_LINK timestamp-win last-error-win bit-scan-revers)
endif()
add_executable(test-fifo-fn ${TEST_FIFO_FN_SOURCES})
add_dependencies(test-fifo-fn fifo-fake-node-fix ${TEST_FIFO_FN_LINK})
target_link_libraries(test-fifo-fn set-thread-affinity fifo-fake-node-fix Threads::Threads ${TEST_FIFO_FN_LINK})
