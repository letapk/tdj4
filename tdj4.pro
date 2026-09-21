#-------------------------------------------------
#
# Project created by QtCreator 2013-07-10T16:35:47
#
#-------------------------------------------------

QT       += core gui widgets

#This should be uncommented to produce a compile time error
#for string literals that are not enclosed within a tr()
#DEFINES  += QT_NO_CAST_FROM_ASCII

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
    tdjstore.cpp \
    tdjcrypt.cpp \
    tdjmain.cpp \
    tdjmisc.cpp

HEADERS  += \
    tdj.h \
    tdjstore.h

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

TRANSLATIONS += tdj4_de.ts tdj4_hi.ts
