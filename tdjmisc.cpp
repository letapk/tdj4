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
#include <QFileDialog>
#include <QImageReader>
#include <QDesktopServices>
#include <QSaveFile>

extern void crypt_error_notification (const char *errstr);

extern QString Lockfilename, userpath;
extern bool setpwd;

extern Note note[];

//used while reencrypting data files due to a change in password
QFileInfo reencfileInfo;

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
QMessageBox msgBox;

    qtpath.append (userpath);
    qtdir = QDir (qtpath);

    if (qtdir.exists() == false) {
        s1.append (QObject::tr("The tdj data directory does not exist. "));
        s1.append (QObject::tr("This is required to store your work.\n"));
        s1.append (QObject::tr("Click OK to create a new, hidden subdirectory "));
        s1.append (QObject::tr("in your area with the name :\n"));
        s1.append (qtpath);
        msgBox.setText(s1);
        msgBox.exec();

        qtdir.mkdir(qtpath);

        s1.clear();
        s1.append (QObject::tr("The program needs a password to encrypt the data. "));
        s1.append (QObject::tr("This will not be stored anywhere, and thus should not be forgotten.\n"));
        s1.append (QObject::tr("Click OK to create a new password in the next step.\n"));
        msgBox.setText(s1);
        msgBox.exec();

        //ask for password
        setpwd = true;
    }
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
    //keep both panels usable
    if (lw < 150)
        lw = 150;
    if (lw > width() - 260)
        lw = width() - 260;
    if (lw < 150)
        lw = 150;
    leftwidth = lw;

    //left panel: the calendar takes the full height its content needs so that
    //every day of the month is visible even with a larger application font;
    //the widgets below it shift down accordingly (clamped so the tree below
    //keeps some room)
    int calH = calendar->sizeHint().height() + 12;
    int calMax = height() - 215;//keep room for the today button, buttons, tree
    if (calH > calMax)
        calH = calMax;
    if (calH < 150)//keep the calendar usable in a very small window
        calH = 150;

    calendar->setGeometry(lx, 30, lw, calH);

    int y1 = 30 + calH + 5;
    todaybut.setGeometry(lx, y1, lw, 25);

    //buttons for the contact tree: Group / Contact / Delete
    int c3 = (lw - 20) / 3;
    int y2 = y1 + 30;
    catbut.setGeometry(lx, y2, c3, 25);
    childbut.setGeometry(lx + c3 + 10, y2, c3, 25);
    delbut.setGeometry(lx + 2 * (c3 + 10), y2, lw - 2 * (c3 + 10), 25);

    //buttons for the notes tree: New Note / Delete
    int c2 = (lw - 10) / 2;
    listbut.setGeometry(lx, y2, c2, 25);
    delist.setGeometry(lx + c2 + 10, y2, lw - c2 - 10, 25);

    //trees fill the remaining height on the left
    int y3 = y2 + 30;
    contree->setGeometry(lx, y3, lw, height() - 10 - y3);
    listree->setGeometry(lx, y3, lw, height() - 10 - y3);

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
//handle dragging of the divider between the left and right panels
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

    return QMainWindow::eventFilter(watched, event);
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
        std::exit (1);
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
        std::exit (1);
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
        std::exit (1);
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
        std::exit (1);
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
//original (the original is first preserved as `*.bak`). Any failure aborts
//before a single rename so a crash or error can never leave a mixed-key
//state.
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

    //phase 1: read with the old key, write `*.new` with the new key
    for (i = 0; i < list.size(); i++) {
        reencfileInfo = list.at(i);//file at position i in the list
        if (reencfileInfo.baseName().startsWith("Notes")) {
            read_journal_file (1);
            write_journal_file (1);
        }
        else {
            read_appt_file (1);
            write_appt_file (1);
        }
        path = Homepath + "/" + reencfileInfo.fileName() + ".new";
        newFiles.append(path);
        origFiles.append(Homepath + "/" + reencfileInfo.fileName());
    }

    //flat stores are re-encrypted from the in-memory content (it was saved
    //under the old key just before the change began)
    reencfileInfo = QFileInfo(DailyAppointmentsfilename);
    write_daily_appt_file (1);
    newFiles.append(Homepath + "/DailyAppointments.tdj.new");
    origFiles.append(Homepath + "/DailyAppointments.tdj");

    reencfileInfo = QFileInfo(Anniversaryfilename);
    write_ann_file (1);
    newFiles.append(Homepath + "/" + reencfileInfo.fileName() + ".new");
    origFiles.append(Homepath + "/" + reencfileInfo.fileName());

    reencfileInfo = QFileInfo(Contactfilename);
    write_contacts (1);
    newFiles.append(Homepath + "/" + reencfileInfo.fileName() + ".new");
    origFiles.append(Homepath + "/" + reencfileInfo.fileName());

    reencfileInfo = QFileInfo(Listfilename);
    write_lists (1);
    newFiles.append(Homepath + "/" + reencfileInfo.fileName() + ".new");
    origFiles.append(Homepath + "/" + reencfileInfo.fileName());

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

    //phase 2: commit - original -> *.bak, *.new -> original
    for (i = 0; i < newFiles.size() && ok; i++) {
        if (QFile::exists(origFiles.at(i)))
            if (QFile::rename(origFiles.at(i), origFiles.at(i) + ".bak") == false)
                ok = false;
        if (ok)
            if (QFile::rename(newFiles.at(i), origFiles.at(i)) == false)
                ok = false;
    }

    if (ok == true) {//success: back-ups are no longer needed
        for (i = 0; i < newFiles.size(); i++)
            QFile::remove(origFiles.at(i) + ".bak");
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
        crypt_error_notification ("Error in password re-encryption.");
    }

}

void MainWindow::save_notes_as_text()
{
int i, j;
QDir dir;
QStringList fltr;
QFileInfoList list;
QString txtfile, s, fname, Year, Month;
QTextDocument *doc;
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
        reencfileInfo = list.at(i);//file at position i in the list
        //read the listed file with the current session key (0), not the
        //migration key that inivecflag==1 implies
        read_journal_file (0, Homepath + "/" + reencfileInfo.fileName());

        fname = reencfileInfo.baseName();//filename without path and extension : "Notes-xxxx-xx"
        Month = fname.remove (0, 11);//remove leading part : "Notes-xxxx-". Only "xx" remains

        fname = reencfileInfo.baseName();//filename without path and extension : "Notes-xxxx-xx"
        Year = fname.remove(0, 6);//remove leading part of Notefilename : "Notes-". Only "xxxx-xx" remains
        Year.truncate(4);//remove trailing part : "-xx". Only "xxxx" remains.

        for (j = 1; j <= 31; j++) {
            if (note[j].data.isEmpty() == false) {
                out << j << " " << get_month_name(Month.toInt()) << " " << Year << "\n";

                doc = new QTextDocument ();
                doc->setHtml(note[j].data);
                s = doc->toPlainText();
                delete doc;

                out << s;
                out << "\n";
            }
        }
        out << "\n";
    }
    file.close();

    //the loop above left note[] holding the last exported file; restore the
    //displayed month so a subsequent save cannot write into the wrong file
    reload_current_month ();

    s = QString (tr("Journal data saved to %1")).arg(txtfile);
    statustext->setText(s);

}

void MainWindow::save_lists_as_text()
{
QString txtfile, s;
QTreeWidgetItem *it;
QTextDocument *doc;
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

        doc = new QTextDocument ();
        doc->setHtml(it->text(1));
        s = doc->toPlainText();
        delete doc;

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
QString s, filters, fname;
QFileInfo fi;
QTextEdit *editor;
QMessageBox msgBox;
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
    if (!QFile::exists(file))
        return;

    fi = QFileInfo(file);
    if (fi.path() != Homepath) {
        //copy the file to the data subdirectory
        fname.clear();
        fname.append(Homepath);
        fname.append("/");
        fname.append(fi.fileName());
        QFile::copy (fi.filePath(), fname);

        s.append (QObject::tr("The image file has been copied to the tdj data directory "));
        s.append (Homepath);
        s.append (QObject::tr("\nClick OK to continue"));
        msgBox.setText(s);
        msgBox.exec();
    }

    editor->insertHtml(QString ("<img src=\"%1/%2\">").arg(Homepath).arg(fi.fileName()));
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
