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

int get_month_int (QString s)
{
    //new data stores a canonical month number ("1".."12") so sorting and
    //colouring never depend on the display locale. Legacy files store the
    //name exactly as it was picked in the combo box (populated with translated
    //names), so fall back to matching the translated names for both UI
    //contexts that emit them, then the English names.
    bool numok = false;
    int num = s.toInt(&numok);
    if (numok && num >= 1 && num <= 12)
        return num;

    static const char *names[13] = {
        0, "January", "February", "March", "April", "May", "June", "July",
        "August", "September", "October", "November", "December"
    };

    for (int i = 1; i <= 12; i++) {
        const char *n = names[i];
        if (s.compare(QCoreApplication::translate("ComboBoxItemDelegate", n), Qt::CaseInsensitive) == 0)
            return i;
        if (s.compare(QCoreApplication::translate("MainWindow", n), Qt::CaseInsensitive) == 0)
            return i;
        if (s.compare(QLatin1String(n), Qt::CaseInsensitive) == 0)
            return i;
    }

    return 0;
}

void MainWindow::format_anniversaries ()
//color the dates with anniversaries yellow
{
int i, j;
QDate *date;
QTextCharFormat f1;

    f1 = calendar->weekdayTextFormat(Qt::Monday);
    f1.setBackground(Qt::yellow);

    //loop over all 366 table rows, not just over the days of a (possibly
    //non-leap) year, so that the 29 February entry is never dropped
    for (i = 1; i < 367; i++){
        if (anniversary[i].description.length() != 0) {
            j = get_month_int (anniversary[i].month);
            if (j >= 1 && j <= 12) {
                date = new QDate (calendar->yearShown(), j, anniversary[i].date.toInt());
                if (date->isValid() && (j - month) == 0) {
                    calendar->setDateTextFormat(*date, f1);
                }
                delete date;
            }
        }
    }
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

    f = f1 = calendar->weekdayTextFormat(Qt::Monday);
    f.setBackground(Qt::white);
    f1.setBackground(Qt::lightGray);

    date1 = new QDate (calendar->yearShown(), calendar->monthShown(), 1);
    days = date1->daysInMonth();

    for (i = 1; i <= days; i++) {//loop over the dates of this month
        date = new QDate (calendar->yearShown(), calendar->monthShown(), i);

        //uses the cached hasText flag instead of building a QTextDocument for
        //every day of the month on each keystroke
        if (note[i].hasText == true){//some note exists, color it lightgray
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
