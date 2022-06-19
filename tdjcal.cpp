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

int get_month_int (QString s);

extern Note note[];
extern Appointment appointment[];
extern Anniversary anniversary [];

void MainWindow::getdate ()
//get the selected year, month and date
{
QDate d;

    //get selected day and date
    year = calendar->yearShown();
    month = calendar->monthShown();
    d = calendar->selectedDate();
    date_to_show = d.day();
}

QString MainWindow::get_month_name (int mnth)
{
    if (mnth == 1)
        return QString(tr("January"));
    if (mnth == 2)
        return QString(tr("February"));
    if (mnth == 3)
        return QString(tr("March"));
    if (mnth == 4)
        return QString(tr("April"));
    if (mnth == 5)
        return QString(tr("May"));
    if (mnth == 6)
        return QString(tr("June"));
    if (mnth == 7)
        return QString(tr("July"));
    if (mnth == 8)
        return QString(tr("August"));
    if (mnth == 9)
        return QString(tr("September"));
    if (mnth == 10)
        return QString(tr("October"));
    if (mnth == 11)
        return QString(tr("November"));
    if (mnth == 12)
        return QString(tr("December"));

    return QString ("");
}

//int MainWindow::get_month_int (QString s)
int get_month_int (QString s)
{
int x = 0;

    x = s.compare(("January"), Qt::CaseInsensitive);
    if (x == 0)
        return 1;

    x = s.compare(("February"), Qt::CaseInsensitive);
    if (x == 0)
        return 2;

    x = s.compare(("March"), Qt::CaseInsensitive);
    if (x == 0)
        return 3;

    x = s.compare(("April"), Qt::CaseInsensitive);
    if (x == 0)
        return 4;

    x = s.compare(("May"), Qt::CaseInsensitive);
    if (x == 0)
        return 5;

    x = s.compare(("June"), Qt::CaseInsensitive);
    if (x == 0)
        return 6;

    x = s.compare(("July"), Qt::CaseInsensitive);
    if (x == 0)
        return 7;

    x = s.compare(("August"), Qt::CaseInsensitive);
    if (x == 0)
        return 8;

    x = s.compare(("September"), Qt::CaseInsensitive);
    if (x == 0)
        return 9;

    x = s.compare(("October"), Qt::CaseInsensitive);
    if (x == 0)
        return 10;

    x = s.compare(("November"), Qt::CaseInsensitive);
    if (x == 0)
        return 11;

    x = s.compare(("December"), Qt::CaseInsensitive);
    if (x == 0)
        return 12;

    return 0;
}

void MainWindow::format_anniversaries ()
//color the dates with anniversaries yellow
{
int days, i, j;
QDate *date, *date1;
QTextCharFormat f1;

    f1 = calendar->weekdayTextFormat(Qt::Monday);
    f1.setBackground(Qt::yellow);

    date1 = new QDate (calendar->yearShown(), calendar->monthShown(), 1);
    days = date1->daysInYear();

    for (i = 1; i <= days; i++){
        if (anniversary[i].description.length() != 0) {
            j = get_month_int (anniversary[i].month);
            date = new QDate (calendar->yearShown(), j, anniversary[i].date.toInt());
            if ((j - month) == 0) {
                calendar->setDateTextFormat(*date, f1);
            }
            delete date;
        }
    }
    delete date1;
}

void MainWindow::format_appointments ()
//color the dates with appointments cyan
{
int days, i, j = 0, row;
QDate *date, *date1;
QTextCharFormat f1;

    f1 = calendar->weekdayTextFormat(Qt::Monday);
    f1.setBackground(Qt::cyan);

    date1 = new QDate (calendar->yearShown(), calendar->monthShown(), 1);
    days = date1->daysInMonth();

    for (i = 1; i <= days; i++) {//loop over the dates of this month
        j = 0;
        date = new QDate (calendar->yearShown(), calendar->monthShown(), i);
        for (row = 0; row < 48; row++) {//loop over all the appointments
            j += appointment[i].apptdesc[row+1].length();//add the length of their descriptions
            if (j > 0){//some description exists, color it cyan
                calendar->setDateTextFormat(*date, f1);
            }
        }
        delete date;
    }
    delete date1;
}

void MainWindow::format_notes()
//color the dates with notes lightgray
{
int days, i;
QDate *date, *date1;
QTextCharFormat f, f1;
QTextDocument *doc;
QString s;

    f = f1 = calendar->weekdayTextFormat(Qt::Monday);
    f.setBackground(Qt::white);
    f1.setBackground(Qt::lightGray);

    date1 = new QDate (calendar->yearShown(), calendar->monthShown(), 1);
    days = date1->daysInMonth();

    for (i = 1; i <= days; i++) {//loop over the dates of this month
        date = new QDate (calendar->yearShown(), calendar->monthShown(), i);

        doc = new QTextDocument ();
        doc->setHtml(note[i].data);
        s = doc->toPlainText();
        delete doc;

        if (s.isEmpty() == false){//some note exists, color it lightgray
        //if ((note[i].data.isEmpty()) == false){//some note exists, color it lightgray
            calendar->setDateTextFormat(*date, f1);
        }
        else {//default background is white
            calendar->setDateTextFormat(*date, f);
        }
        delete date;
    }
    delete date1;
}

void MainWindow::format_headers()
//color the calendar vertical and horizontal headers
{
QTextCharFormat f;

    f = calendar->headerTextFormat();
    f.setBackground(headercolor);
    calendar->setHeaderTextFormat(f);
}

void MainWindow::set_header_color()
{
QColor c;

    c = QColorDialog::getColor(headercolor);
    if (c.isValid() != false) {
        headercolor = c;
        format_headers();
    }
}

void MainWindow::go_to_today()
{
QDate d;

    d = QDate::currentDate();
    calendar->setSelectedDate(d);

    //update current date (it may have crossed midnight at month end or year end!)
    d.getDate(&current_year, &current_month, &current_date);
    //set the label for the button showing today's date
    todaybut.setText(d.toString());

    calendar->showToday();

    getdate ();

    load_month();
    load_day();
}
