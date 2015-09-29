set(SERVICE_FILES
     google-caldav.service
)

set(SERVICE_OUTPUT_FILES
     ${CMAKE_CURRENT_BINARY_DIR}/google-caldav.service
)

TRANSLATE_XML(SERVICE_FILES)

install(FILES ${SERVICE_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/services
)
