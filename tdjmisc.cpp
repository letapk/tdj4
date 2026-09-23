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

//Last modified 22 Sep 2026

#include "tdj.h"
#include <QFileDialog>
#include <QImageReader>
#include <QDesktopServices>
#include <QSaveFile>
#include <QTimer>
#include <QRegularExpression>
#include <QUrl>
#include <QTextBlock>
#include <QBuffer>
#include <QSignalBlocker>

extern void crypt_error_notification (const char *errstr);

extern QString Lockfilename, userpath;
extern bool setpwd;

void MainWindow::about()
//open a window to show program information and copyright license
{
QFile file(Gnugplfilename);
QTextStream in(&file);
QTextBrowser *gnugpl;
bool ok;

    ok = file.open(QFile::ReadOnly);
    if (ok == false)
        return;

    gnugpl = new QTextBrowser ();
    gnugpl->setAttribute(Qt::WA_DeleteOnClose);//free the window when closed
    gnugpl->setGeometry(10, 10, 800, 600);
    gnugpl->setWindowTitle (QObject::tr("About The Daily Journal"));
    gnugpl->setPlainText(in.readAll());
    gnugpl->setAlignment(Qt::AlignLeft);
    gnugpl->show();

}

void MainWindow::help()
//open and show the user manual
{
bool ok;
QString s1;
QMessageBox msgBox;

    QFile file(Helpfilename);
    ok = file.exists();
    if (ok == false) {
        s1 = QObject::tr("The help file was not found.");
        s1.append (QObject::tr("Please make sure that it is present in the hidden tdj data directory."));

        msgBox.setText(s1);
        msgBox.exec();
    }
    else {
        QDesktopServices::openUrl (QUrl (Helpfilename));
    }
}


bool check_lockfile (void)
{
QMessageBox msgBox;
QString s1, s2, s3;
bool ok = false;

    QFile file(Lockfilename);
    ok = file.open(QFile::ReadOnly);
    if (ok == true) {//lockfile present, close it and inform user
        file.close();

        s1 = QObject::tr("It seems that \"The Daily Journal\" is already running.");
        s2 = QObject::tr("If this is not the case, click \"Continue\", else click \"Abort\". ");
        s3 = QObject::tr("A lockfile has been found in the hidden tdj data-subdirectory. ");
        s3.append (QObject::tr("The program may be currently running in another terminal, in which case click \"Abort\". "));
        s3.append (QObject::tr("Alternatively, an earlier instance of the program may have failed to delete the lockfile. "));
        s3.append (QObject::tr("If you are sure that tdj is not running in your account, click \"Continue\". "));
        s3.append (QObject::tr("See the user manual about the risks of running two instances of the program at the same time."));

        msgBox.setText(s1);
        msgBox.setInformativeText(s2);
        msgBox.setDetailedText(s3);

        msgBox.addButton(QObject::tr("Continue"), QMessageBox::ApplyRole);
        msgBox.addButton(QObject::tr("Abort"), QMessageBox::RejectRole);

        int ret = msgBox.exec();
        switch (ret) {
        case QMessageBox::ApplyRole://continue
            file.remove();
            return true;
            break;
        case QMessageBox::RejectRole://abort
            return false;
            break;
        }
    }

    return true;//lockfile absent
}

bool create_lockfile ()
{
    QFile file(Lockfilename);
    if (file.open(QFile::WriteOnly) == false)
        return false;//caller must abort: without the lock a second instance could run
    file.close();
    return true;
}

void delete_lockfile ()
{
    QFile file(Lockfilename);
    file.remove();
}

void check_qtdata_dir ()
{
QString qtpath, s1;
QDir qtdir;

    qtpath.append (userpath);
    qtdir = QDir (qtpath);

    if (qtdir.exists() == false) {
        s1.append (QObject::tr("The tdj data directory does not exist. "));
        s1.append (QObject::tr("This is required to store your work.\n"));
        s1.append (QObject::tr("Click OK to create a new, hidden subdirectory "));
        s1.append (QObject::tr("in your area with the name :\n"));
        s1.append (qtpath);
        //a fresh box per prompt: one that was already exec()'d can re-show with
        //a duplicated OK button under some platform themes
        QMessageBox msgBox;
        msgBox.setText(s1);
        msgBox.exec();

        qtdir.mkdir(qtpath);

        s1.clear();
        s1.append (QObject::tr("The program needs a password to encrypt the data. "));
        s1.append (QObject::tr("This will not be stored anywhere, and thus should not be forgotten.\n"));
        s1.append (QObject::tr("Click OK to create a new password in the next step.\n"));
        QMessageBox pwdBox;
        pwdBox.setText(s1);
        pwdBox.exec();

        //ask for password
        setpwd = true;
    }
}

void MainWindow::set_data_filenames ()
//compute every data-store path for this database (Homepath) and the month
//currently shown. Called before the password gate so a brand-new database can
//anchor its store set to the real paths, and again when the displayed month
//changes; all the load/save paths read straight from these members.
{
QString s1, s2, s3;

    Notefilename = Homepath;
    s1.setNum (year);
    s2.setNum(month);
    if (month < 10) {
        s3 = QString ("/Notes-%1-0%2.tdj").arg(s1).arg(s2);
    }
    else
        s3 = QString ("/Notes-%1-%2.tdj").arg(s1).arg(s2);
    Notefilename += s3;

    Appointmentsfilename = Homepath;
    if (month < 10) {
        s3 = QString ("/Appointments-%1-0%2.tdj").arg(s1).arg(s2);
    }
    else
        s3 = QString ("/Appointments-%1-%2.tdj").arg(s1).arg(s2);
    Appointmentsfilename += s3;

    DailyAppointmentsfilename = Homepath + "/DailyAppointments.tdj";
    Contactfilename = Homepath + "/Contacts.tdj";
    Listfilename = Homepath + "/Lists.tdj";
    Anniversaryfilename = Homepath + "/Anniversaries.tdj";
    Attachmentsfilename = Homepath + "/Attachments.tdj";
}

MainWindow::~MainWindow()
{

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    save_notes_and_appts();
    save_other_data ();
    writeprefs();
    delete_lockfile ();

    event->accept();
}

void MainWindow::layout_panels()
//position the left (calendar/trees) and right (toolbar/tabs) panels from
//leftwidth and the current window size
{
int lx, lw, rx, rw, tw, th;

    lx = 10;//left margin
    lw = leftwidth;
    //never let the left panel get narrow enough to clip the calendar's
    //rightmost day columns; sizeHint() already reflects the current font and
    //the compact stylesheet, so the stop point follows both
    int lwMin = calendar->sizeHint().width() + 4;
    if (lw < lwMin)
        lw = lwMin;
    if (lw > width() - 260)
        lw = width() - 260;
    if (lw < lwMin)//window too narrow: the calendar wins over the right panel
        lw = lwMin;
    leftwidth = lw;

    //left panel: the calendar height is the manual value from the horizontal
    //divider below the Today button once the user has dragged it, otherwise it
    //auto-fits the current font (sizeHint) so every day of the month is
    //visible; the widgets below shift down accordingly (clamped so the tree
    //keeps some room)
    int calH = calheight;//-1 means auto-fit
    if (calH < 0)
        calH = calendar->sizeHint().height() + 12;
    int calMax = height() - 227;//room for the today button, divider, buttons, tree
    if (calH > calMax)
        calH = calMax;
    if (calH < 150)//keep the calendar usable in a very small window
        calH = 150;

    calendar->setGeometry(lx, 30, lw, calH);

    int y1 = 30 + calH + 5;
    todaybut.setGeometry(lx, y1, lw, 25);

    //horizontal divider below the Today button: drag to resize the calendar
    hdiv->setGeometry(lx + 2, y1 + 27, lw - 4, 8);

    //buttons for the contact tree: Group / Contact / Delete
    int c3 = (lw - 20) / 3;
    int y2 = y1 + 37;
    catbut.setGeometry(lx, y2, c3, 25);
    childbut.setGeometry(lx + c3 + 10, y2, c3, 25);
    delbut.setGeometry(lx + 2 * (c3 + 10), y2, lw - 2 * (c3 + 10), 25);

    //buttons for the notes tree: New Note / Delete
    int c2 = (lw - 10) / 2;
    listbut.setGeometry(lx, y2, c2, 25);
    delist.setGeometry(lx + c2 + 10, y2, lw - c2 - 10, 25);

    //top of the contact/note trees; their lower border is aligned with the
    //editor after the right panel is laid out below
    int y3 = y2 + 30;

    divider->setGeometry(lx + lw + 4, 30, 8, height() - 60);

    //right panel
    rx = lx + lw + 20;
    rw = width() - rx - 10;
    if (rw < 100)
        rw = 100;

    tb->setGeometry(rx, 30, rw, 30);
    tabcontainer->setGeometry(rx, 60, rw, height() - 100);

    tw = tabcontainer->width();
    th = tabcontainer->height();
    noteditor->setGeometry(10, 10, tw - 20, th - 55);
    contacteditor->setGeometry(10, 10, tw - 20, th - 55);
    listeditor->setGeometry(10, 10, tw - 20, th - 55);
    apptable->setGeometry(10, 10, tw - 20, th - 55);
    anntable->setGeometry(10, 10, tw - 20, th - 55);

    //trees on the left: their lower border lines up with the lower border of
    //the editor (so they clear the status text row), and their top follows
    //the horizontal divider above. mapTo() accounts for the tab bar and the
    //page inset that position the editor inside the tab container.
    int treeBot = noteditor->mapTo(this, noteditor->rect().bottomLeft()).y();
    if (treeBot - y3 < 80)//keep a usable minimum in very small windows
        treeBot = y3 + 80;
    contree->setGeometry(lx, y3, lw, treeBot - y3);
    listree->setGeometry(lx, y3, lw, treeBot - y3);

    //resize and reposition the search widgets
    searchtxtbox->setGeometry(120, 10, tw - 220, 25);
    srchbut->setGeometry(tw - 90, 10, 80, 25);
    srchresults->setGeometry(10, 45, tw - 20, th - 65 - 25);

    //status area: transient messages fill the left, the selected date is
    //shown persistently at the right and never overwritten by them
    statustext->setGeometry(10, height() - 30, width() - 200, 25);
    datelabel->setGeometry(width() - 190, height() - 30, 180, 25);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
//handle dragging of the vertical divider (left/right panels) and the
//horizontal divider (calendar height)
{
    if (watched == divider) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                divider_dragging = true;
                divider_drag_x = me->globalPosition().toPoint().x();
                divider_drag_left = leftwidth;
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (divider_dragging) {
                int dx = me->globalPosition().toPoint().x() - divider_drag_x;
                leftwidth = divider_drag_left + dx;
                layout_panels ();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                divider_dragging = false;
                return true;
            }
        }
    }

    if (watched == hdiv) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                calheight_dragging = true;
                calheight_drag_y = me->globalPosition().toPoint().y();
                calheight_drag_base = calendar->height();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (calheight_dragging) {
                int dy = me->globalPosition().toPoint().y() - calheight_drag_y;
                calheight = calheight_drag_base + dy;
                layout_panels ();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                calheight_dragging = false;
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showEvent(QShowEvent *event)
//the tab container positions its pages only in the layout pass that follows
//the show, which moves the editor lower on screen; re-lay the panels once that
//layout has happened so the trees line up with the editor's lower border
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]
    {
        layout_panels ();
    });
}

void MainWindow::resizeEvent(QResizeEvent *event)
//move and shift the widgets when the window size changes
{
    layout_panels ();

    //this makes the font size box visible on expanding the window and fixes
    //a bug which would prevent it showing if the user has clicked the extension
    //button on the toolbar
    comboFont->setVisible(true);
    comboSize->setVisible(true);

    //pass the event up the chain
    QWidget::resizeEvent(event);
}

void MainWindow::test_password ()
{
QString Testfilename, s, serr;
QString Decstr;
QMessageBox msgBox;
QByteArray decrypted;

    Testfilename.append (Homepath);
    Testfilename.append ("/Checkpwd.tdj");

    QFile f(Testfilename);
    if (!f.open(QIODevice::ReadOnly)) {//no test file - cannot continue
        serr.clear();
        serr.append (QObject::tr("The encrypted test phrase could not be read."));
        serr.append (QObject::tr("The program will now terminate"));
        msgBox.setText(serr);
        msgBox.exec();
        delete_lockfile();
        throw StartupAbort{1};
    }

    QByteArray iv;
    iv.resize(16);
    if (f.read(iv.data(), 16) != 16) {
        f.close();
        serr.clear();
        serr.append (QObject::tr("The encrypted test phrase could not be read."));
        serr.append (QObject::tr("The program will now terminate"));
        msgBox.setText(serr);
        msgBox.exec();
        delete_lockfile();
        throw StartupAbort{1};
    }
    m_crypto.setLegacyIv(iv.constData());

    qint32 len;
    int k = tdj_read_field_len(f, len);
    if (k == TdjFieldOk)
        k = tdj_read_field_body(f, len, decrypted,
                                m_crypto.legacyKey(), m_crypto.legacyIv());
    f.close();

    if (k != TdjFieldOk) {
        serr.clear();
        serr.append (QObject::tr("The encrypted test phrase could not be decoded."));
        serr.append (QObject::tr("Check that you entered the correct password."));
        serr.append (QObject::tr("The program will now terminate"));
        msgBox.setText(serr);
        msgBox.exec();
        delete_lockfile();
        throw StartupAbort{1};
    }

    Decstr = QString::fromUtf8(decrypted);

    //display the string
    Cnfdialog = new QDialog (this);

    lbl4 = new QLabel(tr("Confirm encoded phrase"), Cnfdialog);
    ledt4 = new QLineEdit (Cnfdialog);
    ledt4->setText(Decstr);
    lbl4->setBuddy(ledt4);
    ok2but = new QPushButton(tr("&Confirm"));
    cncl2but = new QPushButton(tr("&Reject"));

    connect (ok2but, &QPushButton::clicked, this, &MainWindow::confirm_phrase);
    connect (cncl2but, &QPushButton::clicked, this, &MainWindow::reject_phrase);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(lbl4);
    vbox->addWidget(ledt4);
    vbox->addWidget(ok2but);
    vbox->addWidget(cncl2but);
    Cnfdialog->setLayout(vbox);

    //execute the dialog
    Cnfdialog->exec();
    //check the result
    k = Cnfdialog->result();

    if (k == QDialog::Rejected) {
        serr.clear();
        serr.append (QObject::tr("The program will now terminate"));
        msgBox.setText(serr);
        msgBox.exec();
        delete_lockfile();
        throw StartupAbort{1};
    }

}

void MainWindow::reencrypt_all_stores ()
{
//Transactional re-encryption of every store. Caller must have installed the
//old key (reads with inivecflag==1 use oldAesSymKey32) and the new key
//(writes use newAesSymKey32) into the globals first.
//
//Phase 1 decrypts each store with the old key and writes the new ciphertext
//to `*.new`; nothing is modified yet. Phase 2 promotes every `.new` over the
//original (the original is first preserved as `*.bak`). A crash in the middle
//of phase 2 cannot corrupt the database: the Reencrypt.pending manifest
//(written with state "committing" before the first rename, then atomically
//rewritten as "committed" once every pair is promoted) lets a later start
//roll the set back to the old key or finish the change deterministically.
QStringList newFiles, origFiles;
QDir dir;
QStringList fltr;
QFileInfoList list;
QString path;
int i;
bool ok = true;

    dir.setFilter(QDir::Files);
    dir.setPath(Homepath);

    //month-partitioned stores (Notes + Appointments)
    fltr << "Notes*.tdj" << "Appointments*.tdj";
    dir.setNameFilters(fltr);
    list = dir.entryInfoList();//list of month files

    //phase 1: read with the old key, write `*.new` with the new key.
    //Month stores run through StorageManager with explicit paths (the old
    //code routed this through the global reencfileInfo; the GUI wrappers are
    //only used for the displayed month and never re-encrypt)
    for (i = 0; i < list.size(); i++) {
        path = Homepath + "/" + list.at(i).fileName();
        if (list.at(i).baseName().startsWith("Notes")) {
            if (m_store.loadJournal(m_crypto, path, 1) != TdjFieldCorrupt)
                m_store.saveJournal(m_crypto, path + ".new", 1);
        }
        else {
            if (m_store.loadApptMonth(m_crypto, path, 1) != TdjFieldCorrupt)
                m_store.saveApptMonth(m_crypto, path + ".new", 1);
        }
        newFiles.append(path + ".new");
        origFiles.append(path);
    }

    //flat stores are re-encrypted from the in-memory content (it was saved
    //under the old key just before the change began); the GUI writers append
    //`.new` to the store filename when inivecflag==1
    write_daily_appt_file (1);
    newFiles.append(DailyAppointmentsfilename + ".new");
    origFiles.append(DailyAppointmentsfilename);

    write_ann_file (1);
    newFiles.append(Anniversaryfilename + ".new");
    origFiles.append(Anniversaryfilename);

    write_contacts (1);
    newFiles.append(Contactfilename + ".new");
    origFiles.append(Contactfilename);

    write_lists (1);
    newFiles.append(Listfilename + ".new");
    origFiles.append(Listfilename);

    //encrypted attachment store re-encrypts through the same key shuffle
    m_store.loadAttachments (m_crypto, Attachmentsfilename, 1);
    m_store.saveAttachments (m_crypto, Attachmentsfilename + ".new", 1);
    newFiles.append(Attachmentsfilename + ".new");
    origFiles.append(Attachmentsfilename);

    //every `*.new` must exist, else abort without touching the originals
    for (i = 0; i < newFiles.size(); i++)
        if (QFile::exists(newFiles.at(i)) == false)
            ok = false;

    if (ok == false) {
        for (i = 0; i < newFiles.size(); i++)
            QFile::remove(newFiles.at(i));
        crypt_error_notification ("Error in password re-encryption.");
        return;
    }

    //record the transaction before touching a single original. A failure here
    //(e.g. full disk) drops the untouched `.new` copies; the originals are
    //still under the old key throughout. (A `.new` accidentally left behind
    //without a manifest is inert - no loader reads it.)
    if (tdj_reencrypt_write_manifest(Homepath, origFiles, newFiles,
                                     false) == false) {
        for (i = 0; i < newFiles.size(); i++)
            QFile::remove(newFiles.at(i));
        crypt_error_notification ("Error in password re-encryption.");
        return;
    }

    //phase 2: commit - original -> *.bak, *.new -> original
    for (i = 0; i < newFiles.size() && ok; i++) {
        if (QFile::exists(origFiles.at(i)))
            if (QFile::rename(origFiles.at(i), origFiles.at(i) + ".bak") == false)
                ok = false;
        if (ok)
            if (QFile::rename(newFiles.at(i), origFiles.at(i)) == false)
                ok = false;
    }

    if (ok == true) {
        //switch the manifest to "committed" BEFORE deleting backups, so a
        //crash between the renames and the cleanup resolves on a later start
        //by simply dropping the backups; the set is already uniformly new-key
        if (tdj_reencrypt_write_manifest(Homepath, origFiles, newFiles,
                                         true) == false)
            ok = false;
    }

    if (ok == true) {//success: back-ups are no longer needed
        for (i = 0; i < newFiles.size(); i++)
            QFile::remove(origFiles.at(i) + ".bak");
        tdj_reencrypt_clear_manifest(Homepath);
    }
    else {
        //roll back: restore every `.bak` over its original, drop stray `.new`
        for (i = 0; i < newFiles.size(); i++) {
            if (QFile::exists(origFiles.at(i) + ".bak"))
                QFile::rename(origFiles.at(i) + ".bak", origFiles.at(i));
            else
                QFile::remove(origFiles.at(i));
            QFile::remove(newFiles.at(i));
        }
        tdj_reencrypt_clear_manifest(Homepath);
        crypt_error_notification ("Error in password re-encryption.");
    }

}

void MainWindow::save_notes_as_text()
{
int i, j;
QDir dir;
QStringList fltr;
QFileInfoList list;
QString txtfile, s, fname, Year, Month, path;
bool ok;

    //file to put the exported text
    txtfile.append (Homepath);
    txtfile.append ("/Journal.txt");
    QFile file(txtfile);

    ok = file.open(QFile::WriteOnly);
    if (ok == false)
        return;
    QTextStream out (&file);

    //path for file list
    dir.setFilter(QDir::Files);
    dir.setPath(Homepath);

    //create list of Notes files
    fltr << "Notes*.tdj";
    dir.setNameFilters(fltr);
    list = dir.entryInfoList();//list of Notes files

    //read and export each file as text
    for (i = 0; i < list.size(); i++) {
        path = Homepath + "/" + list.at(i).fileName();
        //read the listed file with the current session key (0), not the
        //migration key that inivecflag==1 implies
        read_journal_file (0, path);

        fname = list.at(i).baseName();//filename without path and extension : "Notes-xxxx-xx"
        Month = fname.remove (0, 11);//remove leading part : "Notes-xxxx-". Only "xx" remains

        fname = list.at(i).baseName();//filename without path and extension : "Notes-xxxx-xx"
        Year = fname.remove(0, 6);//remove leading part of Notefilename : "Notes-". Only "xxxx-xx" remains
        Year.truncate(4);//remove trailing part : "-xx". Only "xxxx" remains.

        for (j = 1; j <= 31; j++) {
            if (m_store.note(j).data.isEmpty() == false) {
                out << j << " " << get_month_name(Month.toInt()) << " " << Year << "\n";

                QTextDocument doc;
                doc.setHtml(m_store.note(j).data);
                s = doc.toPlainText();

                out << s;
                out << "\n";
            }
        }
        out << "\n";
    }
    file.close();

    //the loop above left the model holding the last exported file; restore the
    //displayed month so a subsequent save cannot write into the wrong file
    reload_current_month ();

    s = QString (tr("Journal data saved to %1")).arg(txtfile);
    statustext->setText(s);

}

void MainWindow::save_lists_as_text()
{
QString txtfile, s;
QTreeWidgetItem *it;
int i, toplevelcount;
bool ok;

    txtfile.append (Homepath);
    txtfile.append ("/Notes.txt");
    QFile file(txtfile);
    ok = file.open(QFile::WriteOnly);
    if (ok == false)
        return;
    QTextStream out(&file);

    //number of categories
    toplevelcount = listree->topLevelItemCount();
    out << "Number of notes:" << toplevelcount << "\n";

    //loop over lists
    for (i = 0; i < toplevelcount; i++){
        it = listree->topLevelItem(i);

        QTextDocument doc;
        doc.setHtml(it->text(1));
        s = doc.toPlainText();

        out << "\nName of Note:";
        out << s;
        out << "\n";
    }

    file.close();
    s = QString (tr("Notes data saved to %1")).arg(txtfile);
    statustext->setText(s);
}

void MainWindow::confirm_phrase()
{
    Cnfdialog->accept();
}

void MainWindow::reject_phrase()
{
    Cnfdialog->reject();
}

void MainWindow::setuptoolbar()
{
    //toolbar for the editor
    tb = new QToolBar;
    tb->setParent (this);
    tb->setGeometry(320, 30, 470, 30);
    tb->setFloatable (false);
    tb->setMovable(false);

    actionTextBold = new QAction(QIcon::fromTheme("", QIcon(":/images/textbold.png")), tr("&Bold"), this);
    actionTextBold->setShortcut(Qt::CTRL | Qt::Key_B);
    actionTextBold->setPriority(QAction::LowPriority);
    QFont bold;
    bold.setBold(true);
    actionTextBold->setFont(bold);
    connect(actionTextBold, &QAction::triggered, this, &MainWindow::textBold);
    tb->addAction(actionTextBold);
    actionTextBold->setCheckable(true);

    actionTextItalic = new QAction(QIcon::fromTheme("", QIcon(":/images/textitalic.png")), tr("&Italic"), this);
    actionTextItalic->setPriority(QAction::LowPriority);
    actionTextItalic->setShortcut(Qt::CTRL | Qt::Key_I);
    QFont italic;
    italic.setItalic(true);
    actionTextItalic->setFont(italic);
    connect(actionTextItalic, &QAction::triggered, this, &MainWindow::textItalic);
    tb->addAction(actionTextItalic);
    actionTextItalic->setCheckable(true);

    actionTextUnderline = new QAction(QIcon::fromTheme("", QIcon(":/images/textunder.png")), tr("&Underline"), this);
    actionTextUnderline->setShortcut(Qt::CTRL | Qt::Key_U);
    actionTextUnderline->setPriority(QAction::LowPriority);
    QFont underline;
    underline.setUnderline(true);
    actionTextUnderline->setFont(underline);
    connect(actionTextUnderline, &QAction::triggered, this, &MainWindow::textUnderline);
    tb->addAction(actionTextUnderline);
    actionTextUnderline->setCheckable(true);

    QActionGroup *grp = new QActionGroup(this);
    connect(grp, &QActionGroup::triggered, this, &MainWindow::textAlign);

    actionAlignLeft = new QAction(QIcon::fromTheme("", QIcon(":/images/textleft.png")),tr("&Left"), grp);
    actionAlignCenter = new QAction(QIcon::fromTheme("",QIcon(":/images/textcenter.png")),tr("C&enter"), grp);
    actionAlignRight = new QAction(QIcon::fromTheme("",QIcon(":/images/textright.png")),tr("&Right"), grp);
    actionAlignJustify = new QAction(QIcon::fromTheme("",QIcon(":/images/textjustify.png")),tr("&Justify"), grp);

    actionAlignLeft->setShortcut(Qt::CTRL | Qt::Key_L);
    actionAlignLeft->setCheckable(true);
    actionAlignLeft->setPriority(QAction::LowPriority);
    actionAlignCenter->setShortcut(Qt::CTRL | Qt::Key_E);
    actionAlignCenter->setCheckable(true);
    actionAlignCenter->setPriority(QAction::LowPriority);
    actionAlignRight->setShortcut(Qt::CTRL | Qt::Key_R);
    actionAlignRight->setCheckable(true);
    actionAlignRight->setPriority(QAction::LowPriority);
    actionAlignJustify->setShortcut(Qt::CTRL | Qt::Key_J);
    actionAlignJustify->setCheckable(true);
    actionAlignJustify->setPriority(QAction::LowPriority);

    tb->addActions(grp->actions());

    QAction *actionInsertImage= new QAction(QIcon::fromTheme("", QIcon(":/images/insert-image.png")), tr("&Insert image"), this);
    actionInsertImage->setPriority(QAction::LowPriority);
    connect(actionInsertImage, &QAction::triggered, this, &MainWindow::insertImage);
    actionInsertImage->setCheckable(true);

    tb->addAction(actionInsertImage);

    QPixmap pix(16, 16);
    pix.fill(Qt::black);
    actionTextColor = new QAction(pix, tr("&Color..."), this);
    connect(actionTextColor, &QAction::triggered, this, &MainWindow::textColor);
    tb->addAction(actionTextColor);

    comboFont = new QFontComboBox(tb);
    tb->addWidget(comboFont);
    //Qt6 removed QComboBox::activated(QString); textActivated passes the text
    connect(comboFont, &QComboBox::textActivated, this, &MainWindow::textFamily);

    comboSize = new QComboBox(tb);
    comboSize->setObjectName("comboSize");
    tb->addWidget(comboSize);
    comboSize->setEditable(true);

    for (int size : QFontDatabase::standardSizes ())
    comboSize->addItem(QString::number(size));

    connect(comboSize, &QComboBox::textActivated, this, &MainWindow::textSize);
    comboSize->setCurrentIndex(comboSize->findText(QString::number(QApplication::font().pointSize())));
}

void MainWindow::textBold()
{
QTextCharFormat fmt;

    fmt.setFontWeight(actionTextBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(actionTextItalic->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textUnderline()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(actionTextUnderline->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textAlign(QAction *a)
{
QTextEdit *editor;
int i = -1;

    i = tabcontainer->currentIndex();

    if (i == 0)
        editor = noteditor;
    else if (i == 2)
        editor = contacteditor;
    else if (i == 3)
        editor = listeditor;
    else
        return;

    if (a == actionAlignLeft)
        editor->setAlignment(Qt::AlignLeft | Qt::AlignAbsolute);
    else if (a == actionAlignCenter)
        editor->setAlignment(Qt::AlignHCenter);
    else if (a == actionAlignRight)
        editor->setAlignment(Qt::AlignRight | Qt::AlignAbsolute);
    else if (a == actionAlignJustify)
        editor->setAlignment(Qt::AlignJustify);
}

void MainWindow::textColor()
{
QTextEdit *editor;
int i = -1;

    i = tabcontainer->currentIndex();

    if (i == 0)
        editor = noteditor;
    else if (i == 2)
        editor = contacteditor;
    else if (i == 3)
        editor = listeditor;
    else
        return;

    QColor col = QColorDialog::getColor(editor->textColor(), this);
    if (!col.isValid())
        return;
    QTextCharFormat fmt;
    fmt.setForeground(col);
    mergeFormatOnWordOrSelection(fmt);
    colorChanged(col);
}

void MainWindow::insertImage()
{
QTextEdit *editor;
QString filters;
int i;

    i = tabcontainer->currentIndex();

    if (i == 0)
        editor = noteditor;
    else if (i == 2)
        editor = contacteditor;
    else if (i == 3)
        editor = listeditor;
    else
        return;

    filters += tr("Common Graphics (*.png *.jpg *.jpeg *.gif);;");
    filters += tr("Portable Network Graphics (PNG) (*.png);;");
    filters += tr("JPEG (*.jpg *.jpeg);;");
    filters += tr("Graphics Interchange Format (*.gif);;");
    filters += tr("All Files (*)");

    QString file = QFileDialog::getOpenFileName(this, tr("Open image..."), QString(), filters);
    if (file.isEmpty())
        return;

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QByteArray img = f.readAll();

    if (img.isEmpty() || img.size() > kMaxAttachmentBytes) {
        QMessageBox::warning(this, tr("Image too large"),
            tr("The image is larger than %1 MB and cannot be stored in the "
               "encrypted database.").arg(kMaxAttachmentBytes / (1024 * 1024)));
        return;
    }

    //the bytes go into the encrypted Attachments.tdj store; the editor HTML
    //carries only a content-address reference, never a filesystem path
    QString id = m_store.putAttachment(img);
    if (id.isEmpty())
        return;

    editor->insertHtml(QString("<img src=\"%1\" />").arg(id));
    m_attachmentsDirty = true;

    //fit the fresh picture to the current editor width (later window or
    //splitter resizes are handled by TdjEditor::resizeEvent)
    static_cast<TdjEditor*>(editor)->refitImagesToWidth();
}

QString MainWindow::import_legacy_images(const QString &html)
{
    //rewrite the src of every local-file <img> to a tdj-image: reference,
    //importing the bytes into the encrypted attachment store. Already-managed
    //tdj-image: srcs and unresolvable files (missing, unreadable, oversized)
    //are left untouched, so the function is safe to run on any displayed html.
    static const QRegularExpression imgTag("<img\\b[^>]*>",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression srcAttr(
        "\\bsrc\\s*=\\s*(\"([^\"]*)\"|'([^']*)'|([^\\s>]*))",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator it = imgTag.globalMatch(html);
    QString out;
    qsizetype pos = 0;
    bool imported = false;

    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        out.append(html.mid(pos, m.capturedStart() - pos));
        pos = m.capturedEnd();

        QString tag = m.captured(0);
        QRegularExpressionMatch src = srcAttr.match(tag);
        if (!src.hasMatch()) {
            out.append(tag);
            continue;
        }
        QString val = src.captured(2);
        if (val.isEmpty())
            val = src.captured(3);
        if (val.isEmpty())
            val = src.captured(4);
        if (val.startsWith("tdj-image:")) {//already an encrypted reference
            out.append(tag);
            continue;
        }

        //resolve the src to a local path (accept absolute or Homepath-relative)
        QString path = val;
        if (path.startsWith("file://"))
            path = QUrl(path).toLocalFile();
        if (!QFileInfo(path).isAbsolute())
            path = Homepath + "/" + path;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            out.append(tag);
            continue;
        }
        QByteArray img = f.readAll();
        if (img.isEmpty() || img.size() > kMaxAttachmentBytes) {
            out.append(tag);
            continue;
        }

        QString id = m_store.putAttachment(img);
        if (id.isEmpty()) {
            out.append(tag);
            continue;
        }
        imported = true;
        out.append(tag.left(src.capturedStart())
                   + "src=\"" + id + "\""
                   + tag.mid(src.capturedEnd()));
    }
    if (!imported)
        return html;
    out.append(html.mid(pos));
    m_attachmentsDirty = true;
    return out;
}

void MainWindow::prune_orphan_attachments()
{
    if (m_store.attachmentCount() == 0)
        return;

    QSet<QString> keep;

    //content currently in memory (the displayed month, contacts, lists)
    int d;
    for (d = 1; d <= 31; d++) {
        for (const QString &id : tdj_attachment_refs(m_store.note(d).data))
            keep.insert(id);
    }

    //every stored month, via throwaway models so the live buffers survive
    QDir dir(Homepath);
    QStringList fltr;
    fltr << "Notes-*.tdj";
    dir.setNameFilters(fltr);
    QFileInfoList list = dir.entryInfoList();
    for (const QFileInfo &fi : list) {
        StorageManager month;
        if (month.loadJournal(m_crypto, Homepath + "/" + fi.fileName(), 0)
                != TdjFieldOk)
            continue;
        for (d = 1; d <= 31; d++)
            for (const QString &id : tdj_attachment_refs(month.note(d).data))
                keep.insert(id);
    }

    //lists + contacts (their editor HTML can embed images too)
    {
        StorageManager flat;
        QVector<TdjList> lists;
        if (flat.loadLists(m_crypto, Listfilename, lists) == TdjFieldOk)
            for (const TdjList &l : lists)
                for (const QString &id : tdj_attachment_refs(l.data))
                    keep.insert(id);
        QVector<TdjGroup> groups;
        if (flat.loadContacts(m_crypto, Contactfilename, groups) == TdjFieldOk)
            for (const TdjGroup &g : groups)
                for (const TdjItem &it : g.items)
                    for (const QString &id : tdj_attachment_refs(it.data))
                        keep.insert(id);
    }

    int before = m_store.attachmentCount();
    m_store.pruneAttachments(keep);

    if (m_store.attachmentCount() != before)
        m_store.saveAttachments(m_crypto, Attachmentsfilename, 0);
}

TdjEditor::TdjEditor(StorageManager *store, QWidget *parent)
: QTextEdit (parent), m_store(store)
{
}

QVariant TdjEditor::loadResource(int type, const QUrl &name)
{
    if (type == QTextDocument::ImageResource && m_store != 0) {
        QString id = name.toString();
        if (id.startsWith("tdj-image:")) {
            QByteArray bytes;
            if (m_store->getAttachment(id, bytes)) {
                QPixmap pm;
                if (pm.loadFromData(bytes))
                    return pm;
            }
        }
    }
    return QTextEdit::loadResource(type, name);
}

void TdjEditor::refitImagesToWidth()
{
    if (m_store == 0)
        return;

    //follow the window width with a small margin, like Treecle's panel; never
    //let the available box collapse to nothing during a drag
    int maxWidth = qMax(60, viewport()->width() - 30);
    if (maxWidth < 1)
        return;

    struct ImgScale { int pos; int len; QTextImageFormat fmt; };
    QList<ImgScale> work;

    QTextDocument *doc = document();

    //pass 1: collect, without touching the document, which images actually
    //need rescaling. Intrinsic sizes come from the stored attachment bytes via
    //QImageReader::size(), which only decodes the image headers - cheap enough
    //to repeat on every resize tick.
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment frag = it.fragment();
            QTextCharFormat cf = frag.charFormat();
            if (cf.isImageFormat() == false)
                continue;
            QTextImageFormat img = cf.toImageFormat();
            QString id = img.name();
            if (!id.startsWith("tdj-image:"))
                continue;

            QByteArray bytes;
            if (!m_store->getAttachment(id, bytes))
                continue;
            QBuffer dev(&bytes);
            QImageReader reader;
            reader.setDevice(&dev);
            QSize orig = reader.size();
            if (orig.isValid() == false || orig.width() <= 0 || orig.height() <= 0)
                continue;

            int w = -1, h = -1;
            if (orig.width() > maxWidth) {
                w = maxWidth;
                h = qMax(1, (int)((qreal)orig.height() * maxWidth / orig.width()));
                if (img.width() == w && img.height() == h)
                    continue;   //already sized to the current panel
            }
            else if (img.width() > 0)
                ;               //smaller than the panel: restore intrinsic size
            else
                continue;       //already at its intrinsic size

            ImgScale s;
            s.pos = frag.position();
            s.len = frag.length();
            s.fmt = img;
            s.fmt.setWidth(w);
            s.fmt.setHeight(h);
            work.append(s);
        }
    }

    if (work.isEmpty())
        return;

    //pass 2: rewrite the formats. The edit block makes it one undo entry; the
    //signal blocker keeps the editor's textChanged (-> save_note) from firing
    //on a display-only resize, and the undo history is suspended so a resize
    //drag can never poison the user's Undo stack.
    const bool undoState = doc->isUndoRedoEnabled();
    doc->setUndoRedoEnabled(false);
    {
        QSignalBlocker blocker(doc);
        QTextCursor cur(doc);
        cur.beginEditBlock();
        for (const ImgScale &s : work) {
            cur.setPosition(s.pos);
            cur.setPosition(s.pos + s.len, QTextCursor::KeepAnchor);
            cur.setCharFormat(s.fmt);
        }
        cur.endEditBlock();
    }
    doc->setUndoRedoEnabled(undoState);
}

void TdjEditor::resizeEvent(QResizeEvent *e)
{
    //let the base class relayout to the new width first, then refit the
    //attachments so large pictures track the editor (or splitter) size
    QTextEdit::resizeEvent(e);
    refitImagesToWidth();
}

void MainWindow::cursorPositionChanged()
{
QTextEdit *editor;
int i = -1;

    i = tabcontainer->currentIndex();

    if (i == 0)
        editor = noteditor;
    else if (i == 2)
        editor = contacteditor;
    else if (i == 3)
        editor = listeditor;
    else
        return;

    alignmentChanged(editor->alignment());
}

void MainWindow::currentCharFormatChanged(const QTextCharFormat &format)
{
    fontChanged(format.font());
    colorChanged(format.foreground().color());
}

void MainWindow::fontChanged(const QFont &f)
{
    comboFont->setCurrentIndex(comboFont->findText(QFontInfo(f).family()));
    comboSize->setCurrentIndex(comboSize->findText(QString::number(f.pointSize())));

    actionTextBold->setChecked(f.bold());
    actionTextItalic->setChecked(f.italic());
    actionTextUnderline->setChecked(f.underline());
}

void MainWindow::colorChanged(const QColor &c)
{
    QPixmap pix(16, 16);
    pix.fill(c);
    actionTextColor->setIcon(pix);
}

void MainWindow::alignmentChanged(Qt::Alignment a)
{
    if (a & Qt::AlignLeft)
        actionAlignLeft->setChecked(true);
    else if (a & Qt::AlignHCenter)
        actionAlignCenter->setChecked(true);
    else if (a & Qt::AlignRight)
        actionAlignRight->setChecked(true);
    else if (a & Qt::AlignJustify)
        actionAlignJustify->setChecked(true);
}

void MainWindow::textFamily(const QString &f)
{
    QTextCharFormat fmt;
    fmt.setFontFamilies(QStringList() << f);
    mergeFormatOnWordOrSelection(fmt);
}

void MainWindow::textSize(const QString &p)
{
    qreal pointSize = p.toFloat();
    if (p.toFloat() > 0) {
        QTextCharFormat fmt;
        fmt.setFontPointSize(pointSize);
        mergeFormatOnWordOrSelection(fmt);
    }
}

void MainWindow::mergeFormatOnWordOrSelection(const QTextCharFormat &format)
{
QTextEdit *editor;
int i = -1;

    i = tabcontainer->currentIndex();

    if (i == 0)
        editor = noteditor;
    else if (i == 2)
        editor = contacteditor;
    else if (i == 3)
        editor = listeditor;
    else
        return;

    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    editor->mergeCurrentCharFormat(format);
}
