set(SYNC_MONITOR_QML_SRC
    plugin.h
    plugin.cpp
    syncmonitor-qml.h
    syncmonitor-qml.cpp
)

set(SYNC_MONITOR_QML_FILES
    qmldir
)

add_library(syncmonitor-qml SHARED ${SYNC_MONITOR_QML_SRC})
qt5_use_modules(syncmonitor-qml Core Qml DBus)


execute_process(
    COMMAND qmake -query QT_INSTALL_QML
    OUTPUT_VARIABLE QT_INSTALL_QML
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(SYNCMONITOR_QML_INSTALL_DIR ${QT_INSTALL_QML}/Ubuntu/SyncMonitor)
install(TARGETS syncmonitor-qml DESTINATION ${SYNCMONITOR_QML_INSTALL_DIR})
install(FILES ${SYNC_MONITOR_QML_FILES} DESTINATION ${SYNCMONITOR_QML_INSTALL_DIR})

# make the files visible on qtcreator
add_custom_target(syncmonitor_QmlFiles ALL SOURCES ${SYNC_MONITOR_QML_FILES})
