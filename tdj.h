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

//Last modified 5 July 2020

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <stdio.h>
#include <string.h>

#include <QWidget>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QCalendarWidget>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileInfo>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QFontDialog>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QComboBox>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QColorDialog>
#include <QColor>
#include <QTextCursor>
#include <QToolBar>
#include <QTextCharFormat>
#include <QFontComboBox>
#include <QTranslator>

namespace Ui {
class MainWindow;
}

//notes for the month on display, begins from 1
class Note
{
public:
    //length of the text stored in the note
    //length does not include the NULL char at the end
    int length;
    QString data;
};

//appointments array for the month on display, begins from 1
class Appointment
{
public:
    //no. of appointments for this day
    int total;
    //time and description data for each appointment, from 1-48, 0 is not used
    QString apptime[49], apptdesc[49];
};

//this is one row of a two column table, used for sorting the appointments
class Tablerow {
public:
    QString col0, col1;
    bool chkstate;
};

class Anniversary
{
public:
    QString date, month, description;
};

class ComboBoxItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    ComboBoxItemDelegate(QObject *parent = 0);
    ~ComboBoxItemDelegate();

    virtual QWidget *createEditor( QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index ) const;
    virtual void setEditorData ( QWidget *editor, const QModelIndex &index ) const;
    virtual void setModelData ( QWidget *editor, QAbstractItemModel *model, const QModelIndex &index ) const;

    //friend class MainWindow;
};

class MainWindow : public QMainWindow
{

    QMenu *filemenu, *helpmenu;

    //toolbar and its contents
    QToolBar *tb;
    QAction *actionTextBold, *actionTextColor, *actionTextItalic, *actionTextUnderline;
    QAction *actionAlignLeft, *actionAlignCenter, *actionAlignRight, *actionAlignJustify;
    QAction *actionInsertImage;

    QFontComboBox *comboFont;
    QComboBox *comboSize;
    QCalendarWidget *calendar;
    QPushButton todaybut;
    QColor headercolor;
    int headerred, headergreen, headerblue;

    int year, month, date_to_show;
    int current_year, current_month, current_date;

    QTreeWidget *contree, *listree;
    QTreeWidgetItem *cur_con, *cur_cat, *con_item;
    QPushButton catbut, childbut, delbut;
    bool contreeempty;
    int catflag;

    QTreeWidgetItem *cur_list;
    QPushButton listbut, delist;
    bool listreeempty;

    QTabWidget *tabcontainer;
    QWidget *noted,  *contacted, *listed, *appt, *anni, *search, *prefs;
    QTextEdit *noteditor, *contacteditor, *listeditor;
    QTableWidget *apptable, *anntable;

    //text in editor
    QString note_to_show, list_to_show;

    //items for appointments table
    QTableWidgetItem appcol0[48], appcol1[48];
    //this stores all the 48 appt times for today, some may be zero.
    int appt_time_array[48];

    //items for anniversaries table
    QTableWidgetItem anncol0[367], anncol1[367], anncol2[367];
    int max_anniversaries;

    //search
    QGroupBox *searchbox;
    QLabel *searchboxlabel;
    QLineEdit *searchtxtbox;
    QPushButton *srchbut;
    QTextEdit *srchresults;
    QString srchtxt;
    QTextCursor srchcursor;

    //preferences
    QGroupBox *weekbox, *tabbox;
    QRadioButton *sun, *mon;

    QCheckBox *gridbox, *weeknumbox, *fortunebox;
    bool grid, weeknum, fortune;

    QRadioButton *t1, *t2, *t3, *t4;

    uint weekstrt, tabstart;

    QPushButton *fontbut;
    QPushButton *headercolbut;
    QFont curfont;

    QLabel *statustext;

    //password
    QString password, phrase_to_encode;
    QString old_password, new_password;

    QDialog *Chpwdialog, *Cnfdialog;
    QLabel *lbl1, *lbl2, *lbl3, *lbl4;
    QLineEdit *ledt1, *ledt2, *ledt3, *ledt4;
    QPushButton *okbut, *cnclbut;
    QPushButton *ok2but, *cncl2but;

    //stores the path to the data subdirectory
    QString Homepath;

    //data files
    QString Notefilename;
    QString Anniversaryfilename;
    QString Appointmentsfilename;
    QString DailyAppointmentsfilename;
    QString Contactfilename;
    QString Listfilename;
    QString Gnugplfilename;
    QString Helpfilename;

    void resizeEvent(QResizeEvent *);

    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    friend class ComboBoxItemDelegate;

public slots:
    //menu
    void change_password();
    void save_notes_as_text();
    void save_lists_as_text();
    void save_and_quit();

    void help ();
    void about ();

    //toolbar
    void setuptoolbar();

    void textBold();
    void textItalic();
    void textUnderline();
    void textAlign(QAction*);
    void textColor();

    void currentCharFormatChanged(const QTextCharFormat &format);
    void fontChanged(const QFont &f);
    void colorChanged(const QColor &c);
    void alignmentChanged(Qt::Alignment a);
    void cursorPositionChanged();

    void textFamily(const QString &f);
    void textSize(const QString &f);

    void mergeFormatOnWordOrSelection(const QTextCharFormat &format);

    void insertImage();

    //calendar
    void getdate();
    QString get_month_name (int);
    void format_appointments();
    void format_anniversaries ();
    void format_notes();
    void format_headers();
    void set_header_color();
    void go_to_today();

    //noteditor
    void shownote ();
    void save_note ();

    void read_journal_file (int inivecflag);
    void write_journal_file (int inivecflag);

    //void cl_read_journal_file ();
    //void cl_write_journal_file ();

    //appointments table
    void fill_appointment_items ();
    void get_appointment_items ();
    void sort_appointments(void);

    void check_appt_time (QString, bool *);
    void save_appt_cell (QTableWidgetItem *);

    void read_appt_file (int inivecflag);
    void write_appt_file (int inivecflag);

    void read_daily_appt_file ();
    void write_daily_appt_file (int inivecflag);

    //void cl_read_appt_file (void);
    //void cl_write_appt_file (void);

    //void cl_read_daily_appt_file (void);
    //void cl_write_daily_appt_file (void);

    void issue_appt_alarm();
    void set_next_appointment_timer ();
    void set_appt_time_array();

    //anniversaries table
    void fill_anniversary_items ();
    void get_anniversary_items ();
    void sort_anniversaries (void);

    void save_ann_cell (QTableWidgetItem *);

    void read_ann_file ();
    void write_ann_file (int inivecflag);

    //void cl_read_ann_file (void);
    //void cl_write_ann_file (void);

    //contacts
    void show_contact();
    void save_contact ();
    void modify_name (QTreeWidgetItem *);

    void set_contact (QTreeWidgetItem *);

    void read_contacts ();
    void write_contacts (int inivecflag);

    //void cl_write_contacts ();
    //void cl_read_contacts ();

    void add_category ();
    void add_contact ();
    void del_item ();

    //lists
    void show_list ();
    void save_list();

    void add_list();
    void del_list();

    void set_list(QTreeWidgetItem *);

    void read_lists ();
    void write_lists (int inivecflag);

    //void cl_read_lists ();
    //void cl_write_lists ();

    //preferences
    void create_prefs_weekgrp_box ();
    void weekstartsun (bool);
    void weekstartmon (bool);
    void set_cal_grid(int);
    void set_cal_week_num(int);

    void create_prefs_tabgrp_box ();
    void tab_start(bool);

    void fortunestate (int);
    void select_font();

    //search
    void search_data ();

    void search_list_notes();
    void search_notes_file (QFileInfo, QStringList *);

    void search_list_appts();
    void search_appts_file (QFileInfo, QStringList *);

    //preferences
    void writeprefs();
    void readprefs();

    //password
    void re_encrypt_notes_appt_data ();
    void check_password();
    void cncl_pwd_change();
    void make_password_test_file (QString phrase);
    void test_password ();

    void confirm_phrase();
    void reject_phrase();

    //other functions
    void save_notes_and_appts();
    void save_other_data();
    void make_tab_visible(int);
    void load_day ();
    void load_month ();
    void initialize ();
    void closeEvent(QCloseEvent *event);

};

#endif // MAINWINDOW_H
