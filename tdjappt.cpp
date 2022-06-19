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

extern Appointment appointment[], dailyappt;

void compact_appointments (int j);
void sort_appointments (int j);
int appt_compare (const void *a, const void *b);

void MainWindow::fill_appointment_items ()
//assigns the data read in from file to the appointments table items
{
QString s;
int row, i, j, k;

    j = -1;//in case there are no daily appointments
    for (row = 0; row < 48; row++) {
        //find the size of this daily appointment
        i = dailyappt.apptdesc[row+1].size() + dailyappt.apptime[row+1].size();
        if (i > 0) {//somethng there, so display it
            appcol0[row].setCheckState(Qt::Checked);
            appcol0[row].setText (dailyappt.apptime[row+1]);
            appcol1[row].setText (dailyappt.apptdesc[row+1]);
            j = row;
        }
        else {//clear this row
            appcol0[row].setCheckState(Qt::Unchecked);
            s.clear();
            appcol0[row].setText (s);
            appcol1[row].setText (s);
        }
    }
    k = 1;//index of single appointment
    //daily appointments filled upto j-th row
    //j+1 is the index of the first blank row in the table
    //this is 0 when j=-1
    for (row = j+1; row < 48; row++) {
        i = appointment[date_to_show].apptdesc[k].size() + appointment[date_to_show].apptime[k].size();
        if (i > 0){
            appcol0[row].setCheckState(Qt::Unchecked);
            appcol0[row].setText (appointment[date_to_show].apptime[k]);
            appcol1[row].setText (appointment[date_to_show].apptdesc[k]);
            k++;
        }
    }
    sort_appointments ();
}

void MainWindow::sort_appointments(void)
{
//this contains the array of appointments for this day
Tablerow approw[49];
int row, i, j;

    //transfer the appointments table rows to the array
    for (row = 0; row < 48; row++) {
        //first clear it, since it is a temporary array
        approw[row+1].col0.clear();
        approw[row+1].col1.clear();
        //copy the appointment items
        approw[row+1].col0 = appcol0[row].text();
        approw[row+1].col1 = appcol1[row].text();

        if (appcol0[row].checkState() == 0)
            approw[row+1].chkstate = false;
        else
            approw[row+1].chkstate = true;
    }

    //sort the array
    qsort (&(approw[1]), 48, sizeof (Tablerow), appt_compare);

    //copy the sorted array back to the appointment items
    //only if the element contains something
    row = 0;
    for (i = 1; i <= 48; i++) {
        //check the size of the data
        j = approw[i].col0.size() + approw[i].col0.size();
        if (j > 0) {//something there, so copy it to the table item
            appcol0[row].setText (approw[i].col0);
            appcol1[row].setText (approw[i].col1);

            if (approw[i].chkstate == false)
                appcol0[row].setCheckState(Qt::Unchecked);
            else
                appcol0[row].setCheckState(Qt::Checked);

            //next table item
            row++;
        }
    }
}

int appt_compare (const void *a, const void *b)
{
Tablerow *c, *d;
int ctime, dtime;
bool ok;

    c = (Tablerow *) a;
    d = (Tablerow *) b;

    //time of appointment
    ctime = c->col0.toInt(&ok, 10);
    dtime = d->col0.toInt(&ok, 10);

    if (ctime < dtime)
        return -1;
    if (ctime > dtime)
        return 1;

    return 0;
}

void MainWindow::get_appointment_items ()
//assigns the data from the table items to the appointment class which will be saved to file
{
int row;

    for (row = 0; row < 48; row++) {
        if (appcol0[row].checkState() == Qt::Checked) {//this appointment repeats daily
            dailyappt.apptime[row+1] = appcol0[row].text();
            dailyappt.apptdesc[row+1] = appcol1[row].text();

            appointment[date_to_show].apptime[row+1].clear();
            appointment[date_to_show].apptdesc[row+1].clear();
        }
        else {//this appointment is only for today
            appointment[date_to_show].apptime[row+1] = appcol0[row].text();
            appointment[date_to_show].apptdesc[row+1] = appcol1[row].text();

            dailyappt.apptime[row+1].clear();
            dailyappt.apptdesc[row+1].clear();
        }
    }
    compact_appointments (date_to_show);
}

void compact_appointments(int j)
{
Appointment spare;
int row, sparei, i;

    //copy only the non-zero appointments to the spare,
    //so that all the blank ones are at the end
    sparei = 1;
    for (row = 0; row < 48; row++) {
        spare.apptime[row+1].clear();
        spare.apptdesc[row+1].clear();

        i = appointment[j].apptime[row+1].size() + appointment[j].apptdesc[row+1].size();
        if (i > 0) {
            spare.apptdesc[sparei].append(appointment[j].apptdesc[row+1]);
            spare.apptime[sparei].append(appointment[j].apptime[row+1]);
            sparei++;
            //clear this appointment
            appointment[j].apptime[row+1].clear();
            appointment[j].apptdesc[row+1].clear();
         }
    }

    //copy the spare array back to the appointments array
    for (i = 1; i < sparei; i++) {
        appointment[j].apptime[i].append(spare.apptime[i]);
        appointment[j].apptdesc[i].append(spare.apptdesc[i]);
    }

    //now do the same for the daily appointments

    sparei = 1;
    for (row = 0; row < 48; row++) {
        spare.apptime[row+1].clear();
        spare.apptdesc[row+1].clear();

        i = dailyappt.apptime[row+1].size() + dailyappt.apptdesc[row+1].size();
        if (i > 0) {
            spare.apptdesc[sparei].append(dailyappt.apptdesc[row+1]);
            spare.apptime[sparei].append(dailyappt.apptime[row+1]);
            sparei++;
            //clear this appointment
            dailyappt.apptime[row+1].clear();
            dailyappt.apptdesc[row+1].clear();
         }
    }

    for (i = 1; i < sparei; i++) {
        dailyappt.apptime[i].append(spare.apptime[i]);
        dailyappt.apptdesc[i].append(spare.apptdesc[i]);
    }
}

void MainWindow::save_appt_cell (QTableWidgetItem *it)
{
int row, col;
QString s;
bool ok = true;

    //row = apptable->currentRow();
    row = it->row();
    //col = apptable->currentColumn();
    col = it->column();

    if (col == -1 || row == -1)
        return;

    s.clear();
    s = it->text();

    if (s.size() != 0) {
        if (col == 0) {//check contents
            check_appt_time (s, &ok);
            if (ok == true) {//string is a valid time between 0 and 2400
                appcol0[row].setText(s);
                s.clear();
            }
            else {//invalid string
                appcol0[row].setText(tr("Error!"));
                statustext->setText(tr("The time entered in the first column is incorrect"));
                s.clear();
            }
        }
        else if (col == 1){//col 1 - details
            appcol1[row].setText(s);
            s.clear();
        }
    }
    if (current_year == year && current_month == month && current_date == date_to_show){
        //the appt is today, rewrite array for setting appt alarms
        set_appt_time_array();
        set_next_appointment_timer ();
    }
}

void MainWindow::check_appt_time (QString s, bool *ok)
{
bool ok2;
int i;

    i = s.toInt(&ok2, 10);
    if (ok2 == true) {//text is a number
        if (i >= 0 && i <=2400)//valid time
            *ok = true;
        else//invalid time
            *ok = false;
    }
    else {//text is not a number
        *ok = false;
    }
}

void MainWindow::set_next_appointment_timer ()
{
QTime now;
int mininterval = 2400, interval = 0;
int row;
bool setalarm = false;

    now = QTime::currentTime();

    for (row = 0; row < 48; row++) {
        interval = appt_time_array[row] - (now.hour()*100+now.minute());//interval to this appt
        if (interval > 5) {//time is in the future, and greater than 5 min
            //find the least interval
            mininterval = (mininterval < interval) ? mininterval : interval;
            setalarm = true;
        }
    }

    if (setalarm == true) {
        mininterval -= 5;//less 5 min from least interval
        //issue timer to call alarm function
        QTimer::singleShot(mininterval*60*1000, this, SLOT(issue_appt_alarm()));
    }
}

void MainWindow::issue_appt_alarm()
{
QString s;
QMessageBox msgBox;

    statustext->setText(tr("There is an upcoming appointment in 5 minutes!"));

    QApplication::beep();

    s.append (tr("There is an upcoming appointment in 5 minutes"));
    msgBox.setText(s);
    msgBox.setWindowModality(Qt::ApplicationModal);
    msgBox.exec();

    set_next_appointment_timer();
}

void MainWindow::set_appt_time_array()
{
int row;

    for (row = 0; row < 48; row++) {
        appt_time_array[row] = appcol0[row].text().toInt();
    }
}
