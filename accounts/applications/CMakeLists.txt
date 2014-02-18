set(APPLICATION_FILES
    google-caldav.application
    google-carddav.application
)

set(APPLICATION_OUTPUT_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/google-caldav.application
    ${CMAKE_CURRENT_BINARY_DIR}/google-carddav.application
)


TRANSLATE_XML(APPLICATION_FILES)

install(FILES ${APPLICATION_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/applications
)
