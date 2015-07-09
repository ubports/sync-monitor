set(SERVICE_FILES
     google-caldav.service
     google-carddav.service
     yahoo-caldav.service
     yahoo-carddav.service
)

set(SERVICE_OUTPUT_FILES
     ${CMAKE_CURRENT_BINARY_DIR}/google-caldav.service
     #${CMAKE_CURRENT_BINARY_DIR}/google-carddav.service
     #${CMAKE_CURRENT_BINARY_DIR}/yahoo-caldav.service
     #${CMAKE_CURRENT_BINARY_DIR}/yahoo-carddav.service
)

TRANSLATE_XML(SERVICE_FILES)

install(FILES ${SERVICE_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/services
)
