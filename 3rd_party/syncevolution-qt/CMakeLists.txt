project(syncevolution-qt)

set(SYNCEVOLUTION_QT syncevolution-qt)

set(SYNCEVOLUTION_QT_SRC
    dbustypes.cpp
    dbustypes.h
)

add_library(${SYNCEVOLUTION_QT} STATIC
    ${SYNCEVOLUTION_QT_SRC}
)

qt5_use_modules(${SYNCEVOLUTION_QT} Core DBus)
