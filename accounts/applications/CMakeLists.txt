set(APPLICATION_FILES
    contacts-sync.application
    calendar-sync.application
)

set(APPLICATION_OUTPUT_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/contacts-sync.application
    ${CMAKE_CURRENT_BINARY_DIR}/calendar-sync.application
)


TRANSLATE_XML(APPLICATION_FILES)

install(FILES ${APPLICATION_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/applications
)
