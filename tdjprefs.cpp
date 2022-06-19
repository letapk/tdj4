/*

The Daily Journal - A PIM program
Qt version

begin                : 12 July 2013
copyright            : (C) Kartik Patel
email                : letapk@gmail.com

*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *

*/

//Last modified 19 June 2022

#include "tdj.h"

void MainWindow::writeprefs()
{
    QSettings settings(tr("tdj"), tr("The Daily Journal"));

    settings.setValue(tr("pos"), pos());//window position

    settings.setValue(tr("size"), size());//window size

    settings.setValue(tr("weekstart"), weekstrt);//week starts on this day

    if (grid == true)
        settings.setValue(tr("Grid"), tr("1"));
    else
        settings.setValue(tr("Grid"), tr("0"));

    if (weeknum == true)
        settings.setValue(tr("WeekNum"), tr("1"));
    else
        settings.setValue(tr("WeekNum"), tr("0"));

#ifdef Q_OS_LINUX
    if (fortune == true)
        settings.setValue(tr("Fortune"), tr("1"));
    else
        settings.setValue(tr("Fortune"), tr("0"));
#endif

    settings.setValue(tr("Font"), QString(curfont.toString()));//selected font

    settings.setValue(tr("Startingtab"), tabstart);//starting tab

    headerred = headercolor.red();
    headergreen = headercolor.green();
    headerblue = headercolor.blue();

    settings.setValue(tr("Headerred"), headerred);//calendar header background colors
    settings.setValue(tr("Headergreen"), headergreen);//calendar header background colors
    settings.setValue(tr("Headerblue"), headerblue);//calendar header background colors
}

void MainWindow::readprefs()
{
int i;
QString s, s1;
QFont f;

    QSettings settings(tr("tdj"), tr("The Daily Journal"));

    QPoint pos = settings.value(tr("pos"), QPoint(20, 20)).toPoint();

    QSize size = settings.value(tr("size"), QSize(800, 630)).toSize();

    s = settings.value(tr("weekstart"), QString(tr("7"))).toString();
    weekstrt = s.toInt();
    if (weekstrt == 1){
        mon->setChecked(true);
        sun->setChecked(false);
        weekstartmon(true);
    }
    else {
        sun->setChecked(true);
        mon->setChecked(false);
        weekstartsun(true);
    }

    i = settings.value(tr("Grid"), QString(tr("0"))).toInt();
    if (i == 1) {
        grid = true;
        gridbox->setChecked(true);
    }
    else {
        grid = false;
        gridbox->setChecked(false);
    }

    i = settings.value(tr("WeekNum"), QString(tr("0"))).toInt();
    if (i == 1) {
        weeknum = true;
        weeknumbox->setChecked(true);
    }
    else {
        weeknum = false;
        weeknumbox->setChecked(false);
    }

#ifdef Q_OS_LINUX
    i = settings.value(tr("Fortune"), QString(tr("0"))).toInt();
    if (i == 1) {
        fortune = true;
        fortunebox->setChecked(true);
    }
    else {
        fortune = false;
        fortunebox->setChecked(false);
    }
#endif

    f = QApplication::font();
    s1 = f.toString();
    s = settings.value(tr("Font"), QString(s1)).toString();
    curfont.fromString(s);
    QApplication::setFont(curfont);

    s = settings.value(tr("Startingtab"), QString(tr("0"))).toString();
    tabstart = s.toInt();

    if (tabstart == 0){
        t1->setChecked(true);
    }
    if (tabstart == 1){
        t2->setChecked(true);
    }
    if (tabstart == 2){
        t3->setChecked(true);
    }
    if (tabstart == 3){
        t4->setChecked(true);
    }

    s = settings.value(tr("Headerred"), QString(tr("0"))).toString();
    headerred = s.toInt();
    s = settings.value(tr("Headergreen"), QString(tr("255"))).toString();
    headergreen = s.toInt();
    s = settings.value(tr("Headerblue"), QString(tr("255"))).toString();
    headerblue = s.toInt();

    headercolor.setRed(headerred);
    headercolor.setGreen(headergreen);
    headercolor.setBlue(headerblue);

    resize(size);
    move(pos);
}

void MainWindow::weekstartsun (bool checked)
{
    if (checked == true)
        calendar->setFirstDayOfWeek (Qt::Sunday);

    weekstrt = 7;
}

void MainWindow::weekstartmon (bool checked)
{
    if (checked == true)
        calendar->setFirstDayOfWeek (Qt::Monday);

    weekstrt = 1;
}

void MainWindow::set_cal_grid(int)
{
bool ok;

     ok = gridbox->isChecked ();
     if (ok == true)
         grid = true;
     else
         grid = false;

     if (grid == true)
         calendar->setGridVisible(true);
     else
         calendar->setGridVisible(false);

}

void MainWindow::set_cal_week_num(int)
{
bool ok;

    ok = weeknumbox->isChecked ();
    if (ok == true)
        weeknum = true;
    else
        weeknum = false;

    if (weeknum == true)
        calendar->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
    else
        calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

}

void MainWindow::fortunestate (int)
{
#ifdef Q_OS_LINUX
bool ok;

    ok = fortunebox->isChecked();
    if (ok == true)
        fortune = true;
    else
        fortune = false;
#endif
}

void MainWindow::select_font()
{
bool ok;
QFont f;

    f = QFontDialog::getFont(&ok, curfont, this);
    if (ok == true) {
        //set the user selected font everywhere
        QApplication::setFont(f);
        curfont = f;
    } else {
        return;
    }
}

void MainWindow::create_prefs_weekgrp_box ()
{
    weekbox = new QGroupBox ();
    sun = new QRadioButton (tr("The week begins on S&unday"));
    sun->setToolTip(tr("The calendar will show the week beginning on a Sunday"));
    connect (sun, SIGNAL(clicked(bool)), this, SLOT(weekstartsun (bool)));
    sun->setChecked(true);

    mon = new QRadioButton(tr("The week begins on &Monday"));
    mon->setToolTip(tr("The calendar will show the week beginning on a Monday"));
    connect (mon, SIGNAL(clicked(bool)), this, SLOT(weekstartmon (bool)));

    gridbox = new QCheckBox (tr("Draw grid l&ines in the calendar"));
    gridbox->setToolTip(tr("The calendar will draw lines between adjacent dates"));
    connect (gridbox, SIGNAL(stateChanged(int)), this, SLOT(set_cal_grid (int)));

    weeknumbox = new QCheckBox (tr("Show the &week numbers in the calendar"));
    weeknumbox->setToolTip(tr("The leftmost column shows the week number"));
    connect (weeknumbox, SIGNAL(stateChanged(int)), this, SLOT(set_cal_week_num (int)));

#ifdef Q_OS_LINUX
    fortunebox = new QCheckBox(tr("Show a &quote from \"fortune\" in blank notes"), prefs);
    fortunebox->setToolTip(tr("If the 'fortune' program is installed, blank notes will display a quote"));
    connect (fortunebox, SIGNAL(stateChanged(int)), this, SLOT(fortunestate (int)));
#endif

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(sun);
    vbox->addWidget(mon);
    vbox->addWidget(gridbox);
    vbox->addWidget(weeknumbox);
#ifdef Q_OS_LINUX
    vbox->addWidget(fortunebox);
#endif

    weekbox->setLayout(vbox);
    weekbox->setFlat(true);
}

void MainWindow::create_prefs_tabgrp_box ()
{
    tabbox = new QGroupBox (tr("When the pro&gram starts display :"));

    t1 = new QRadioButton (tr("Notes"));//index 0
    connect (t1, SIGNAL(clicked(bool)), this, SLOT(tab_start (bool)));
    t1->setChecked(true);

    t2 = new QRadioButton (tr("Appointments"));//index 1
    connect (t2, SIGNAL(clicked(bool)), this, SLOT(tab_start (bool)));

    t3 = new QRadioButton (tr("Contacts"));//index 2
    connect (t3, SIGNAL(clicked(bool)), this, SLOT(tab_start (bool)));

    t4 = new QRadioButton (tr("Lists"));//index 3
    connect (t4, SIGNAL(clicked(bool)), this, SLOT(tab_start (bool)));

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(t1);
    vbox->addWidget(t2);
    vbox->addWidget(t3);
    vbox->addWidget(t4);

    tabbox->setLayout(vbox);
    tabbox->setFlat(true);

}

void MainWindow::tab_start(bool)
{
    tabstart = 0;

    if (t1->isChecked() == true)
        tabstart = 0;
    if (t2->isChecked() == true)
        tabstart = 1;
    if (t3->isChecked() == true)
        tabstart = 2;
    if (t4->isChecked() == true)
        tabstart = 3;
}
