set(SERVICE_FILES
    generic-caldav.service
    google-caldav.service
    owncloud-caldav.service
    nextcloud-caldav.service
)

set(SERVICE_OUTPUT_FILES
     ${CMAKE_CURRENT_BINARY_DIR}/generic-caldav.service
     ${CMAKE_CURRENT_BINARY_DIR}/google-caldav.service
     ${CMAKE_CURRENT_BINARY_DIR}/owncloud-caldav.service
     ${CMAKE_CURRENT_BINARY_DIR}/nextcloud-caldav.service
)

TRANSLATE_XML(SERVICE_FILES)

install(FILES ${SERVICE_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/services
)
