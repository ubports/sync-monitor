set(PROVIDER_FILES
    generic-caldav.provider
)

set(PROVIDER_OUTPUT_FILES
     ${CMAKE_CURRENT_BINARY_DIR}/generic-caldav.provider
)

TRANSLATE_XML(PROVIDER_FILES)

install(FILES ${PROVIDER_OUTPUT_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/providers
)

set(PLUGIN_QML_FILES
    qml/Main.qml
    qml/NewAccount.qml
)

install(FILES ${PLUGIN_QML_FILES}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/accounts/qml-plugins/generic-caldav
)

set(PLUGIN_ICON
    online-accounts-caldav.png
)

install(FILES ${PLUGIN_ICON}
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/icons/suru/apps/128
)
