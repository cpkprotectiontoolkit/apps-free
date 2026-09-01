
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = zlibtool
TEMPLATE = app

VERSION = 1.0.0
QMAKE_TARGET_COMPANY = "IT World\\256 Software Solutions"
QMAKE_TARGET_PRODUCT = "ZlibTool"
QMAKE_TARGET_DESCRIPTION = "ZlibTool"
QMAKE_TARGET_COPYRIGHT = "\\251 IT World\\256 Software Solutions"

SOURCES += \
    main.cpp

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
