set(TEMPLATE_FILES
     generic-caldav.conf
     google.conf
     yahoo.conf
     owncloud.conf
     nextcloud.conf
)

install(FILES ${TEMPLATE_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/sync-monitor/templates
)
