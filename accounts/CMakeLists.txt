macro(TRANSLATE_FILES INPUT_FILES FILE_TYPE)
    foreach(INPUT_FILE ${${INPUT_FILES}})
        set(INPUT_FILE_IN "${INPUT_FILE}.in")
        add_custom_target(${INPUT_FILE} ALL ${INTLTOOL_MERGE_EXECUTABLE} -${FILE_TYPE}
            ${CMAKE_SOURCE_DIR}/po ${CMAKE_CURRENT_SOURCE_DIR}/${INPUT_FILE_IN}
            ${CMAKE_CURRENT_BINARY_DIR}/${INPUT_FILE})
    endforeach()
endmacro()

macro(TRANSLATE_XML INPUT_FILES)
    TRANSLATE_FILES(${INPUT_FILES} x)
endmacro()

macro(TRANSLATE_DESKTOP INPUT_FILES)
    TRANSLATE_FILES(${INPUT_FILES} d)
endmacro()

add_subdirectory(plugin)
add_subdirectory(services)
add_subdirectory(desktop)

