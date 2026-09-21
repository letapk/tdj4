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

#include <QHeaderView>
#include <QInputDialog>
#include <QTextBlock>
#include <QFileDialog>

#include "tdj.h"

//the six data models (notes, appointments, the repeating-appointments day,
//anniversaries) live in StorageManager MainWindow::m_store - no file-scope
//buffers or extern chains anywhere in the GUI

//userpath contains the path to the data subdirectory
QString Lockfilename, userpath;
bool setpwd = false;

bool check_lockfile(void);
bool create_lockfile ();
void delete_lockfile ();
void clean_up_and_quit ();

void check_qtdata_dir ();

int main(int argc, char *argv[])
//checks for the lockfile and the data directories
//start user-interface
{
bool ok = false;
int i;

    Q_INIT_RESOURCE(tdj);
    QApplication app(argc, argv);

    //translations are optional: install only the ones actually found, so a
    //missing locale simply leaves the program in its source language
    QTranslator appTranslator;
    if (appTranslator.load("tdj4_" + QLocale::system().name(), qApp->applicationDirPath()))
        app.installTranslator(&appTranslator);

    QTranslator qtTranslator;
    if (qtTranslator.load("qt_" + QLocale::system().name(), qApp->applicationDirPath()))
        app.installTranslator(&qtTranslator);

    //data directory: ~/.cryptdj by default. TDJ_DATA_DIR overrides it (used
    //to point the program at a throwaway database for testing).
    userpath.clear();
    QByteArray dataDirOverride = qgetenv("TDJ_DATA_DIR");
    if (dataDirOverride.isEmpty() == false)
        userpath.append (QString::fromLocal8Bit(dataDirOverride));
    else
        userpath.append (QDir::homePath() + "/.cryptdj");

    //check for the tdj data directory and create it if required
    check_qtdata_dir();

    //before constructing the mainwindow check if a lockfile is present
    Lockfilename.append (userpath);
    Lockfilename.append ("/tdjlockfile.tdj");

    i = CryptoManager::init ();
    if (i != 0){//error in initialization of gcrypt
        QMessageBox msgBox;
        msgBox.setText ("Error in initialization of GNU cryptographic library. Click OK to terminate.");
        msgBox.exec();
        return 0;
    }

    ok = check_lockfile ();
    if (ok == false)//lockfile present, exit
        return 0;
    if (create_lockfile () == false) {//no single-instance protection: abort
        QMessageBox msgBox;
        msgBox.setText (QObject::tr("The program could not create its lock file. "
                                    "The data directory may not be writable, or another "
                                    "instance may be using it. The program will now "
                                    "terminate."));
        msgBox.exec();
        return 1;
    }

    MainWindow mainwindow;
    mainwindow.setWindowTitle(QObject::tr("The Daily Journal"));

    mainwindow.show();

    return app.exec();

}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
//set up the user-interface
{
    //initial window size
    this->resize(900,630);
    this->setMinimumHeight(500);
    this->setMinimumWidth(500);

    //width of the left (calendar/trees) panel, changed by dragging the divider
    leftwidth = 300;
    divider_dragging = false;

    //height of the calendar, changed by dragging the horizontal divider; -1
    //means "auto-fit the current font" (reset on every font change)
    calheight = -1;
    calheight_dragging = false;

    //main menu
    filemenu = menuBar()->addMenu(tr("&File"));

    QAction *chpwd = new QAction (tr("Change pass&word"), this);
    filemenu->addAction(chpwd);
    connect(chpwd, &QAction::triggered, this, &MainWindow::change_password);

    //two distinct exports: the per-day Journal, and the Notes tab (the former
    //"Lists" tab). They used to share the same label, which looked like a
    //duplicate menu entry.
    QAction *savenotes = new QAction (tr("Export &journal as text"), this);
    filemenu->addAction(savenotes);
    connect(savenotes, &QAction::triggered, this, &MainWindow::save_notes_as_text);

    QAction *savelists = new QAction (tr("Export &notes as text"), this);
    filemenu->addAction(savelists);
    connect(savelists, &QAction::triggered, this, &MainWindow::save_lists_as_text);

    QAction *quit = new QAction(tr("E&xit"), this);
    filemenu->addAction(quit);
    connect(quit, &QAction::triggered, this, &MainWindow::save_and_quit);

    helpmenu = menuBar()->addMenu(tr("&Help"));

    QAction *helpitem = new QAction(tr("&Help"), this);
    helpmenu->addAction(helpitem);
    connect(helpitem, &QAction::triggered, this, &MainWindow::help);

    QAction *aboutitem = new QAction(tr("&About"), this);
    helpmenu->addAction(aboutitem);
    connect(aboutitem, &QAction::triggered, this, &MainWindow::about);

    QAction *aboutQtitem = new QAction(tr("About &Qt"), this);
    helpmenu->addAction(aboutQtitem);
    connect(aboutQtitem, &QAction::triggered, qApp, &QApplication::aboutQt);

    //this is the default data subdirectory
    Datadirectory.clear();
    Datadirectory.append(userpath);

    //---------------------LEFT side

    //calendar
    calendar = new QCalendarWidget (this);
    calendar->setGeometry(10, 30, 300, 200);
    calendar->setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);

    calendar->setMinimumDate(QDate(2000, 1, 1));
    calendar->setMaximumDate(QDate(3000, 12, 31));
    calendar->showToday();

    //compact day cells: the style default adds a wide margin around every day,
    //growing the gaps between dates and, with a larger application font,
    //pushing the bottom week rows out of the widget. Shrinking the padding
    //keeps the gaps small; layout_panels() then gives the calendar the full
    //height its content needs so every day of the month is visible.
    calendar->setStyleSheet(
        "QCalendarWidget QAbstractItemView { padding: 3px;"
        " selection-background-color: palette(highlight);"
        " selection-color: palette(highlightedText); }");

    connect (calendar, &QCalendarWidget::selectionChanged, this, &MainWindow::load_day);
    connect (calendar, &QCalendarWidget::currentPageChanged, this, &MainWindow::load_month);

    todaybut.setParent(this);
    todaybut.setGeometry(10, 235, 300, 25);
    todaybut.setToolTip(tr("Click to go to today's date"));
    connect (&todaybut, &QPushButton::clicked, this, &MainWindow::go_to_today);

    //buttons for tree of contacts
    catbut.setParent(this);
    catbut.setGeometry(10, 270, 100, 25);
    catbut.setText (tr("&Group"));
    catbut.setToolTip(tr("Create a new group of contacts"));
    connect (&catbut, &QPushButton::clicked, this, &MainWindow::add_category);

    childbut.setParent(this);
    childbut.setGeometry(120, 270, 90, 25);
    childbut.setText (tr("C&ontact"));
    childbut.setToolTip(tr("Create a new contact within this group"));
    connect (&childbut, &QPushButton::clicked, this, &MainWindow::add_contact);

    delbut.setParent(this);
    delbut.setGeometry(220, 270, 90, 25);
    delbut.setText (tr("&Delete"));
    delbut.setToolTip(tr("Delete this contact"));
    connect (&delbut, &QPushButton::clicked, this, &MainWindow::del_item);

    //tree of contacts
    contree = new QTreeWidget (this);
    contree->setGeometry(10, 300, 300, 290);
    contree->setColumnCount(1);
    connect (contree, &QTreeWidget::itemChanged, this, &MainWindow::save_contact);
    connect (contree, &QTreeWidget::itemClicked, this, &MainWindow::set_contact);

    QStringList contreeheader;
    contreeheader << tr("Contacts");
    contree->setHeaderLabels(contreeheader);

    //buttons for tree of to-do lists
    listbut.setParent(this);
    listbut.setGeometry(10, 270, 100, 25);
    listbut.setText (tr("&New Note"));
    listbut.setToolTip(tr("Create a new note"));
    connect (&listbut, &QPushButton::clicked, this, &MainWindow::add_list);

    delist.setParent(this);
    delist.setGeometry(120, 270, 90, 25);
    delist.setText (tr("&Delete"));
    delist.setToolTip(tr("Delete this note"));
    connect (&delist, &QPushButton::clicked, this, &MainWindow::del_list);

    listbut.hide();
    delist.hide();

    //tree of to-do lists
    listree = new QTreeWidget (this);
    listree->setGeometry(10, 300, 300, 290);
    listree->setColumnCount(1);
    connect (listree, &QTreeWidget::itemChanged, this, &MainWindow::save_list);
    connect (listree, &QTreeWidget::itemClicked, this, &MainWindow::set_list);
    connect (listree, &QTreeWidget::itemActivated, this, &MainWindow::set_list);

    QStringList listreeheader;
    listreeheader << tr("Notes");
    listree->setHeaderLabels(listreeheader);
    listree->hide();

    //draggable divider between the left and right panels
    divider = new QFrame (this);
    divider->setFrameShape (QFrame::VLine);
    divider->setFrameShadow (QFrame::Sunken);
    divider->setCursor (Qt::SplitHCursor);
    divider->setToolTip (tr("Drag to resize the left panel"));
    divider->installEventFilter (this);
    divider->raise();

    //draggable horizontal divider below the Today button: resizes the
    //calendar above it while the button and the tree buttons stay fixed
    hdiv = new QFrame (this);
    hdiv->setObjectName ("hdiv");
    hdiv->setFrameShape (QFrame::HLine);
    hdiv->setFrameShadow (QFrame::Sunken);
    hdiv->setCursor (Qt::SplitVCursor);
    hdiv->setToolTip (tr("Drag to resize the calendar"));
    hdiv->installEventFilter (this);
    hdiv->raise();

    //---------------------RIGHT side

    setuptoolbar ();

    //tabs
    tabcontainer = new QTabWidget;
    tabcontainer->setParent (this);
    tabcontainer->setGeometry(320, 60, 470, 560);
    connect (tabcontainer, &QTabWidget::currentChanged, this, &MainWindow::make_tab_visible);

    //index 0 - notes
    noted = new QWidget ();
    noteditor = new TdjEditor (&m_store, noted);
    connect(noteditor, &QTextEdit::currentCharFormatChanged, this, &MainWindow::currentCharFormatChanged);
    connect(noteditor, &QTextEdit::cursorPositionChanged, this, &MainWindow::cursorPositionChanged);
    connect (noteditor, &QTextEdit::textChanged, this, &MainWindow::save_note);

    fontChanged(noteditor->font());
    colorChanged(noteditor->textColor());
    alignmentChanged(noteditor->alignment());

    noteditor->setGeometry(10, 10, 450, 540);
    tabcontainer->addTab (noted, tr("&Journal"));

    //index 1 - appointments
    appt = new QWidget ();
    apptable = new QTableWidget (appt);
    apptable->setGeometry(10, 10, 450, 540);
    apptable->setRowCount(48);
    apptable->setColumnCount(2);
    apptable->setColumnWidth(0, 100);
    QHeaderView *hdr = apptable->horizontalHeader();
    hdr->setStretchLastSection(true);
    tabcontainer->addTab (appt, tr("A&ppointments"));

    QStringList appheader;
    appheader << tr("Time") << tr("Details");
    apptable->setHorizontalHeaderLabels(appheader);
    connect (apptable, &QTableWidget::itemChanged, this, &MainWindow::save_appt_cell);

    //index 2 - contacts
    contacted = new QWidget ();
    contacteditor = new TdjEditor (&m_store, contacted);
    connect(contacteditor, &QTextEdit::currentCharFormatChanged, this, &MainWindow::currentCharFormatChanged);
    connect(contacteditor, &QTextEdit::cursorPositionChanged, this, &MainWindow::cursorPositionChanged);
    connect (contacteditor, &QTextEdit::textChanged, this, &MainWindow::save_contact);

    fontChanged(contacteditor->font());
    colorChanged(contacteditor->textColor());
    alignmentChanged(contacteditor->alignment());

    contacteditor->setGeometry(10, 10, 450, 540);
    tabcontainer->addTab (contacted, tr("&Contacts"));

    //index 3 - to-do lists
    listed = new QWidget ();
    listeditor = new TdjEditor (&m_store, listed);
    connect(listeditor, &QTextEdit::currentCharFormatChanged, this, &MainWindow::currentCharFormatChanged);
    connect(listeditor, &QTextEdit::cursorPositionChanged, this, &MainWindow::cursorPositionChanged);
    connect (listeditor, &QTextEdit::textChanged, this, &MainWindow::save_list);

    fontChanged(listeditor->font());
    colorChanged(listeditor->textColor());
    alignmentChanged(listeditor->alignment());

    listeditor->setGeometry(10, 10, 450, 540);
    tabcontainer->addTab (listed, tr("&Notes"));

    //index 4 - anniversaries
    anni = new QWidget ();
    anntable = new QTableWidget (anni);
    ComboBoxItemDelegate *cbid = new ComboBoxItemDelegate(anntable);
    anntable->setItemDelegate(cbid);
    anntable->setGeometry(10, 10, 450, 540);
    anntable->setRowCount(366);
    anntable->setColumnCount(3);
    anntable->setColumnWidth(0, 110);
    anntable->setColumnWidth(1, 60);
    hdr = anntable->horizontalHeader();
    hdr->setStretchLastSection(true);
    tabcontainer->addTab (anni, tr("Anni&versaries"));

    QStringList annheader;
    annheader << tr("Month") << tr("Date") << tr("Details");
    anntable->setHorizontalHeaderLabels(annheader);
    connect (anntable, &QTableWidget::itemChanged, this, &MainWindow::save_ann_cell);

    //index 5 - search
    search = new QWidget ();
    tabcontainer->addTab (search, tr("&Search"));

    searchboxlabel = new QLabel;
    searchboxlabel->setParent(search);
    searchboxlabel->setGeometry(10, 10, 100, 25);
    searchboxlabel->setAlignment(Qt::AlignCenter);
    searchboxlabel->setText(tr("Search for"));

    searchtxtbox = new QLineEdit (search);
    searchtxtbox->setGeometry(120, 10, tabcontainer->width()-220, 25);
    searchtxtbox->setMaxLength(50);

    srchbut = new QPushButton (tr("S&earch"), search);
    srchbut->setGeometry(tabcontainer->width()-90, 10, 80, 25);
    srchbut->setToolTip(tr("Click to begin search"));
    connect (srchbut, &QPushButton::clicked, this, &MainWindow::search_data);

    srchresults = new QTextEdit (search);
    srchresults->setReadOnly(true);
    srchresults->setGeometry(10, 45, 450, tabcontainer->height() - 65 - 25);

    srchcursor = srchresults->textCursor();

    //index 6 - preferences
    prefs = new QWidget();
    tabcontainer->addTab (prefs, tr("P&references"));

    QVBoxLayout *vbox1 = new QVBoxLayout;

    create_prefs_weekgrp_box();
    create_prefs_tabgrp_box ();

    headercolbut = new QPushButton (tr("Select calendar h&eader background color"));
    headercolbut->setGeometry(10, 200, 225, 25);
    headercolbut->setToolTip(tr("Click to select a different background color for the day names and the week numbers"));
    connect (headercolbut, &QPushButton::clicked, this, &MainWindow::set_header_color);

    fontbut = new QPushButton (tr("Select a different fon&t for text not within an editor"), prefs);
    fontbut->setGeometry(10, 280, 225, 25);
    fontbut->setToolTip(tr("Click to select a different font for the application"));
    connect (fontbut, &QPushButton::clicked, this, &MainWindow::select_font);

    datadirbut = new QPushButton (tr("Select a different data subdirectory"), prefs);
    datadirbut->setGeometry(10, 280, 225, 25);
    datadirbut->setToolTip(tr("Click to select a different data subdirectory for the application"));
    connect (datadirbut, &QPushButton::clicked, this, &MainWindow::change_directory);

    vbox1->addWidget(weekbox);
    vbox1->addWidget(headercolbut);
    vbox1->addWidget(fontbut);
    vbox1->addWidget(datadirbut);
    vbox1->addWidget(tabbox);

    vbox1->addStretch(1);
    prefs->setLayout(vbox1);

    //status bar
    statustext = new QLabel (this);
    statustext->setGeometry(10, 600, 780, 25);
    statustext->setText(tr("Status messages appear here"));
    statustext->setFrameStyle(QFrame::Plain);
    statustext->setAlignment(Qt::AlignBottom);

    //persistent display of the selected date, at the right end of the bar;
    //unlike statustext it is never overwritten by status messages
    datelabel = new QLabel (this);
    datelabel->setFrameStyle(QFrame::NoFrame);
    datelabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    //change password dialog
    Chpwdialog = new QDialog (this);

    lbl1 = new QLabel(tr("New password:"), Chpwdialog);
    ledt1 = new QLineEdit (Chpwdialog);
    ledt1->setEchoMode(QLineEdit::Password);
    ledt1->setToolTip(tr("Use a complex mix of letters, numbers and special characters"));
    lbl1->setBuddy(ledt1);

    lbl2 = new QLabel(tr("Re-enter new password:"), Chpwdialog);
    ledt2 = new QLineEdit (Chpwdialog);
    ledt2->setEchoMode(QLineEdit::Password);
    ledt2->setToolTip(tr("Re-type the password exactly as in the box above"));
    lbl2->setBuddy(ledt2);

    okbut = new QPushButton(tr("&OK"));
    cnclbut = new QPushButton(tr("&Cancel"));

    QVBoxLayout *vbox = new QVBoxLayout;

    vbox->addWidget(lbl1);
    vbox->addWidget(ledt1);

    vbox->addWidget(lbl2);
    vbox->addWidget(ledt2);

    vbox->addWidget(okbut);
    vbox->addWidget(cnclbut);
    Chpwdialog->setLayout(vbox);

    connect (okbut, &QPushButton::clicked, this, &MainWindow::check_password);
    connect (cnclbut, &QPushButton::clicked, this, &MainWindow::cncl_pwd_change);

    //position the left and right panels
    layout_panels ();

    //read the data files and show the note for today
    initialize ();
}

void MainWindow::initialize()
//read the data files and show the note for today
//get the date for today and initialize some data variables
{
bool ok;
int row;
QString p, s1, s2, s3;
int inivecflag = 0;

    //when the program starts:
    //by default the calendar shows today's date
    //the date_to_show is current_date
    //the month is current_month
    //the year is current year
    //the appointments table shows today's appointments
    //later these values may change if the user selects some other date

    //read the user's preferences
    readprefs();

    //set the Homepath
    Homepath.append(Datadirectory);

    //do not show fortune by default
    fortune = false;

    //determine which format the existing data uses and fix the DB salt
    //(a fresh, per-database salt is created for legacy or new databases)
    TdjDbState dbstate = (TdjDbState) m_crypto.scanDbState(Homepath);
    m_crypto.initDbSalt(Homepath);

    if (setpwd == true) {
        //a brand-new database: the password dialog installs the key and
        //anchors the store set
        change_password();
        setpwd = false;
        ok = true;
    }
    else {
        p = QInputDialog::getText (this, tr("Enter password"), tr("Password"), QLineEdit::Password, tr (""), &ok);
        if (ok && !p.isEmpty()) {//enter or OK button
            password.append (p);
            //same password for the legacy decrypting and encrypting keys
            m_crypto.setLegacyKey (password);

            //derive the TDJ2 session key from the password
            m_crypto.deriveSessionKey (password);

            if (dbstate == TdjDbTdj2) {
                //verify the password against the GCM tag of a store
                if (m_crypto.verifyPassword(Homepath) == false) {
                    QMessageBox msgBox;
                    msgBox.setText (QObject::tr("The password entered does not match the stored data."));
                    msgBox.setInformativeText (QObject::tr("The program will now terminate"));
                    msgBox.exec();
                    delete_lockfile();
                    std::exit (1);
                }
            }
            else if (dbstate == TdjDbLegacy || dbstate == TdjDbMixed) {
                //existing legacy data (or a partially migrated set): the
                //phrase-confirmation dialog gates the old password
                test_password ();
                //one-time migration is completed after the stores are read
                m_crypto.copySessionToOldAndNew ();
            }
        }
        else {//escape or Cancel button
            delete_lockfile();
            std::exit (0);
        }
    }

    //set calendar preferences
    if (grid == true)
        calendar->setGridVisible(true);
    else
        calendar->setGridVisible(false);

    if (weeknum == true)
        calendar->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
    else
        calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    //set calendar header color
    format_headers();

    //set up initial empty contact list
    cur_cat = new QTreeWidgetItem ();
    cur_con = new QTreeWidgetItem ();

    con_item = new QTreeWidgetItem ();
    cur_con = con_item;

    //set up the empty list tree
    cur_list = new QTreeWidgetItem ();
    con_item = new QTreeWidgetItem ();

    //get current day and date
    getdate();
    current_year = year;
    current_month = month;
    current_date = date_to_show;

    //set the label for the button showing today's date
    QDate *d = new QDate (current_year, current_month, current_date);
    todaybut.setText(d->toString());
    delete d;

    //encrypted attachment store (T2): images live here, never as loose
    //plaintext files; load it before shownote() so existing tdj-image: refs
    //render on the very first day
    Attachmentsfilename.append (Homepath);
    Attachmentsfilename.append ("/Attachments.tdj");
    m_store.loadAttachments (m_crypto, Attachmentsfilename, inivecflag);

    //notes file to read
    s1.setNum (year);
    s2.setNum(month);
    Notefilename.append (Homepath);
    if (month < 10) {
        s3 = QString ("/Notes-%1-0%2.tdj").arg(s1).arg(s2);
    }
    else
        s3 = QString ("/Notes-%1-%2.tdj").arg(s1).arg(s2);
    Notefilename.append (s3);
    read_journal_file (inivecflag);

    //display today's journal entry
    shownote ();

    //show the checkboxes in col 1 of the appointments table
    for (row = 0; row < 48; row++) {
        appcol0[row].setCheckState(Qt::Unchecked);
    }

    //appointments file to read
    Appointmentsfilename.append(Homepath);
    if (month < 10) {
        s3 = QString ("/Appointments-%1-0%2.tdj").arg(s1).arg(s2);
    }
    else
        s3 = QString ("/Appointments-%1-%2.tdj").arg(s1).arg(s2);
    Appointmentsfilename.append (s3);
    read_appt_file (inivecflag);

    //daily appointments file to read
    DailyAppointmentsfilename.append(Homepath);
    s3 = QString ("/DailyAppointments.tdj");
    DailyAppointmentsfilename.append (s3);
    read_daily_appt_file ();

    //populate the appointment table items (appcols) with today's appointments,
    //if there are any, or with empty strings, otherwise
    fill_appointment_items ();

    //assign the items to the table
    for (row = 0; row < 48; row++) {
        apptable->setItem(row, 0, &appcol0[row]);
        apptable->setItem(row, 1, &appcol1[row]);

        appcol0[row].setToolTip(tr ("Type a time between 0 and 2400 hrs.\nCheck the box to repeat this appointment everyday"));
        appcol1[row].setToolTip(tr ("Click and type a description for this appointment"));
    }

    catflag = 0;
    contreeempty = true;

    //contacts file to read
    Contactfilename.append (Homepath);
    Contactfilename.append ("/Contacts.tdj");
    read_contacts ();

    //set the first category in contacts as the current category
    if (contreeempty == false) {
        cur_cat = contree->topLevelItem(0);
        contree->setCurrentItem(cur_cat);
        contacteditor->setHtml(import_legacy_images(cur_cat->text(1)));
        cur_con = cur_cat;
        catflag = 1;
    }
    else {
        con_item = new QTreeWidgetItem ();
        cur_cat = con_item;
    }

    listreeempty = true;

    //lists file to read
    Listfilename.append (Homepath);
    Listfilename.append ("/Lists.tdj");
    read_lists ();

    //set the first list as the current list
    if (listreeempty == false) {
        cur_list = listree->topLevelItem(0);
        listree->setCurrentItem(cur_list);
        listeditor->setHtml(cur_list->text(1));
    }
    else {
        cur_list = con_item;
    }

    //sort the list
    listree->setSortingEnabled(true);
    listree->sortByColumn(0, Qt::AscendingOrder);
    listree->setSortingEnabled(false);

    //anniversary file to read
    Anniversaryfilename.append(Homepath);
    Anniversaryfilename.append("/Anniversaries.tdj");
    read_ann_file ();

    //all stores are now in memory; migrate any legacy database in place
    if (dbstate == TdjDbLegacy || dbstate == TdjDbMixed) {
        reencrypt_all_stores ();
        //the TDJ2 session key is now in effect for all future saves
        m_crypto.commitNewToSession ();
        //the legacy password file served its purpose
        QFile::remove(Homepath + "/Checkpwd.tdj");
        //reencrypt_all_stores() left the buffers holding the last month file
        //it walked; reload the month actually on screen before any save can
        //copy another month's notes/appointments into it
        reload_current_month ();
    }

    sort_anniversaries();

    //populate anniversary items, if there are any, or with empty strings otherwise
    fill_anniversary_items ();

    //initialize anntable with table items
    for (row = 0; row < 367; row++) {
        anntable->setItem(row, 0, &anncol0[row]);
        anntable->setItem(row, 1, &anncol1[row]);
        anntable->setItem(row, 2, &anncol2[row]);

        anncol0[row].setToolTip(tr("Double click to select the month"));
        anncol1[row].setToolTip(tr("Double click to select the date"));
        anncol2[row].setToolTip(tr("Click and type a description for this anniversary"));
    }

    //help file to read
    Helpfilename.append (Homepath);
    Helpfilename.append ("/tdjhelp.pdf");

    //when the program starts the notes tab is in the foreground
    tabcontainer->setCurrentIndex(tabstart);

    //"COPYING" file to read
    Gnugplfilename.append (Homepath);
    Gnugplfilename.append ("/COPYING");

    format_notes();
    format_appointments();
    format_anniversaries ();

    set_appt_time_array();
    set_next_appointment_timer();

    //loading today's note (setHtml) fires the editor's textChanged signal,
    //which reports "Note saved to buffer"; leave the status line blank at
    //startup and let the first real edit repaint it
    statustext->clear();

    return;
}

void MainWindow::save_note ()
//transfer user data from editor to note array
{
    //save note to memory
    m_store.note(date_to_show).data = noteditor->toHtml();

    //compute the plain-text emptiness flag cheaply, without re-serialising
    //the whole document (this runs on every keypress)
    bool hadText = m_store.note(date_to_show).hasText;
    m_store.note(date_to_show).hasText = false;
    QTextBlock b = noteditor->document()->begin();
    while (b.isValid()) {
        if (!b.text().isEmpty()) {
            m_store.note(date_to_show).hasText = true;
            break;
        }
        b = b.next();
    }
    //a note that just lost all its text may have orphaned an image reference
    if (hadText && !m_store.note(date_to_show).hasText)
        m_attachmentsDirty = true;

    //recolour only the edited date - recalculating the whole month on every
    //keystroke used to build a QTextDocument for each day of the month. The
    //single-date recolor reapplies the yellow>cyan>lightGray>white precedence,
    //so displaying the journal never wipes an anniversary/appointment colour.
    recolor_calendar_date(date_to_show);

    statustext->setText(tr("Note saved to buffer"));
}

void MainWindow::save_notes_and_appts()
{
int inivecflag = 0;

    get_appointment_items ();

    write_journal_file (inivecflag);//saves all notes
    write_appt_file (inivecflag);//saves all appointments
    write_daily_appt_file(inivecflag);//saves the appointments that repeat daily

    //persist the attachment store and drop orphaned images only after an
    //import or an emptied note marked it dirty (the orphan scan reads every
    //month file, so it must not run on every tab switch)
    if (m_attachmentsDirty) {
        prune_orphan_attachments ();
        m_store.saveAttachments (m_crypto, Attachmentsfilename, 0);
        m_attachmentsDirty = false;
    }
}

void MainWindow::save_other_data()
//transfer user data from editor to note array
//save data to files
{
int inivecflag = 0;

    get_anniversary_items();

    write_ann_file(inivecflag);//save anniversaries
    write_contacts (inivecflag);//save contacts
    write_lists (inivecflag);//save lists
}

void MainWindow::save_and_quit()
{
    MainWindow::close();
}

void MainWindow::shownote()
//load the journal entry for the selected date into the noteditor
{
int i;
QTextStream *in;
QTextDocument *doc;
QString s;


    note_to_show.clear();

    //migrate legacy <img src="/abs/path"> stored in old notes into the
    //encrypted attachment store; the editor then renders them via tdj-image:
    //and the next save persists the rewritten html
    note_to_show.append(import_legacy_images(m_store.note(date_to_show).data));

    s.clear();
    doc = new QTextDocument ();
    doc->setHtml(note_to_show);
    //strip HTML formatting
    s = doc->toPlainText();
    //all this to find the actual length of the note
    i = s.length();

    if (i != 0) {//display journal entry
        //note_to_show already holds the (possibly image-migrated) html
        noteditor->setHtml(note_to_show);
    }
    else if (fortune == true) {//empty note, show a fortune
        FILE *f;
        f = popen ("fortune", "r");

        in = new QTextStream (f);
        QString s = in->readAll();

        note_to_show.append(s);
        noteditor->setHtml(s);

        pclose (f);
    }
    else {//show empty editor window
        noteditor->setHtml(note_to_show);
    }

    datelabel->setText(calendar->selectedDate().toString("ddd d MMM yyyy"));
}

void MainWindow::load_day()
//display data for another date
{
    get_appointment_items ();

    getdate ();
    shownote ();
    fill_appointment_items ();
}

void MainWindow::load_month()
//read data for another month and display it
{
QString s1, s2, s3;
int inivecflag = 0;

    get_appointment_items ();
    write_journal_file (inivecflag);
    write_appt_file (inivecflag);

    //get the new month and year displayed on calendar
    getdate();

    //notes file to read
    Notefilename.clear();
    s1.setNum (year);
    s2.setNum(month);
    Notefilename.append (Homepath);
    if (month < 10) {
        s3 = QString ("/Notes-%1-0%2.tdj").arg(s1).arg(s2);
    }
    else
        s3 = QString ("/Notes-%1-%2.tdj").arg(s1).arg(s2);
    Notefilename.append (s3);

    Appointmentsfilename.clear();
    Appointmentsfilename.append(Homepath);
    if (month < 10) {
        s3 = QString ("/Appointments-%1-0%2.tdj").arg(s1).arg(s2);
    }
    else
        s3 = QString ("/Appointments-%1-%2.tdj").arg(s1).arg(s2);
    Appointmentsfilename.append (s3);

    read_journal_file (inivecflag);
    read_appt_file (inivecflag);

    shownote();
    fill_appointment_items ();

    format_notes();
    format_appointments();
    format_anniversaries ();
}

void MainWindow::reload_current_month()
//re-read the currently displayed month into the in-memory buffers and refresh
//the editor and tables. Must be called after a re-encryption (migration or
//password change) commits the new key: reencrypt_all_stores() walks every
//month file and leaves note[]/appointment[] holding the *last file processed*,
//so without this reload the next save would write that foreign month's data
//into the displayed month's file.
{
    if (Notefilename.isEmpty() == true)
        return;//no month selected yet (fresh-database setup)

    read_journal_file (0);
    read_appt_file (0);

    shownote();
    fill_appointment_items ();

    format_notes();
    format_appointments();
}

void MainWindow::make_tab_visible(int i)
//show the tab which has been selected and adjust the tree-related buttons if required
{
    tabcontainer->setCurrentIndex(i);

    //in editor, enable toolbar
    if ((i == 0) || (i == 2) || (i == 3))
        tb->setEnabled(true);
    else//disable it
        tb->setDisabled(true);

    if (tabcontainer->currentWidget() == listed) {
        listree->show();
        contree->hide();

        catbut.hide();
        childbut.hide();
        delbut.hide();

        listbut.show();
        delist.show();

        show_list();
    }
    if (tabcontainer->currentWidget() == contacted) {
        listree->hide();
        contree->show();

        catbut.show();
        childbut.show();
        delbut.show();

        listbut.hide();
        delist.hide();

        show_contact();
    }

}

void MainWindow::change_directory()
//this changes the default data directory
{
QString dir;
QMessageBox msgBox;

    dir = QFileDialog::getExistingDirectory(this,
        tr("Select the new data directory"), Datadirectory);
    if (dir.isEmpty())
        return;//cancelled - nothing was changed

    if (QDir().mkpath(dir) == false) {//cannot create the directory
        msgBox.setText (tr("The directory could not be created."));
        msgBox.exec();
        return;
    }

    Datadirectory = dir;
    writeprefs();//persist the new directory via Defdatadir

    msgBox.setWindowTitle(tr("Data directory changed"));
    msgBox.setText (tr("The new data directory will be used the next time "
                       "the program starts.\nExisting data is not copied or "
                       "moved; the new directory must contain a database "
                       "(or will be created empty)."));
    msgBox.exec();
}

void MainWindow::change_password()
{
int i;
QString s;

    //save nothing yet: the key under which the new stores are written is
    //only known once the dialog has been accepted
    ledt1->clear();
    ledt2->clear();
    //execute the dialog
    Chpwdialog->exec();
    //check the result
    i = Chpwdialog->result();

    if (i == QDialog::Accepted) {
        if (setpwd == true) {
            //brand-new database: nothing exists yet. Derive the key from the
            //confirmed password and anchor a valid, empty TDJ2 store set
            m_crypto.deriveSessionKey(ledt1->text());
            m_crypto.copySessionToOldAndNew ();
            reencrypt_all_stores ();
        }
        else {
            //password change: the in-memory data is still under the old key,
            //so write it out first, then re-encrypt every store on disk
            save_notes_and_appts();
            save_other_data();
            m_crypto.copySessionToOld ();
            m_crypto.deriveIntoNewKey(ledt1->text());
            reencrypt_all_stores ();
            //the new password is now in effect
            m_crypto.commitNewToSession ();
            //reload the displayed month: reencryption left the buffers at the
            //last file it walked
            reload_current_month ();
        }

        s = QString (tr("Password changed"));
    }
    else if (i == QDialog::Rejected) {
        if (setpwd == true) {
            clean_up_and_quit();
        }
    }
    else {
        s = QString (tr("Password unchanged"));
    }
    statustext->setText(s);
}

void MainWindow::check_password()//user clicked OK
{
QString s1, s2, serr;
QMessageBox msgBox;

    s1 = ledt1->text();
    s2 = ledt2->text();

    if (s1 != s2) {
        serr.clear();
        serr.append (QObject::tr("Passwords do not match."));
        msgBox.setText(serr);
        msgBox.exec();
        if (setpwd == false)
            Chpwdialog->reject();
        else
            clean_up_and_quit();
    }
    else if ((s1 == s2) && (s1.isEmpty() == true)) {
        serr.clear();
        serr.append (QObject::tr("The password cannot be empty"));
        msgBox.setText(serr);
        msgBox.exec();
        if (setpwd == false)
            Chpwdialog->reject();
        else
            clean_up_and_quit();
    }
    else {
        serr.clear();
        serr.append (QObject::tr("All the data will be encrypted with this password. "));
        serr.append (QObject::tr("Enter this password the next time you run The Daily Journal. "));
        serr.append (QObject::tr("There is no way to recover the data if it is forgotten."));
        msgBox.setText(serr);
        msgBox.exec();

        Chpwdialog->accept();
    }
}

void MainWindow::cncl_pwd_change()//user clicked cancel
{
QString serr;
QMessageBox msgBox;

    serr.clear();
    if (setpwd == false) {
        serr.append (QObject::tr("Password unchanged."));
        msgBox.setText(serr);
        msgBox.exec();
        Chpwdialog->reject();
    }
    else
        clean_up_and_quit ();
}

void clean_up_and_quit ()
{
QString qtpath, serr;
QMessageBox msgBox;
QDir qtdir;

    qtpath.append (userpath);
    qtdir = QDir (qtpath);

    serr.append (QObject::tr("The program cannot continue without a password. "));
    serr.append (QObject::tr("Click OK to exit."));
    msgBox.setText(serr);
    msgBox.exec();

    delete_lockfile();
    qtdir.rmdir(qtpath);
    std::exit(0);
}
