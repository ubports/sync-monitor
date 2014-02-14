project(synq-upstart)

configure_file(sync-monitor.conf.in ${CMAKE_CURRENT_BINARY_DIR}/sync-monitor.conf)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sync-monitor.conf
    DESTINATION ${CMAKE_INSTALL_FULL_SHAREDSTATEDIR}/upstart/sessions
)
