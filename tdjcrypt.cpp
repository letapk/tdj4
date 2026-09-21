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

//Store read/write for the MainWindow data models (Phase 3, v4-0.4).
//
//The binary layouts, the codec, the container and every key/salt/IV now live
//in the Qt-Core-only storage layer (tdjstore.h / tdjstore.cpp). The methods
//here only walk the in-memory models, build the plaintext field stream with
//tdj_append_field(), and hand it to TdjEncryptedFile - the GUI never touches
//a cipher handle, key buffer, salt or magic byte directly. All crypto state
//is owned by CryptoManager MainWindow::m_crypto.

#include "tdj.h"
#include <QTextDocument>
#include <QTreeWidgetItem>

extern Note note[];
extern Appointment appointment[], dailyappt;
extern Anniversary anniversary [];
//used while reencrypting data files due to a change in password
extern QFileInfo reencfileInfo;

void crypt_error_notification (const char *errstr)
{
QString s;

    s.append (errstr);

    QMessageBox msgBox;
    msgBox.setText (s);
    msgBox.exec();
}

void MainWindow::read_journal_file (int inivecflag, const QString &pathOverride)
{
int i;

    //clear all notes
    for (i = 1; i < 32; i++) {
        note[i].data.clear();
        note[i].hasText = false;
    }

    //the caller may name an explicit file to read (export); otherwise an
    //in-progress password change reads the next file from the list. The key is
    //chosen from inivecflag alone, so reading a listed file with the current
    //session key (export) must pass 0 here.
    QString path = Notefilename;
    if (pathOverride.isEmpty() == false) {
        path = pathOverride;
    }
    else if (inivecflag == 1) {//pwd change in progress, filename comes from file list
        path = Homepath + "/" + reencfileInfo.fileName();
    }

    TdjEncryptedFile store(m_crypto);
    int r = store.open(path, m_crypto.readKey32(inivecflag));
    if (r == TdjFieldEof)
        return;//file does not exist: nothing to read
    if (r != TdjFieldOk) {
        crypt_error_notification ("Error in reading journal data.");
        return;
    }
    TdjFieldSource &fs = store.source();

    if (fs.format == 2) {
        //legacy journal: all 31 day lengths are stored first as plain ints,
        //then the (decrypted) body of each non-empty day in day order
        qint32 lens[32] = {};
        for (i = 1; i <= 31; i++) {
            int r2 = fs.readLen(lens[i]);
            if (r2 == TdjFieldEof)//file ends cleanly here, remaining days empty
                break;
            if (r2 == TdjFieldCorrupt) {
                crypt_error_notification ("Error in reading journal data.");
                return;
            }
        }
        for (i = 1; i <= 31; i++) {
            if (lens[i] == 0)//no note for this day
                continue;
            QByteArray plain;
            if (fs.readBody(lens[i], plain) == TdjFieldCorrupt) {
                crypt_error_notification ("Error in reading journal data.");
                return;
            }
            note[i].data = QString::fromUtf8(plain);

            //cache the plain-text emptiness flag for the calendar colours
            QTextDocument doc;
            doc.setHtml(note[i].data);
            note[i].hasText = !doc.toPlainText().isEmpty();
        }
        return;
    }

    //TDJ2: interleaved [len][body] forward stream, one field per day
    for (i = 1; i <= 31; i++) {
        qint32 len;
        int r2 = fs.readLen(len);
        if (r2 == TdjFieldEof)//file ends cleanly here, remaining days are empty
            break;
        if (r2 == TdjFieldCorrupt) {
            crypt_error_notification ("Error in reading journal data.");
            break;
        }
        if (len == 0)//no note for this day
            continue;

        QByteArray plain;
        r2 = fs.readBody(len, plain);
        if (r2 == TdjFieldCorrupt) {
            crypt_error_notification ("Error in reading journal data.");
            break;
        }

        note[i].data = QString::fromUtf8(plain);

        //cache the plain-text emptiness flag for the calendar colours
        QTextDocument doc;
        doc.setHtml(note[i].data);
        note[i].hasText = !doc.toPlainText().isEmpty();
    }
}

void MainWindow::write_journal_file (int inivecflag)
{
QByteArray body;
int i;

    for (i = 1; i <= 31; i++) {
        QByteArray utf8 = note[i].data.toUtf8();
        if (utf8.isEmpty()) {//write an empty length prefix, no data
            tdj_append_field(body, QByteArray());
            continue;
        }
        tdj_append_field(body, utf8);
    }

    QString target = Notefilename;
    if (inivecflag == 1)//pwd change / migration: write the re-encrypted copy
        target = Homepath + "/" + reencfileInfo.fileName() + ".new";
    else {//normal save: a month with no notes leaves no file behind
        bool any = false;
        for (i = 1; i <= 31; i++) {
            if (note[i].hasText) {
                any = true;
                break;
            }
        }
        if (!any) {
            QFile::remove(target);
            return;
        }
    }

    if (TdjEncryptedFile::write(m_crypto, target, Tdj2Notes, body, inivecflag)
        == false)
        crypt_error_notification ("Error in writing journal data.");
}

void MainWindow::read_appt_file (int inivecflag)
{
int i, row;

    //clear all appointments
    for (i = 1; i <= 31; i++) {
         for(row = 0; row < 48; row++) {
             appointment[i].apptime[row+1].clear();
             appointment[i].apptdesc[row+1].clear();
         }
         appointment[i].total = 0;
    }

    QString path = Appointmentsfilename;
    if (inivecflag == 1) {//pwd change in progress, filename comes from file list
        path = Homepath + "/" + reencfileInfo.fileName();
    }

    TdjEncryptedFile store(m_crypto);
    int r = store.open(path, m_crypto.readKey32(inivecflag));
    if (r == TdjFieldEof)
        return;//file does not exist: nothing to read
    if (r != TdjFieldOk) {
        crypt_error_notification ("Error in reading appointments data.");
        return;
    }
    TdjFieldSource &fs = store.source();

    bool corrupt = false;
    for (i = 1; i <= 31 && !corrupt; i++) {
         for(row = 0; row < 48 && !corrupt; row++) {
             QByteArray time, desc;

             qint32 len;
             int k = fs.readLen(len);
             if (k == TdjFieldEof || k == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }
             if (fs.readBody(len, time) == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }

             k = fs.readLen(len);
             if (k == TdjFieldEof || k == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }
             if (fs.readBody(len, desc) == TdjFieldCorrupt) {
                 corrupt = true;
                 break;
             }

             appointment[i].apptime[row+1] = QString::fromUtf8(time);
             appointment[i].apptdesc[row+1] = QString::fromUtf8(desc);

             if (appointment[i].apptime[row+1].size() + appointment[i].apptdesc[row+1].size() > 0)
                 (appointment[i].total)++;
         }
    }

    if (corrupt)
        crypt_error_notification ("Error in reading appointments data.");
}

void MainWindow::write_appt_file (int inivecflag)
{
QByteArray body;
int i, row;

    for (i = 1; i <= 31; i++) {
        for (row = 0; row < 48; row++) {
            QByteArray time = appointment[i].apptime[row+1].toUtf8();
            QByteArray desc = appointment[i].apptdesc[row+1].toUtf8();

            tdj_append_field(body, time);
            tdj_append_field(body, desc);
        }
    }

    QString target = Appointmentsfilename;
    if (inivecflag == 1)
        target = Homepath + "/" + reencfileInfo.fileName() + ".new";
    else {//normal save: a month with no appointments leaves no file behind
        bool any = false;
        for (i = 1; i <= 31 && !any; i++) {
            for (row = 0; row < 48 && !any; row++)
                any = !appointment[i].apptime[row+1].isEmpty()
                   || !appointment[i].apptdesc[row+1].isEmpty();
        }
        if (!any) {
            QFile::remove(target);
            return;
        }
    }

    if (TdjEncryptedFile::write(m_crypto, target, Tdj2Appointments, body,
                                inivecflag) == false)
        crypt_error_notification ("Error in writing appointments data.");
}

void MainWindow::read_daily_appt_file ()
{
int row;

    //clear all appointments
    for(row = 0; row < 48; row++) {
        dailyappt.apptime[row+1].clear();
        dailyappt.apptdesc[row+1].clear();
    }
    dailyappt.total = 0;

    TdjEncryptedFile store(m_crypto);
    int r = store.open(DailyAppointmentsfilename, m_crypto.readKey32(0));
    if (r == TdjFieldEof)
        return;//file does not exist: nothing to read
    if (r != TdjFieldOk) {
        crypt_error_notification ("Error in reading daily appointments data.");
        return;
    }
    TdjFieldSource &fs = store.source();

    bool corrupt = false;
    for(row = 0; row < 48 && !corrupt; row++) {
        QByteArray time, desc;

        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, time) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, desc) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        dailyappt.apptime[row+1] = QString::fromUtf8(time);
        dailyappt.apptdesc[row+1] = QString::fromUtf8(desc);

        if (dailyappt.apptime[row+1].size() + dailyappt.apptdesc[row+1].size() > 0)
            (dailyappt.total)++;
    }

    if (corrupt)
        crypt_error_notification ("Error in reading daily appointments data.");
}

void MainWindow::write_daily_appt_file (int inivecflag)
{
QByteArray body;
int row;

    for (row = 0; row < 48; row++) {
        QByteArray time = dailyappt.apptime[row+1].toUtf8();
        QByteArray desc = dailyappt.apptdesc[row+1].toUtf8();

        tdj_append_field(body, time);
        tdj_append_field(body, desc);
    }

    QString target = DailyAppointmentsfilename;
    if (inivecflag == 1)
        target = Homepath + "/" + reencfileInfo.fileName() + ".new";
    else {//normal save: no daily appointments leaves no file behind
        bool any = false;
        for (row = 0; row < 48 && !any; row++)
            any = !dailyappt.apptime[row+1].isEmpty()
               || !dailyappt.apptdesc[row+1].isEmpty();
        if (!any) {
            QFile::remove(target);
            return;
        }
    }

    if (TdjEncryptedFile::write(m_crypto, target, Tdj2DailyAppts, body,
                                inivecflag) == false)
        crypt_error_notification ("Error in writing daily appointments data.");
}

void MainWindow::read_ann_file ()
{
int i;

    max_anniversaries = 0;

    TdjEncryptedFile store(m_crypto);
    int r = store.open(Anniversaryfilename, m_crypto.readKey32(0));
    if (r == TdjFieldEof)
        return;//file does not exist: nothing to read
    if (r != TdjFieldOk) {
        crypt_error_notification ("Error in reading anniversary data.");
        return;
    }
    TdjFieldSource &fs = store.source();

    for (i = 1; i < 367; i++) {
        anniversary[i].date.clear();
        anniversary[i].month.clear();
        anniversary[i].description.clear();
    }

    bool corrupt = false;
    //read the holidays
    for (i = 1; i < 367 && !corrupt; i++) {
        QByteArray date, month, desc;

        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, date) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, month) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, desc) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        anniversary[i].date = QString::fromUtf8(date);
        anniversary[i].month = QString::fromUtf8(month);
        anniversary[i].description = QString::fromUtf8(desc);

        if (anniversary[i].description.size() > 0)
            max_anniversaries++;
    }

    if (corrupt)
        crypt_error_notification ("Error in reading anniversary data.");
}

void MainWindow::write_ann_file (int inivecflag)
{
QByteArray body;
int i;

    for (i = 1; i < 367; i++) {
        QByteArray date = anniversary[i].date.toUtf8();
        QByteArray month = anniversary[i].month.toUtf8();
        QByteArray desc = anniversary[i].description.toUtf8();

        tdj_append_field(body, date);
        tdj_append_field(body, month);
        tdj_append_field(body, desc);
    }

    QString target = Anniversaryfilename;
    if (inivecflag == 1)
        target = Homepath + "/" + reencfileInfo.fileName() + ".new";

    if (TdjEncryptedFile::write(m_crypto, target, Tdj2Anns, body,
                                inivecflag) == false)
        crypt_error_notification ("Error in writing anniversary data.");
}

void MainWindow::read_contacts ()
{
QTreeWidgetItem *itcat, *itcon;
QString s;
QTextDocument doc;
int i, j, contact_count, toplevelcount;

    contree->clear();

    TdjEncryptedFile store(m_crypto);
    int r = store.open(Contactfilename, m_crypto.readKey32(0));
    if (r == TdjFieldEof)
        return;//file does not exist: nothing to read
    if (r != TdjFieldOk) {
        crypt_error_notification ("Error in reading contacts data.");
        return;
    }
    TdjFieldSource &fs = store.source();

    //number of groups (a plain int in both formats)
    if (fs.readRaw(reinterpret_cast<char *>(&toplevelcount), 4) != 1) {
        crypt_error_notification ("Error in reading contacts data.");
        return;
    }
    if (toplevelcount < 0 || toplevelcount > kMaxFieldBytes) {
        crypt_error_notification ("Error in reading contacts data.");
        return;
    }

    bool corrupt = false;
    //loop over groups
    for (i = 0; i < toplevelcount && !corrupt; i++) {
        itcat = new QTreeWidgetItem (contree);
        contree->addTopLevelItem(itcat);

        QByteArray name, gdesc;

        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, name) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, gdesc) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        //assign the group name to the tree, stripped of HTML
        doc.setHtml(QString::fromUtf8(name));
        s = doc.toPlainText();
        itcat->setText(0, s);
        itcat->setText(1, QString::fromUtf8(gdesc));

        if (fs.readRaw(reinterpret_cast<char *>(&contact_count), 4) != 1) {
            corrupt = true;
            break;
        }
        if (contact_count < 0 || contact_count > kMaxFieldBytes){
            corrupt = true;
            break;
        }

        //loop over contacts for this group
        for (j = 0; j < contact_count && !corrupt; j++){
            itcon = new QTreeWidgetItem ();

            QByteArray cname, cdata;

            k = fs.readLen(len);
            if (k == TdjFieldEof || k == TdjFieldCorrupt) {
                corrupt = true;
                break;
            }
            if (fs.readBody(len, cname) == TdjFieldCorrupt) {
                corrupt = true;
                break;
            }

            k = fs.readLen(len);
            if (k == TdjFieldEof || k == TdjFieldCorrupt) {
                corrupt = true;
                break;
            }
            if (fs.readBody(len, cdata) == TdjFieldCorrupt) {
                corrupt = true;
                break;
            }

            itcon->setText(0, QString::fromUtf8(cname));
            itcon->setText(1, QString::fromUtf8(cdata));

            itcat->addChild(itcon);
        }
    }

    if (corrupt)
        crypt_error_notification ("Error in reading contacts data.");

    if (corrupt == false && toplevelcount > 0)
        contreeempty = false;
}

void MainWindow::write_contacts (int inivecflag)
{
QTreeWidgetItem *itcat, *itcon;
QByteArray body;
int i, j, toplevelcount, contact_count;

    //number of contact groups
    toplevelcount = contree->topLevelItemCount();

    body.append(reinterpret_cast<const char *>(&toplevelcount), 4);

    //loop over groups
    for (i = 0; i < toplevelcount; i++){
        itcat = contree->topLevelItem(i);
        //number of contacts in this group
        contact_count = itcat->childCount();

        QByteArray name = itcat->text(0).toUtf8();
        QByteArray gdesc = itcat->text(1).toUtf8();

        tdj_append_field(body, name);
        tdj_append_field(body, gdesc);

        body.append(reinterpret_cast<const char *>(&contact_count), 4);

        for (j = 0; j < contact_count; j++){
            itcon = itcat->child(j);

            QByteArray cname = itcon->text(0).toUtf8();
            QByteArray cdata = itcon->text(1).toUtf8();

            tdj_append_field(body, cname);
            tdj_append_field(body, cdata);
        }
    }

    QString target = Contactfilename;
    if (inivecflag == 1)
        target = Homepath + "/" + reencfileInfo.fileName() + ".new";

    if (TdjEncryptedFile::write(m_crypto, target, Tdj2Contacts, body,
                                inivecflag) == false)
        crypt_error_notification ("Error in writing contacts data.");
}

void MainWindow::read_lists()
{
QTreeWidgetItem *it;
int i, toplevelcount;

    TdjEncryptedFile store(m_crypto);
    int r = store.open(Listfilename, m_crypto.readKey32(0));
    if (r == TdjFieldEof)
        return;//file does not exist: nothing to read
    if (r != TdjFieldOk) {
        crypt_error_notification ("Error in reading notes data.");
        return;
    }
    TdjFieldSource &fs = store.source();

    //number of lists (a plain int in both formats)
    if (fs.readRaw(reinterpret_cast<char *>(&toplevelcount), 4) != 1) {
        crypt_error_notification ("Error in reading notes data.");
        return;
    }
    if (toplevelcount < 0 || toplevelcount > kMaxFieldBytes) {
        crypt_error_notification ("Error in reading notes data.");
        return;
    }

    bool corrupt = false;
    //loop over lists
    for (i = 0; i < toplevelcount && !corrupt; i++) {
        it = new QTreeWidgetItem (listree);
        listree->addTopLevelItem(it);

        QByteArray name, data;

        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, name) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }
        if (fs.readBody(len, data) == TdjFieldCorrupt) {
            corrupt = true;
            break;
        }

        it->setText(0, QString::fromUtf8(name));
        it->setText(1, QString::fromUtf8(data));
    }

    if (corrupt)
        crypt_error_notification ("Error in reading notes data.");

    if (corrupt == false && toplevelcount > 0)
        listreeempty = false;
}

void MainWindow::write_lists(int inivecflag)
{
QTreeWidgetItem *it;
QByteArray body;
int i, toplevelcount;

    //number of lists
    toplevelcount = listree->topLevelItemCount();

    body.append(reinterpret_cast<const char *>(&toplevelcount), 4);

    //loop over lists
    for (i = 0; i < toplevelcount; i++) {
        it = listree->topLevelItem(i);

        QByteArray name = it->text(0).toUtf8();
        QByteArray data = it->text(1).toUtf8();

        tdj_append_field(body, name);
        tdj_append_field(body, data);
    }

    QString target = Listfilename;
    if (inivecflag == 1)
        target = Homepath + "/" + reencfileInfo.fileName() + ".new";

    if (TdjEncryptedFile::write(m_crypto, target, Tdj2Lists, body,
                                inivecflag) == false)
        crypt_error_notification ("Error in writing notes data.");
}