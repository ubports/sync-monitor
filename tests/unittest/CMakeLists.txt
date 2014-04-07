macro(declare_test TESTNAME)
    add_executable(${TESTNAME}
                   ${ARGN})
    qt5_use_modules(${TESTNAME} Core Test)

    target_link_libraries(${TESTNAME}
                          gmock
    )

    add_test(${TESTNAME} ${TESTNAME})
endmacro()

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${gmock_SOURCE_DIR}/gtest/include/
)

declare_test(sync-queue-test
             sync-queue-test.cpp
             sync-account-mock.h
             ${CMAKE_SOURCE_DIR}/src/sync-queue.h
)
