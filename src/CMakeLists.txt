project(synq-src)

set(SYNQ_BIN sync-monitor)

set(SYNQ_BIN_SRC
    main.cpp
    address-book-trigger.h
    address-book-trigger.cpp
    sync-account.h
    sync-account.cpp
    sync-daemon.h
    sync-daemon.cpp
    syncevolution-server-proxy.h
    syncevolution-server-proxy.cpp
    syncevolution-session-proxy.h
    syncevolution-session-proxy.cpp
)

add_executable(${SYNQ_BIN}
    ${SYNQ_BIN_SRC}
)

target_link_libraries(${SYNQ_BIN}
    ${ACCOUNTS_LIBRARIES}
    syncevolution-qt
)

qt5_use_modules(${SYNQ_BIN} Core DBus)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${ACCOUNTS_INCLUDE_DIRS}
    ${syncevolution-qt_SOURCE_DIR}
)

install(TARGETS ${SYNQ_BIN}
        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_LIBEXECDIR})
