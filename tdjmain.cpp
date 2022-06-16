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

//Last modified 26 November 2014

#define TESTING 0

#include <QHeaderView>
#include <QInputDialog>

#include "tdj.h"
#include <gcrypt.h>

extern char OldaesSymKey[];//old password
extern char NewaesSymKey[];//new password

extern char iniVector[], newiniVector[];

extern size_t blkLength;

extern int initialize_gcrypt (void);

//notes for the month on display, begins from 1
Note note[32];
//appointments array for the month on display, begins from 1
Appointment appointment[32];
Appointment dailyappt;
//anniversaries array, begins from 1
Anniversary anniversary [367];

//userpath contains the path to the data subdirectory
QString Lockfilename, userpath;
bool setpwd = false;

bool check_lockfile(void);
void create_lockfile ();
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

    QTranslator appTranslator;
    appTranslator.load("tdj4_" + QLocale::system().name(), qApp->applicationDirPath());
    app.installTranslator(&appTranslator);

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(), qApp->applicationDirPath());
    app.installTranslator(&qtTranslator);

    //get the path to the user's home directory
    userpath.clear();
#ifdef Q_OS_WIN
    //c:\Users\{account-name}
    userpath.append (getenv ("USERPROFILE"));
#else
    //home/{account-name}
    userpath.append (getenv ("HOME"));
#endif

    if (TESTING == 1) {
        userpath.append("/.tdjenctest");//home/{account-name}/.tdjenctest
    }
    else {
        userpath.append("/.cryptdj");//home/{account-name}/.cryptdj
    }

    //check for the tdj data directory and create it if required
    check_qtdata_dir();

    //before constructing the mainwindow check if a lockfile is present
    Lockfilename.append (userpath);
    Lockfilename.append ("/tdjlockfile.tdj");

    i = initialize_gcrypt ();
    if (i != 0){//error in initialization of gcrypt
        QMessageBox msgBox;
        msgBox.setText ("Error in initialization of GNU cryptographic library. Click OK to terminate.");
        msgBox.exec();
        return 0;
    }

    if (TESTING == 0) {
        ok = check_lockfile ();
        if (ok == false)//lockfile present, exit
            return 0;
    }
    create_lockfile ();

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

    //main menu
    filemenu = menuBar()->addMenu(tr("&File"));

    QAction *chpwd = new QAction (tr("Change pass&word"), this);
    filemenu->addAction(chpwd);
    connect(chpwd, SIGNAL(triggered()), this, SLOT(change_password()));

    QAction *savenotes = new QAction (tr("Export &notes as text"), this);
    filemenu->addAction(savenotes);
    connect(savenotes, SIGNAL(triggered()), this, SLOT(save_notes_as_text()));

    QAction *savelists = new QAction (tr("Export &lists as text"), this);
    filemenu->addAction(savelists);
    connect(savelists, SIGNAL(triggered()), this, SLOT(save_lists_as_text()));

    QAction *quit = new QAction(tr("E&xit"), this);
    filemenu->addAction(quit);
    connect(quit, SIGNAL(triggered()), this, SLOT(save_and_quit()));

    helpmenu = menuBar()->addMenu(tr("&Help"));

    QAction *helpitem = new QAction(tr("&Help"), this);
    helpmenu->addAction(helpitem);
    connect(helpitem, SIGNAL(triggered()), this, SLOT(help()));

    QAction *aboutitem = new QAction(tr("&About"), this);
    helpmenu->addAction(aboutitem);
    connect(aboutitem, SIGNAL(triggered()), this, SLOT(about()));

    QAction *aboutQtitem = new QAction(tr("About &Qt"), this);
    helpmenu->addAction(aboutQtitem);
    connect(aboutQtitem, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    //---------------------LEFT side

    //calendar
    calendar = new QCalendarWidget (this);
    calendar->setGeometry(10, 30, 300, 200);
    calendar->setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);

    calendar->setMinimumDate(QDate(2000, 1, 1));
    calendar->setMaximumDate(QDate(3000, 12, 31));
    calendar->showToday();

    connect (calendar, SIGNAL(selectionChanged()), this, SLOT(load_day()));
    connect (calendar, SIGNAL(currentPageChanged(int, int)), this, SLOT(load_month()));

    todaybut.setParent(this);
    todaybut.setGeometry(10, 235, 300, 25);
    todaybut.setToolTip(tr("Click to go to today's date"));
    connect (&todaybut, SIGNAL(clicked(bool)), this, SLOT(go_to_today()));

    //buttons for tree of contacts
    catbut.setParent(this);
    catbut.setGeometry(10, 270, 100, 25);
    catbut.setText (tr("&Group"));
    catbut.setToolTip(tr("Create a new group of contacts"));
    connect (&catbut, SIGNAL(clicked(bool)), this, SLOT(add_category()));

    childbut.setParent(this);
    childbut.setGeometry(120, 270, 90, 25);
    childbut.setText (tr("C&ontact"));
    childbut.setToolTip(tr("Create a new contact within this group"));
    connect (&childbut, SIGNAL(clicked(bool)), this, SLOT(add_contact()));

    delbut.setParent(this);
    delbut.setGeometry(220, 270, 90, 25);
    delbut.setText (tr("&Delete"));
    delbut.setToolTip(tr("Delete this contact"));
    connect (&delbut, SIGNAL(clicked(bool)), this, SLOT(del_item()));

    //tree of contacts
    contree = new QTreeWidget (this);
    contree->setGeometry(10, 300, 300, 290);
    contree->setColumnCount(1);
    connect (contree, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(save_contact ()));
    connect (contree, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(set_contact (QTreeWidgetItem *)));

    QStringList contreeheader;
    contreeheader << tr("Contacts");
    contree->setHeaderLabels(contreeheader);

    //buttons for tree of to-do lists
    listbut.setParent(this);
    listbut.setGeometry(10, 270, 100, 25);
    listbut.setText (tr("&New List"));
    listbut.setToolTip(tr("Create a new list"));
    connect (&listbut, SIGNAL(clicked(bool)), this, SLOT(add_list()));

    delist.setParent(this);
    delist.setGeometry(120, 270, 90, 25);
    delist.setText (tr("&Delete"));
    delist.setToolTip(tr("Delete this list"));
    connect (&delist, SIGNAL(clicked(bool)), this, SLOT(del_list()));

    listbut.hide();
    delist.hide();

    //tree of to-do lists
    listree = new QTreeWidget (this);
    listree->setGeometry(10, 300, 300, 290);
    listree->setColumnCount(1);
    connect (listree, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(save_list ()));
    connect (listree, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(set_list (QTreeWidgetItem *)));
    connect (listree, SIGNAL(itemActivated(QTreeWidgetItem*,int)), this, SLOT(set_list (QTreeWidgetItem *)));

    QStringList listreeheader;
    listreeheader << tr("Lists");
    listree->setHeaderLabels(listreeheader);
    listree->hide();

    //---------------------RIGHT side

    setuptoolbar ();

    //tabs
    tabcontainer = new QTabWidget;
    tabcontainer->setParent (this);
    tabcontainer->setGeometry(320, 60, 470, 560);
    connect (tabcontainer, SIGNAL(currentChanged(int)), this, SLOT(make_tab_visible(int)));

    //index 0 - notes
    noted = new QWidget ();
    noteditor = new QTextEdit (noted);
    connect(noteditor, SIGNAL(currentCharFormatChanged(QTextCharFormat)), this, SLOT(currentCharFormatChanged(QTextCharFormat)));
    connect(noteditor, SIGNAL(cursorPositionChanged()), this, SLOT(cursorPositionChanged()));
    connect (noteditor, SIGNAL(textChanged()), this, SLOT(save_note()));

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
    connect (apptable, SIGNAL(itemChanged(QTableWidgetItem *)), this, SLOT(save_appt_cell (QTableWidgetItem *)));

    //index 2 - contacts
    contacted = new QWidget ();
    contacteditor = new QTextEdit (contacted);
    connect(contacteditor, SIGNAL(currentCharFormatChanged(QTextCharFormat)), this, SLOT(currentCharFormatChanged(QTextCharFormat)));
    connect(contacteditor, SIGNAL(cursorPositionChanged()), this, SLOT(cursorPositionChanged()));
    connect (contacteditor, SIGNAL(textChanged()), this, SLOT(save_contact()));

    fontChanged(contacteditor->font());
    colorChanged(contacteditor->textColor());
    alignmentChanged(contacteditor->alignment());

    contacteditor->setGeometry(10, 10, 450, 540);
    tabcontainer->addTab (contacted, tr("&Contacts"));

    //index 3 - to-do lists
    listed = new QWidget ();
    listeditor = new QTextEdit (listed);
    connect(listeditor, SIGNAL(currentCharFormatChanged(QTextCharFormat)), this, SLOT(currentCharFormatChanged(QTextCharFormat)));
    connect(listeditor, SIGNAL(cursorPositionChanged()), this, SLOT(cursorPositionChanged()));
    connect (listeditor, SIGNAL(textChanged()), this, SLOT(save_list()));

    fontChanged(listeditor->font());
    colorChanged(listeditor->textColor());
    alignmentChanged(listeditor->alignment());

    listeditor->setGeometry(10, 10, 450, 540);
    tabcontainer->addTab (listed, tr("&Lists"));

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
    connect (anntable, SIGNAL(itemChanged(QTableWidgetItem *)), this, SLOT(save_ann_cell (QTableWidgetItem *)));

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
    connect (srchbut, SIGNAL(clicked()), this, SLOT(search_data()));

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
    connect (headercolbut, SIGNAL(clicked  ()), this, SLOT(set_header_color()));

    fontbut = new QPushButton (tr("Select a different fon&t for text not within an editor"), prefs);
    fontbut->setGeometry(10, 280, 225, 25);
    fontbut->setToolTip(tr("Click to select a different font for the application"));
    connect (fontbut, SIGNAL(clicked  ()), this, SLOT(select_font()));

    vbox1->addWidget(weekbox);
    vbox1->addWidget(headercolbut);
    vbox1->addWidget(fontbut);
    vbox1->addWidget(tabbox);

    vbox1->addStretch(1);
    prefs->setLayout(vbox1);

    //status bar
    statustext = new QLabel (this);
    statustext->setGeometry(10, 600, 780, 25);
    statustext->setText(tr("Status messages appear here"));
    statustext->setFrameStyle(QFrame::Plain);
    statustext->setAlignment(Qt::AlignBottom);

    //change password dialog
    Chpwdialog = new QDialog (this);

    lbl1 = new QLabel(tr("New password:"), Chpwdialog);
    ledt1 = new QLineEdit (Chpwdialog);
    ledt1->setEchoMode(QLineEdit::Password);
    ledt1->setMaxLength(blkLength);
    ledt1->setToolTip(tr("16 characters, maximum. Use a complex mix of letters, numbers and special characters"));
    lbl1->setBuddy(ledt1);

    lbl2 = new QLabel(tr("Re-enter new password:"), Chpwdialog);
    ledt2 = new QLineEdit (Chpwdialog);
    ledt2->setEchoMode(QLineEdit::Password);
    ledt2->setMaxLength(blkLength);
    ledt2->setToolTip(tr("Re-type the password exactly as in the box above"));
    lbl2->setBuddy(ledt2);

    lbl3 = new QLabel(tr("Phrase to encode:"), Chpwdialog);
    ledt3 = new QLineEdit (Chpwdialog);
    ledt3->setEchoMode(QLineEdit::Normal);
    ledt3->setToolTip(tr("Enter a complex phrase or string of characters"));
    lbl3->setBuddy(ledt3);

    okbut = new QPushButton(tr("&OK"));
    cnclbut = new QPushButton(tr("&Cancel"));

    QVBoxLayout *vbox = new QVBoxLayout;

    vbox->addWidget(lbl1);
    vbox->addWidget(ledt1);

    vbox->addWidget(lbl2);
    vbox->addWidget(ledt2);

    vbox->addWidget(lbl3);
    vbox->addWidget(ledt3);

    vbox->addWidget(okbut);
    vbox->addWidget(cnclbut);
    //QRect *rect = new QRect (10, 10, 450, 450);
    //vbox->setGeometry(*rect);
    Chpwdialog->setLayout(vbox);

    connect (okbut, SIGNAL(clicked()), this, SLOT(check_password()));
    connect (cnclbut, SIGNAL(clicked()), this, SLOT(cncl_pwd_change()));

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

    //set the Homepath
    Homepath.append(userpath);
    //set the working directory to the data subdirectory
    QDir::setCurrent(Homepath);

    //do not show fortune by default
#ifdef Q_OS_LINUX
    fortune = false;
#endif

    if (setpwd == true) {
        change_password();
        setpwd = false;
        ok = true;
    }
    else {
        p = QInputDialog::getText (this, tr("Enter password"), tr("Password"), QLineEdit::Password, tr (""), &ok);
        if (ok && !p.isEmpty()) {//enter or OK button
            password.append (p);
            if (password.length() > 16) {
                password.chop (password.length() - 16);
            }
            //for decrypting
            strcpy (OldaesSymKey, password.toUtf8().data());
            //for encrypting
            strcpy (NewaesSymKey, password.toUtf8().data());
            test_password ();
        }
        else {//escape or Cancel button
            delete_lockfile();
            std::exit (0);
        }
    }

    //read the user's preferences
    readprefs();

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
        contacteditor->setPlainText(cur_cat->text(1));
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

    strcpy (newiniVector, iniVector);

    return;
}

void MainWindow::save_note ()
//transfer user data from editor to note array
{
    note[date_to_show].data.clear();
    note[date_to_show].length = 0;

    note_to_show = noteditor->toHtml();
    if (note_to_show.isEmpty() == true){//do nothing
        ;
    }
    else {//save note to memory
        note[date_to_show].data.append(note_to_show);
        note[date_to_show].length = note_to_show.length();

        statustext->setText(tr("Note saved to buffer"));
    }

    format_notes();
    format_appointments();
    format_anniversaries ();

}

void MainWindow::save_notes_and_appts()
{
int inivecflag = 0;

    get_appointment_items ();

    write_journal_file (inivecflag);//saves all notes
    write_appt_file (inivecflag);//saves all appointments
    write_daily_appt_file(inivecflag);//saves the appointments that repeat daily
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

    s.clear();
    doc = new QTextDocument ();
    doc->setHtml(note[date_to_show].data);
    //strip HTML formatting
    s = doc->toPlainText();
    //all this to find the actual length of the note
    i = s.length();

    if (i != 0) {//display journal entry
        note_to_show.append(note[date_to_show].data);
        noteditor->setHtml(note_to_show);
    }
#ifdef Q_OS_LINUX
    else if (fortune == true) {//empty note, show a fortune
        FILE *f;
        f = popen ("fortune", "r");

        in = new QTextStream (f);
        QString s = in->readAll();

        note_to_show.append(s);
        noteditor->setHtml(s);

        pclose (f);
    }
#endif
    else {//show empty editor window
        noteditor->setHtml(note_to_show);
    }

    statustext->setText((calendar->selectedDate().toString()));
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

void MainWindow::change_password()
{
int i;
QString s;

    //save any unsaved data
    save_notes_and_appts();
    save_other_data();

    //clear the text fields
    ledt1->clear();
    ledt2->clear();
    ledt3->clear();
    //execute the dialog
    Chpwdialog->exec();
    //check the result
    i = Chpwdialog->result();

    if (i == QDialog::Accepted) {
        //create a new random initialization vector
        gcry_create_nonce ((unsigned char *) newiniVector, blkLength);
        //re-read all the notes and appt data files and decrypt using iniVector and OldaesSymkey
        //write the data after encrytion using newiniVector and NewaesSymkey
        re_encrypt_notes_appt_data ();
        //now the new password and newiniVector is in effect so:
        strcpy (OldaesSymKey, NewaesSymKey);
        strcpy (iniVector, newiniVector);
        //everything else gets saved with the new password and iniVector
        save_other_data();
        //encrypt the phrase entered by the user and store it in a file
        make_password_test_file(phrase_to_encode);

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
QString s1, s2, s3, serr;
QMessageBox msgBox;

    s1 = ledt1->text();
    s2 = ledt2->text();
    s3 = ledt3->text();

    if (s3.isEmpty() == true){
        serr.clear();
        serr.append (QObject::tr("None of the fields can be empty"));
        msgBox.setText(serr);
        msgBox.exec();
        if (setpwd == false)
            Chpwdialog->reject();
        else
            clean_up_and_quit();
    }
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
    else if ((s1 == s2) && (s1.isEmpty() == false) && (s3.isEmpty() == false)) {
        serr.clear();
        serr.append (QObject::tr("All the data will be encrypted with this password. "));
        serr.append (QObject::tr("Enter this password the next time you run The Daily Journal "));
        serr.append (QObject::tr("and confirm the decoded phrase when it is displayed."));
        msgBox.setText(serr);
        msgBox.exec();

        if (s1.length() > 16)
            s1.chop (s1.length() - 16);

        strcpy (NewaesSymKey, s1.toUtf8().data());
        phrase_to_encode.clear();
        phrase_to_encode.append(s3);

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
