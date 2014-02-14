set(DESKTOP_FILES
     sync-monitor-calendar.desktop
     sync-monitor-contacts.desktop
)

set(DESKTOP_OUTPUT_FILES
     ${CMAKE_CURRENT_BINARY_DIR}/sync-monitor-calendar.desktop
     ${CMAKE_CURRENT_BINARY_DIR}/sync-monitor-contacts.desktop
)


TRANSLATE_DESKTOP(DESKTOP_FILES)

install(FILES ${DESKTOP_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/applications
)
