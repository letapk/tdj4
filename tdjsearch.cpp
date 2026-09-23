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

void MainWindow::search_data()
{
int inivecflag = 0;

    //save_note();
    get_appointment_items ();

    write_journal_file(inivecflag);
    write_appt_file(inivecflag);

    srchresults->setPlainText(tr(""));

    srchtxt = searchtxtbox->text();
    if (srchtxt.isEmpty() == true) {
        statustext->setText(tr("Please enter text in search box"));
        return;
    }

    //a file that cannot be read (key mismatch, truncation, wrong store type)
    //is skipped so the rest can still be searched; the names are reported at
    //the end instead of silently dropping results
    srchcorruptfiles.clear();

    search_list_notes();
    srchresults->append(tr("\n"));
    search_list_appts();

    if (srchcorruptfiles.isEmpty() == true) {
        statustext->setText(tr("Search complete"));
    }
    else {
        srchresults->append(tr("Warning: the following files could not be "
                               "read and were skipped:\n%1")
                            .arg(srchcorruptfiles.join("\n")));
        statustext->setText(tr("Search complete - %n store(s) could not be read",
                               "", srchcorruptfiles.size()));
    }
}

void MainWindow::search_list_notes()
{
QDir dir;
QString path;
QStringList fltr, result;
QFileInfo fileInfo;
QFileInfoList list;


    fltr << tr("Notes*.tdj");
    dir.setNameFilters(fltr);
    dir.setFilter(QDir::Files);

    path.append (Homepath);

    dir.setPath(path);

    list = dir.entryInfoList();//list of files
    for (int i = 0; i < list.size(); ++i) {
        fileInfo = list.at(i);//file at position i in the list
        search_notes_file (fileInfo, &result);
    }

}

void MainWindow::search_notes_file(QFileInfo fi, QStringList *result)
{
QTextDocument doc;
QString s, s1, s2, s3, fname, Year, Month;
int i;

    fname = fi.baseName();//filename without path and extension : "Notes-xxxx-xx"
    Month = fname.sliced(11);//remove leading part : "Notes-xxxx-". Only the month stays
    Year = fi.baseName().sliced(6, 4);//the year part of the filename

    TdjEncryptedFile store(m_crypto);
    int openr = store.open(fi.filePath(), m_crypto.sessionKey(), Tdj2Notes);
    if (openr == TdjFieldCorrupt) {
        srchcorruptfiles.append(fi.fileName());//reported once, after both passes
        return;
    }
    if (openr != TdjFieldOk) {
        *result += tr("");//file does not exist
        return;
    }
    TdjFieldSource &fs = store.source();

    //a file that breaks mid-stream is recorded and dropped (its readable
    //prefix is not surfaced, so a truncated store cannot give partial hits)
    for (i = 1; i <= 31; i++) {
        QByteArray plain;

        qint32 len;
        int r = fs.readLen(len);
        if (r == TdjFieldEof)//file ends cleanly here
            break;
        if (r == TdjFieldCorrupt) {
            srchcorruptfiles.append(fi.fileName());
            break;
        }
        if (len == 0)//no note for this day
            continue;

        if (fs.readBody(len, plain) == TdjFieldCorrupt) {
            srchcorruptfiles.append(fi.fileName());
            break;
        }

        doc.setHtml(QString::fromUtf8(plain));
        s = doc.toPlainText();

        if (s.contains(srchtxt, Qt::CaseInsensitive) == true) {
            s1 = get_month_name(Month.toInt());
            s2.setNum(i);
            s3 = QString (tr("Found in the note for %1 %2, %3")).arg(s1).arg(s2).arg(Year);
            result->append(s3);
            srchresults->append(s3);
        }
    }
}

void MainWindow::search_list_appts()
{
QDir dir;
QString path;
QStringList fltr, result;
QFileInfo fileInfo;
QFileInfoList list;

    fltr << tr("Appointments*.tdj");
    dir.setNameFilters(fltr);
    dir.setFilter(QDir::Files);

    path.append (Homepath);

    dir.setPath(path);

    list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        fileInfo = list.at(i);
        search_appts_file (fileInfo, &result);
    }

}

void MainWindow::search_appts_file(QFileInfo fi, QStringList *result)
{
QString s, s1, s2, s3, fname, Year, Month;
bool corrupt = false;
int i, row;

    fname = fi.baseName();//filename without path and extension : "Appointments-xxxx-xx"
    Month = fname.sliced(18);//remove leading part : "Appointments-xxxx-". Only the month stays
    Year = fi.baseName().sliced(13, 4);//the year part of the filename

    TdjEncryptedFile store(m_crypto);
    int openr = store.open(fi.filePath(), m_crypto.sessionKey(),
                           Tdj2Appointments);
    if (openr == TdjFieldCorrupt) {
        srchcorruptfiles.append(fi.fileName());//reported once, after both passes
        return;
    }
    if (openr != TdjFieldOk)
        return;//file does not exist
    TdjFieldSource &fs = store.source();

    //a truncated/mixed store is recorded and dropped whole - no partial hits
    for (i = 1; i <= 31 && !corrupt; i++) {
         for(row = 0; row < 48 && !corrupt; row++) {
             QByteArray time, desc;

             qint32 len;
             int r = fs.readLen(len);
             if (r == TdjFieldEof || r == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }
             if (fs.readBody(len, time) == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }

             r = fs.readLen(len);
             if (r == TdjFieldEof || r == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }
             if (fs.readBody(len, desc) == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }

             if (QString::fromUtf8(desc).contains(srchtxt, Qt::CaseInsensitive) == true) {
                 s1 = get_month_name(Month.toInt());
                 s2.setNum(i);
                 s3 = QString (tr("Found in the appointments for %1 %2, %3")).arg(s1).arg(s2).arg(Year);
                 result->append(s3);
                 srchresults->append(s3);
             }
         }
    }
    if (corrupt == true)
        srchcorruptfiles.append(fi.fileName());
}
