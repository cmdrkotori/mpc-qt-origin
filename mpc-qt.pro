#-------------------------------------------------
#
# Project created by QtCreator 2015-04-12T18:21:51
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mpc-qt
TEMPLATE = app

CONFIG += c++11
CONFIG += link_pkgconfig
PKGCONFIG += mpv

SOURCES += main.cpp\
        hostwindow.cpp \
    mpvwidget.cpp \
    qmediaslider.cpp

HEADERS  += hostwindow.h \
    mpvwidget.h \
    qmediaslider.h

FORMS    += hostwindow.ui

RESOURCES += \
    res.qrc

OTHER_FILES += \
    LICENSE \
    README.md
