#-------------------------------------------------
#
# Project created by QtCreator 2013-07-10T16:35:47
#
#-------------------------------------------------

# Updated 19 June, 2022

QT       += core gui

#This should be uncommented to produce a compile time error
#for string literals that are not enclosed within a tr()
#DEFINES  += QT_NO_CAST_FROM_ASCII

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = tdj4
TEMPLATE = app


SOURCES +=\
    tdjappt.cpp \
    tdjtree.cpp \
    tdjann.cpp \
    tdjlist.cpp \
    tdjprefs.cpp \
    tdjsearch.cpp \
    tdjcal.cpp \
    tdjcrypt.cpp \
    tdjmain.cpp \
    tdjmisc.cpp

HEADERS  += \
    tdj.h

FORMS    +=

OTHER_FILES += \
    Changelog.txt \
    TODO \
    README \
    COPYING \
    AUTHORS \
    README-German.txt


unix:!macx:!symbian|win32: LIBS += -lgcrypt

RESOURCES += \
    tdj.qrc

TRANSLATIONS += tdj4_de.ts
