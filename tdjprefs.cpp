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

//Last modified 19 Sep 2026

#include <QEventLoop>
#include "tdj.h"

//userpath contains the path to the data subdirectory
extern QString userpath;

void MainWindow::writeprefs()
{
    QSettings settings("tdj", "The Daily Journal");

    settings.setValue("pos", pos());//window position

    settings.setValue("size", size());//window size

    settings.setValue("weekstart", weekstrt);//week starts on this day

    if (grid == true)
        settings.setValue("Grid", "1");
    else
        settings.setValue("Grid", "0");

    if (weeknum == true)
        settings.setValue("WeekNum", "1");
    else
        settings.setValue("WeekNum", "0");

    if (fortune == true)
        settings.setValue("Fortune", "1");
    else
        settings.setValue("Fortune", "0");

    settings.setValue("Font", QString(curfont.toString()));//selected font

    settings.setValue("Startingtab", tabstart);//starting tab

    headerred = headercolor.red();
    headergreen = headercolor.green();
    headerblue = headercolor.blue();

    settings.setValue("Headerred", headerred);//calendar header background colors
    settings.setValue("Headergreen", headergreen);//calendar header background colors
    settings.setValue("Headerblue", headerblue);//calendar header background colors

    settings.setValue("leftwidth", leftwidth);//width of the left panel

    settings.setValue("Defdatadir", Datadirectory);
}

void MainWindow::readprefs()
{
int i;
QString s, s1;
QFont f;

    QSettings settings("tdj", "The Daily Journal");

    QPoint pos = settings.value("pos", QPoint(20, 20)).toPoint();

    QSize size = settings.value("size", QSize(800, 630)).toSize();

    s = settings.value("weekstart", "7").toString();
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

    i = settings.value("Grid", "0").toInt();
    if (i == 1) {
        grid = true;
        gridbox->setChecked(true);
    }
    else {
        grid = false;
        gridbox->setChecked(false);
    }

    i = settings.value("WeekNum", "0").toInt();
    if (i == 1) {
        weeknum = true;
        weeknumbox->setChecked(true);
    }
    else {
        weeknum = false;
        weeknumbox->setChecked(false);
    }

    i = settings.value("Fortune", "0").toInt();
    if (i == 1) {
        fortune = true;
        fortunebox->setChecked(true);
    }
    else {
        fortune = false;
        fortunebox->setChecked(false);
    }

    f = QApplication::font();
    s1 = f.toString();
    s = settings.value("Font", QString(s1)).toString();
    curfont.fromString(s);
    QApplication::setFont(curfont);
    apply_font_to_calendar(curfont);

    s = settings.value("Startingtab", "0").toString();
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

    s = settings.value("Headerred", "0").toString();
    headerred = s.toInt();
    s = settings.value("Headergreen", "255").toString();
    headergreen = s.toInt();
    s = settings.value("Headerblue", "255").toString();
    headerblue = s.toInt();

    //width of the left panel, remembered from the last divider drag
    leftwidth = settings.value("leftwidth", leftwidth).toInt();

    s = settings.value("Defdatadir", userpath).toString();
    if (!s.isEmpty() && QDir(s).exists())
        Datadirectory = s;

    headercolor.setRed(headerred);
    headercolor.setGreen(headergreen);
    headercolor.setBlue(headerblue);

    resize(size);
    move(pos);

    //resize() has applied the saved window size; lay the panels out again so
    //the restored leftwidth (and its clamp to this window size) takes effect
    layout_panels ();
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

void MainWindow::set_cal_grid(Qt::CheckState)
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

void MainWindow::set_cal_week_num(Qt::CheckState)
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

void MainWindow::fortunestate (Qt::CheckState)
{
bool ok;

    ok = fortunebox->isChecked();
    if (ok == true)
        fortune = true;
    else
        fortune = false;
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
        apply_font_to_calendar(f);
        //a font change re-fits the calendar width and height to the new text
        //size (any manual resize from the horizontal divider is released)
        calheight = -1;

        //a larger font makes the calendar want more room: let the font change
        //propagate, then widen the left panel (within limits) so it is not
        //clipped. The divider lets the user adjust it further.
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        calendar->updateGeometry();
        int needed = calendar->sizeHint().width() + 20;
        if (leftwidth < needed)
            leftwidth = needed;
        layout_panels ();
    } else {
        return;
    }
}

void MainWindow::apply_font_to_calendar(const QFont &font)
//with the compact stylesheet on it, the calendar's internal navigation
//buttons (prev/next month, month and year pickers) keep the font they had
//when the stylesheet was applied and ignore application font changes, so
//push the font onto every qt_calendar_* child whenever it is set
{
    calendar->setFont(font);
    const QList<QWidget *> kids = calendar->findChildren<QWidget *>();
    for (QWidget *w : kids)
        if (w->objectName().startsWith("qt_calendar_"))
            w->setFont(font);
}

void MainWindow::create_prefs_weekgrp_box ()
{
QButtonGroup *wkbox;

    weekbox = new QGroupBox (tr("General"));

    wkbox = new QButtonGroup ();
    sun = new QRadioButton (tr("The week begins on S&unday"));
    sun->setToolTip(tr("The calendar will show the week beginning on a Sunday"));
    connect (sun, &QRadioButton::clicked, this, &MainWindow::weekstartsun);
    sun->setChecked(true);

    mon = new QRadioButton(tr("The week begins on &Monday"));
    mon->setToolTip(tr("The calendar will show the week beginning on a Monday"));
    connect (mon, &QRadioButton::clicked, this, &MainWindow::weekstartmon);

    wkbox->addButton (sun);
    wkbox->addButton (mon);

    gridbox = new QCheckBox (tr("Draw grid l&ines in the calendar"));
    gridbox->setToolTip(tr("The calendar will draw lines between adjacent dates"));
    connect (gridbox, &QCheckBox::checkStateChanged, this, &MainWindow::set_cal_grid);

    weeknumbox = new QCheckBox (tr("Show the &week numbers in the calendar"));
    weeknumbox->setToolTip(tr("The leftmost column shows the week number"));
    connect (weeknumbox, &QCheckBox::checkStateChanged, this, &MainWindow::set_cal_week_num);

    fortunebox = new QCheckBox(tr("Show a &quote from \"fortune\" in blank notes"), prefs);
    fortunebox->setToolTip(tr("If the 'fortune' program is installed, blank notes will display a quote"));
    connect (fortunebox, &QCheckBox::checkStateChanged, this, &MainWindow::fortunestate);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(sun);
    vbox->addWidget(mon);
    vbox->addWidget(gridbox);
    vbox->addWidget(weeknumbox);
    vbox->addWidget(fortunebox);

    weekbox->setLayout(vbox);
    weekbox->setFlat(true);
}

void MainWindow::create_prefs_tabgrp_box ()
{
    tabbox = new QGroupBox (tr("When the pro&gram starts display :"));

    t1 = new QRadioButton (tr("Journal"));//index 0
    connect (t1, &QRadioButton::clicked, this, &MainWindow::tab_start);
    t1->setChecked(true);

    t2 = new QRadioButton (tr("Appointments"));//index 1
    connect (t2, &QRadioButton::clicked, this, &MainWindow::tab_start);

    t3 = new QRadioButton (tr("Contacts"));//index 2
    connect (t3, &QRadioButton::clicked, this, &MainWindow::tab_start);

    t4 = new QRadioButton (tr("Notes"));//index 3
    connect (t4, &QRadioButton::clicked, this, &MainWindow::tab_start);

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
