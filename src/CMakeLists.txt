project(synq-src)

set(SYNQ_BIN sync-monitor)
set(SYNQ_LIB synq-lib)

set(SYNQ_LIB_SRC
    eds-helper.h
    eds-helper.cpp
    notify-message.h
    notify-message.cpp
    powerd-proxy.h
    powerd-proxy.cpp
    provider-template.h
    provider-template.cpp
    sync-account.h
    sync-account.cpp
    sync-configure.h
    sync-configure.cpp
    sync-daemon.h
    sync-daemon.cpp
    sync-dbus.h
    sync-dbus.cpp
    sync-i18n.h
    sync-queue.h
    sync-queue.cpp
    sync-network.h
    sync-network.cpp
    syncevolution-server-proxy.h
    syncevolution-server-proxy.cpp
    syncevolution-session-proxy.h
    syncevolution-session-proxy.cpp
)

add_library(${SYNQ_LIB} STATIC
    ${SYNQ_LIB_SRC}
)

target_link_libraries(${SYNQ_LIB}
    ${ACCOUNTS_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    ${URLDISPATCHER_LIBRARIES}
    syncevolution-qt
)

qt5_use_modules(${SYNQ_LIB} Core DBus Organizer Contacts Network)

set(SYNQ_BIN_SRC
    main.cpp
)

add_executable(${SYNQ_BIN}
    ${SYNQ_BIN_SRC}
)

target_link_libraries(${SYNQ_BIN}
    ${SYNQ_LIB}
)

qt5_use_modules(${SYNQ_BIN} Core )

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${ACCOUNTS_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${URLDISPATCHER_INCLUDE_DIRS}
    ${syncevolution-qt_SOURCE_DIR}
)

install(TARGETS ${SYNQ_BIN}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/sync-monitor/)
