project(sync-monitor-translations)

# for dh_translations to extract the domain
# (regarding syntax consistency, see http://pad.lv/1181187)
set (GETTEXT_PACKAGE "sync-monitor")

include(FindGettext)

set(DOMAIN sync-monitor)
set(POT_FILE ${DOMAIN}.pot)
file(GLOB PO_FILES *.po)
file(GLOB_RECURSE I18N_SRCS RELATIVE ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)
file(GLOB_RECURSE I18N_QMLS RELATIVE ${CMAKE_SOURCE_DIR}
    ${sync-monitor-helper_SOURCE_DIR}/*.qml
)

string(REPLACE "${CMAKE_SOURCE_DIR}/" "" RELATIVE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

foreach(PO_FILE ${PO_FILES})
    get_filename_component(LANG ${PO_FILE} NAME_WE)
    gettext_process_po_files(${LANG} ALL PO_FILES ${PO_FILE})
    set(INSTALL_DIR ${CMAKE_INSTALL_LOCALEDIR}/${LANG}/LC_MESSAGES)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${LANG}.gmo
            DESTINATION ${INSTALL_DIR}
            RENAME ${DOMAIN}.mo)
endforeach(PO_FILE)

find_program(XGETTEXT_EXECUTABLE xgettext)
if(XGETTEXT_EXECUTABLE)
    add_custom_target(${POT_FILE})
    add_custom_command(TARGET ${POT_FILE}
        COMMAND ${XGETTEXT_EXECUTABLE} -o ${POT_FILE}
                --c++ --qt --add-comments=TRANSLATORS
                --keyword=_ --keyword=N_
                --keyword=tr --keyword=tr:1,2
                --keyword=dtr:2 --keyword=dtr:2,3
                --package-name=${DOMAIN}
                --copyright-holder='Canonical Ltd.'
                -D ${CMAKE_SOURCE_DIR} ${I18N_QMLS}
                -D ${CMAKE_SOURCE_DIR} ${I18N_SRCS}
                -s -p ${CMAKE_CURRENT_SOURCE_DIR} ${I18N_SRCS}
    )
endif()
