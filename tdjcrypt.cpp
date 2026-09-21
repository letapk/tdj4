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

//Thin MainWindow store I/O (Phase 3, v4-0.5).
//
//The data models, the binary layouts, the codec, the container and every
//key/salt/IV live in the Qt-Core-only storage layer (tdjstore.h / tdjstore.cpp,
//class StorageManager). The methods here only bind MainWindow's widgets to
//StorageManager and translate its result codes into user-facing messages;
//the GUI never touches a field layout, length prefix, magic byte, or cipher
//handle. Re-encryption (migration / password change) calls StorageManager
//directly with explicit paths and never routes through these wrappers.

#include "tdj.h"
#include <QTextDocument>
#include <QTreeWidgetItem>

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
    //the caller may name an explicit file to read (export); otherwise the
    //displayed month's file is used. The key is chosen from inivecflag alone,
    //so reading a listed file with the current session key (export) passes 0.
    QString path = Notefilename;
    if (pathOverride.isEmpty() == false)
        path = pathOverride;

    if (m_store.loadJournal(m_crypto, path, inivecflag) == TdjFieldCorrupt)
        crypt_error_notification ("Error in reading journal data.");
}

void MainWindow::write_journal_file (int inivecflag)
{
    QString target = Notefilename;
    if (inivecflag == 1)//pwd change / migration target the re-encrypted copy
        target += ".new";

    if (m_store.saveJournal(m_crypto, target, inivecflag) == false)
        crypt_error_notification ("Error in writing journal data.");
}

void MainWindow::read_appt_file (int inivecflag)
{
    if (m_store.loadApptMonth(m_crypto, Appointmentsfilename, inivecflag)
        == TdjFieldCorrupt)
        crypt_error_notification ("Error in reading appointments data.");
}

void MainWindow::write_appt_file (int inivecflag)
{
    QString target = Appointmentsfilename;
    if (inivecflag == 1)
        target += ".new";

    if (m_store.saveApptMonth(m_crypto, target, inivecflag) == false)
        crypt_error_notification ("Error in writing appointments data.");
}

void MainWindow::read_daily_appt_file ()
{
    if (m_store.loadDailyApps(m_crypto, DailyAppointmentsfilename)
        == TdjFieldCorrupt)
        crypt_error_notification ("Error in reading daily appointments data.");
}

void MainWindow::write_daily_appt_file (int inivecflag)
{
    QString target = DailyAppointmentsfilename;
    if (inivecflag == 1)
        target += ".new";

    if (m_store.saveDailyApps(m_crypto, target, inivecflag) == false)
        crypt_error_notification ("Error in writing daily appointments data.");
}

void MainWindow::read_ann_file ()
{
    if (m_store.loadAnns(m_crypto, Anniversaryfilename) == TdjFieldCorrupt)
        crypt_error_notification ("Error in reading anniversary data.");
}

void MainWindow::write_ann_file (int inivecflag)
{
    QString target = Anniversaryfilename;
    if (inivecflag == 1)
        target += ".new";

    if (m_store.saveAnns(m_crypto, target, inivecflag) == false)
        crypt_error_notification ("Error in writing anniversary data.");
}

void MainWindow::read_contacts ()
{
QTreeWidgetItem *itcat, *itcon;
QTextDocument doc;
QVector<TdjGroup> groups;

    contree->clear();

    int r = m_store.loadContacts(m_crypto, Contactfilename, groups);
    if (r == TdjFieldCorrupt) {
        crypt_error_notification ("Error in reading contacts data.");
        return;
    }
    if (r == TdjFieldEof)
        return;//file does not exist: there are no contacts

    for (const TdjGroup &g : groups) {
        itcat = new QTreeWidgetItem (contree);
        contree->addTopLevelItem(itcat);

        //assign the group name stripped of HTML, the description verbatim
        doc.setHtml(g.name);
        itcat->setText(0, doc.toPlainText());
        itcat->setText(1, g.desc);

        for (const TdjItem &it : g.items) {
            itcon = new QTreeWidgetItem ();
            itcon->setText(0, it.name);
            itcon->setText(1, it.data);
            itcat->addChild(itcon);
        }
    }

    if (!groups.isEmpty())
        contreeempty = false;
}

void MainWindow::write_contacts (int inivecflag)
{
QVector<TdjGroup> groups;
int i, j;

    for (i = 0; i < contree->topLevelItemCount(); i++) {
        const QTreeWidgetItem *itcat = contree->topLevelItem(i);

        TdjGroup g;
        g.name = itcat->text(0);
        g.desc = itcat->text(1);
        for (j = 0; j < itcat->childCount(); j++) {
            const QTreeWidgetItem *itcon = itcat->child(j);
            g.items.append(TdjItem { itcon->text(0), itcon->text(1) });
        }
        groups.append(g);
    }

    QString target = Contactfilename;
    if (inivecflag == 1)
        target += ".new";

    if (m_store.saveContacts(m_crypto, target, inivecflag, groups) == false)
        crypt_error_notification ("Error in writing contacts data.");
}

void MainWindow::read_lists()
{
QTreeWidgetItem *it;
QVector<TdjList> lists;

    listree->clear();

    int r = m_store.loadLists(m_crypto, Listfilename, lists);
    if (r == TdjFieldCorrupt) {
        crypt_error_notification ("Error in reading notes data.");
        return;
    }
    if (r == TdjFieldEof)
        return;//file does not exist: there are no lists

    for (const TdjList &l : lists) {
        it = new QTreeWidgetItem (listree);
        listree->addTopLevelItem(it);
        it->setText(0, l.name);
        it->setText(1, l.data);
    }

    if (!lists.isEmpty())
        listreeempty = false;
}

void MainWindow::write_lists(int inivecflag)
{
QVector<TdjList> lists;
int i;

    for (i = 0; i < listree->topLevelItemCount(); i++) {
        const QTreeWidgetItem *it = listree->topLevelItem(i);
        lists.append(TdjList { it->text(0), it->text(1) });
    }

    QString target = Listfilename;
    if (inivecflag == 1)
        target += ".new";

    if (m_store.saveLists(m_crypto, target, inivecflag, lists) == false)
        crypt_error_notification ("Error in writing notes data.");
}