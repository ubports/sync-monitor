configure_file("${CMAKE_CURRENT_SOURCE_DIR}/com.canonical.SyncMonitor.service.in"
               "${CMAKE_CURRENT_BINARY_DIR}/com.canonical.SyncMonitor.service"
               IMMEDIATE @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.SyncMonitor.service
        DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbus-1/services/
)
