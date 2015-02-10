set(SYNC_MONITOR_HELPER_BIN sync-monitor-helper)

QT5_ADD_RESOURCES(SYNC_MONITOR_HELPER_RC
    sync-monitor-helper.qrc
)

add_executable(${SYNC_MONITOR_HELPER_BIN}
    main.cpp
    ${SYNC_MONITOR_HELPER_RC}
)

qt5_use_modules(${SYNC_MONITOR_HELPER_BIN} Core Gui Quick)

set(SYNC_MONITOR_HELPER_QMLS
    main.qml
)

set(SYNC_MONITOR_HELPER_INSTALL_PATH
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/sync-monitor/
)

install(FILES "syncmonitor.url-dispatcher"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/url-dispatcher/urls
)

configure_file(syncmonitor.desktop.in syncmonitor.desktop)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/syncmonitor.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(TARGETS ${SYNC_MONITOR_HELPER_BIN}
        RUNTIME DESTINATION ${SYNC_MONITOR_HELPER_INSTALL_PATH})

# make the files visible on qtcreator
add_custom_target(sync_monitor_helper_QMlFiles ALL SOURCES ${SYNC_MONITOR_HELPER_QMLS})
