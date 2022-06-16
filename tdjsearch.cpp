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

//Last modified 16 September 2014

#include "tdj.h"

extern char iniVector[];//initialization vector
extern size_t blkLength;
extern size_t txtLength; // string plus termination
extern size_t txtLengthpadded; // string plus padded zero characters

extern int padding, tdremainder;
extern char *toencrypt, *encrypted, *todecrypt, *decrypted;
extern FILE *encf;

extern int aesenc (void);
extern int aesdec (void);
extern void crypt_error_notification (const char *errstr);
extern void set_decrypt_variables(void);
extern void free_dec_strings (void);

extern QMessageBox *msgBox;

extern size_t szt;

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

    search_list_notes();
    srchresults->append(tr("\n"));
    search_list_appts();

    statustext->setText(tr("Search complete"));
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
QTextDocument *doc;
QString s, s1, s2, s3, fname, Year, Month;
int i, j[32], k;

    encf = fopen (fi.filePath().toUtf8().data(), "r");

    if (encf == NULL) {
        *result += tr("");
        return;
    }

    for (i = 1; i <= 31; i++) {
        j[i] = 0;
    }

    fname = fi.baseName();//filename without path and extension : "Notes-xxxx-xx"
    Month = fname.remove (0, 11);//remove leading part : "Notes-xxxx-". Only "xx" remains

    fname = fi.baseName();//filename without path and extension : "Notes-xxxx-xx"
    Year = fname.remove(0, 6);//remove leading part of Notefilename : "Notes-". Only "xxxx-xx" remains
    Year.truncate(4);//remove trailing part : "-xx". Only "xxxx" remains.

    szt = fread (&iniVector, sizeof (char), 16, encf);
    for (i = 1; i <= 31; i++) {//length of each note
        szt = fread (&(j[i]), sizeof (int), 1, encf);
    }


    for (i = 1; i <= 31; i++) {

        if (j[i] > 0) {
            txtLength = j[i];
            set_decrypt_variables ();
            //read the data to be decrypted
            szt = fread (todecrypt, txtLengthpadded, 1, encf);

            //decrypt the data and put it in decrypted
            k = aesdec ();
            if (k == 1) {
                crypt_error_notification ("Error in decryption of data.");
            }
            //put decrypted data into note for this day
            s.clear();
            s.append((const char *)decrypted);

            //free the buffers
            free_dec_strings ();

            doc = new QTextDocument ();
            doc->setHtml(s);
            s = doc->toPlainText();
            delete doc;

            if (s.contains(&srchtxt, Qt::CaseInsensitive) == true) {
                s1 = get_month_name(Month.toInt());
                s2.setNum(i);
                s3 = QString (tr("Found in the note for %1 %2, %3")).arg(s1).arg(s2).arg(Year);
                result->append(s3);
                srchresults->append(s3);
            }
        }
    }
    fclose (encf);
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
uint i, k, row;

    encf = fopen (fi.filePath().toUtf8().data(), "r");
    if (encf == NULL)
        return;

    fname = fi.baseName();//filename without path and extension : "Appointments-xxxx-xx"
    Month = fname.remove (0, 18);//remove leading part : "Appointments-xxxx-"

    fname = fi.baseName();//filename without path and extension : "Appointments-xxxx-xx".
    Year = fname.remove(0, 13);//remove leading part of Apptfilename : "Appointments-". Only "xxxx-xx" remains
    Year.truncate(4);//remove trailing part : "-xx". Only "xxxx" remains.

    szt = fread (&iniVector, sizeof (char), 16, encf);
    for (i = 1; i <= 31; i++) {
         for(row = 0; row < 48; row++) {
             //read and skip the time
             //read the size of the time
             szt = fread (&txtLength, sizeof (int), 1, encf);
             set_decrypt_variables();
             //read the encrypted time
             szt = fread (todecrypt, txtLengthpadded, 1, encf);

             //free the buffers
             free_dec_strings ();

             //read the size of the description
             szt = fread (&txtLength, sizeof (int), 1, encf);
             set_decrypt_variables();
             //read the encrypted description
             szt = fread (todecrypt, txtLengthpadded, 1, encf);

             k = aesdec ();
             if (k == 1) {
                 crypt_error_notification ("Error in decryption of appointments data.");
             }

             //assign the description to the appointment
             s.clear();
             s.append(decrypted);

             //free the buffers
             free_dec_strings ();

             if (s.contains(&srchtxt, Qt::CaseInsensitive) == true) {
                 s1 = get_month_name(Month.toInt());
                 s2.setNum(i);
                 s3 = QString (tr("Found in the appointments for %1 %2, %3")).arg(s1).arg(s2).arg(Year);
                 result->append(s3);
                 srchresults->append(s3);
             }
         }
    }

    fclose(encf);
}
