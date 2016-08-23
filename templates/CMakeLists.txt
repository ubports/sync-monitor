set(TEMPLATE_FILES
     google.conf
     yahoo.conf
     owncloud.conf 
)

install(FILES ${TEMPLATE_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/sync-monitor/templates
)
