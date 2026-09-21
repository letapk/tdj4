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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <stdio.h>
#include <string.h>

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
#include <QFrame>
#include <QMouseEvent>
#include <QDir>
#include <QStringList>
#include <QTimer>
#include <QColorDialog>
#include <QColor>
#include <QTextCursor>
#include <QFile>
#include <QToolBar>
#include <QTextCharFormat>
#include <QFontComboBox>
#include <QTranslator>
#include <QButtonGroup>
#include <QActionGroup>
#include <QPixmap>
#include <QUrl>
#include <QTextDocument>

//storage and crypto layer: field codec, TDJ2 container, key/salt ownership
//(CryptoManager) and the store file facade (TdjEncryptedFile). Qt-Core-only.
#include "tdjstore.h"

//editor that renders encrypted attachments: <img src="tdj-image:<id>"> is
//resolved through StorageManager::getAttachment() (the raw bytes live only in
//the encrypted Attachments.tdj store), so nothing is ever read from a loose
//plaintext file in the data dir. Any non-attachment URL falls through to the
//base QTextEdit behaviour.
class TdjEditor : public QTextEdit
{
    Q_OBJECT

public:
    explicit TdjEditor(StorageManager *store, QWidget *parent = 0);

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    StorageManager *m_store;
};

namespace Ui {
class MainWindow;
}

//one row of a two column table, used for sorting the appointments
class Tablerow {
public:
    QString col0, col1;
    bool chkstate;
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
    TdjEditor *noteditor, *contacteditor, *listeditor;
    QTableWidget *apptable, *anntable;

    //draggable handle between the left panel (calendar/trees) and the right
    //panel (toolbar/tabs); leftwidth is the current width of the left panel
    QFrame *divider;
    int leftwidth;
    bool divider_dragging;
    int divider_drag_x;
    int divider_drag_left;

    //horizontal handle below the Today button that resizes the calendar; the
    //height stays -1 ("auto-fit the current font") until the user drags it,
    //and a font change resets it to auto
    QFrame *hdiv;
    int calheight;
    bool calheight_dragging;
    int calheight_drag_y;
    int calheight_drag_base;

    //text in editor
    QString note_to_show, list_to_show;

    //items for appointments table
    QTableWidgetItem appcol0[48], appcol1[48];
    //this stores all the 48 appt times for today, some may be zero.
    int appt_time_array[48];

    //items for anniversaries table
    QTableWidgetItem anncol0[367], anncol1[367], anncol2[367];

    //search
    QGroupBox *searchbox;
    QLabel *searchboxlabel;
    QLineEdit *searchtxtbox;
    QPushButton *srchbut;
    QTextEdit *srchresults;
    QString srchtxt;
    QTextCursor srchcursor;

    //preferences
    QGroupBox *tabbox;
    QGroupBox *weekbox;
    QRadioButton *sun, *mon;

    QCheckBox *gridbox, *weeknumbox, *fortunebox;
    bool grid, weeknum, fortune;

    QRadioButton *t1, *t2, *t3, *t4;

    uint weekstrt, tabstart;

    QPushButton *fontbut;
    QPushButton *headercolbut;
    QPushButton *datadirbut;
    QFont curfont;

    QLabel *statustext;
    QLabel *datelabel;//persistent display of the selected date (bottom right)

    //password
    QString password;
    QString old_password, new_password;

    //owns every key, salt and IV used by the store layer (no file globals)
    CryptoManager m_crypto;
    //owns the in-memory data models and the store serialization (no file
    //globals, no extern chains); the GUI binds widgets through the accessors
    StorageManager m_store;
    //T2: set whenever an image is imported or a note is emptied; the next
    //save_notes_and_appts() then writes Attachments.tdj and prunes orphans
    bool m_attachmentsDirty = false;

    QDialog *Chpwdialog, *Cnfdialog;
    QLabel *lbl1, *lbl2, *lbl4;
    QLineEdit *ledt1, *ledt2, *ledt4;
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
    QString Attachmentsfilename;
    QString Gnugplfilename;
    QString Helpfilename;

    //dialog to change the default data directory
    QDialog *Datadirdialog;
    //data subdirectory for this session
    QString Datadirectory;

    void resizeEvent(QResizeEvent *);
    void showEvent(QShowEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
    //(re)position the left and right panels from leftwidth and the window size
    void layout_panels();
    //push a font onto the calendar and its internal navigation widgets: with
    //the compact stylesheet applied they ignore application font changes
    void apply_font_to_calendar(const QFont &);

    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    friend class ComboBoxItemDelegate;

    //the store the GUI is bound to; exposed for the headless tests that drive
    //the attachment/render paths directly
    StorageManager &store() { return m_store; }

    //test-only: the calendar background colour of a day in the shown month,
    //so a headless test can verify editing/display preserves the colour
    //precedence rather than resetting the date to white
    QColor calendarDateBg(int day) const {
        return calendar->dateTextFormat(QDate(year, month, day)).background().color();
    }

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
    void recolor_calendar_date(int);
    void format_headers();
    void set_header_color();
    void go_to_today();

    //noteditor
    void shownote ();
    void save_note ();

    void read_journal_file (int inivecflag, const QString &pathOverride = QString());
    void write_journal_file (int inivecflag);

    //appointments table
    void fill_appointment_items ();
    void get_appointment_items ();
    void sort_appointments(void);
    void compact_appointments(int j);

    void check_appt_time (QString, bool *);
    void save_appt_cell (QTableWidgetItem *);

    void read_appt_file (int inivecflag);
    void write_appt_file (int inivecflag);

    void read_daily_appt_file ();
    void write_daily_appt_file (int inivecflag);

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

    //contacts
    void show_contact();
    void save_contact ();
    void modify_name (QTreeWidgetItem *);

    //T2 encrypted attachments
    //rewrite every <img src="/abs/path"> (and file://) in `html` to
    //<img src="tdj-image:<id>">, importing the file content into the
    //attachment store; missing/oversized/non-importable files are left as-is
    QString import_legacy_images(const QString &html);
    //scan all stored content, drop unreferenced attachments and persist any
    //resulting shrink to disk
    void prune_orphan_attachments();

    void set_contact (QTreeWidgetItem *);

    void read_contacts ();
    void write_contacts (int inivecflag);

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

    //preferences
    void create_prefs_weekgrp_box ();
    void weekstartsun (bool);
    void weekstartmon (bool);
    void set_cal_grid(Qt::CheckState);
    void set_cal_week_num(Qt::CheckState);

    void create_prefs_tabgrp_box ();
    void tab_start(bool);

    void fortunestate (Qt::CheckState);
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
    void change_directory();

    //password
    void check_password();
    void cncl_pwd_change();
    void test_password ();
    void reencrypt_all_stores ();
    void reload_current_month ();

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

// ---------------------------------------------------------------------------
// Storage and crypto layer (Phase 3, v4-0.4).
//
// The encrypted-field codec, the TDJ2 container, the key/salt/IV ownership
// (CryptoManager) and the store file facade (TdjEncryptedFile) live in
// tdjstore.h / tdjstore.cpp (Qt-Core-only, no widget dependencies). Every
// secret is owned by CryptoManager m_crypto; there are no file-scope key,
// salt, IV or cipher-handle globals and no extern chains any more. The GUI
// talks to the store exclusively through TdjEncryptedFile and CryptoManager.
// ---------------------------------------------------------------------------

#endif // MAINWINDOW_H
