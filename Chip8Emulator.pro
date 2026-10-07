QT       += core gui widgets multimedia

TARGET   = Chip8Emulator
TEMPLATE = app
CONFIG   += c++17

VERSION  = 1.0.0
DEFINES  += APP_VERSION=\\\"$$VERSION\\\"

# Windows: .exe icon and the details shown under Properties > Details
win32 {
    RC_ICONS = icons/app.ico
    QMAKE_TARGET_PRODUCT     = "Chip8 Emulator"
    QMAKE_TARGET_DESCRIPTION = "CHIP-8 / SUPER-CHIP emulator"
    QMAKE_TARGET_COPYRIGHT   = "Copyright (c) 2026 Ilja Fiers"
}

# macOS: app bundle icon, bundle id and a universal (Intel + Apple Silicon) build
macx {
    ICON = icons/app.icns
    QMAKE_TARGET_BUNDLE_PREFIX = com.iljafiers
    QMAKE_APPLE_DEVICE_ARCHS   = x86_64 arm64
}

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    chip8.cpp \
    displaywidget.cpp \
    preferencesdialog.cpp \
    beeper.cpp

HEADERS += \
    mainwindow.h \
    chip8.h \
    displaywidget.h \
    preferencesdialog.h \
    beeper.h

RESOURCES += \
    resources.qrc
