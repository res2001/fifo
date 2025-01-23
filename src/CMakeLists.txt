add_definitions(-DFIFO_DEBUG)

#fifo-fake-node-fix
set(FIFO_FAKE_NODE_FIX_SOURCES fifo-fake-node-fix.c)
set(FIFO_FAKE_NODE_FIX_HEADERS fifo-fake-node-fix.h queue-error.h)
set(FIFO_FAKE_NODE_FIX_HEADERS_INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fifo-fake-node-fix.h ${CMAKE_CURRENT_SOURCE_DIR}/queue-error.h)
add_library(fifo-fake-node-fix STATIC ${FIFO_FAKE_NODE_FIX_SOURCES} ${FIFO_FAKE_NODE_FIX_HEADERS})
target_sources(fifo-fake-node-fix INTERFACE ${FIFO_FAKE_NODE_FIX_HEADERS_INTERFACE})
#target_link_libraries(fifo-fake-node-fix)

#fifo-lock-free-intr
set(FIFO_LF_INTR_SOURCES fifo-lock-free-intr.c fifo-lock-free-intr.h queue-error.h)
set(FIFO_LF_INTR_HEADERS_INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fifo-lock-free-intr.h)
add_library(fifo-lock-free-intr STATIC ${FIFO_LF_INTR_SOURCES})
target_sources(fifo-lock-free-intr INTERFACE ${FIFO_LF_INTR_HEADERS_INTERFACE})
target_link_libraries(fifo-lock-free-intr Threads::Threads)

#fifo-two-lock-intr
add_library(fifo-two-lock-intr STATIC fifo-two-lock-intr.c fifo-two-lock-intr.h queue-error.h)
target_sources(fifo-two-lock-intr INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fifo-two-lock-intr.h)
target_link_libraries(fifo-two-lock-intr Threads::Threads)

#fifo-two-lock-fix
set(FIFO_TLQ_FIX_SOURCES fifo-two-lock-fix.c)
set(FIFO_TLQ_FIX_HEADERS fifo-two-lock-fix.h queue-error.h)
set(FIFO_TLQ_FIX_HEADERS_INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fifo-two-lock-fix.h)
add_library(fifo-two-lock-fix STATIC ${FIFO_TLQ_FIX_SOURCES} ${FIFO_TLQ_FIX_HEADERS})
target_sources(fifo-two-lock-fix INTERFACE ${FIFO_TLQ_FIX_HEADERS_INTERFACE})
target_link_libraries(fifo-two-lock-fix Threads::Threads)

#queue-fix-allocator-ts
set(QUEUE_FIX_ALLOCATOR_TS_SOURCES queue-fix-allocator-ts.c)
set(QUEUE_FIX_ALLOCATOR_TS_HEADERS queue-fix-allocator-ts.h)
set(QUEUE_FIX_ALLOCATOR_TS_HEADERS_INTERFACE queue-fix-allocator-ts.h)
add_library(queue-fix-allocator-ts STATIC ${QUEUE_FIX_ALLOCATOR_TS_SOURCES} ${QUEUE_FIX_ALLOCATOR_TS_HEADERS})
target_sources(queue-fix-allocator-ts INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${QUEUE_FIX_ALLOCATOR_TS_HEADERS_INTERFACE})
target_link_libraries(queue-fix-allocator-ts Threads::Threads)

#fifo-lock-intr
add_library(fifo-lock-intr STATIC fifo-lock-intr.c fifo-lock-intr.h queue-error.h)
target_sources(fifo-lock-intr INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/fifo-lock-intr.h)
target_link_libraries(fifo-lock-intr Threads::Threads)

# common
add_library(bit-scan-revers bit-scan-revers.c bit-scan-revers.h)
target_sources(bit-scan-revers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/bit-scan-revers.h)