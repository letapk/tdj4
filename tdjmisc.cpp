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

//Last modified 19 June 2022

#include "tdj.h"
#include <QFileDialog>
#include <QImageReader>
#include <QDesktopServices>

extern size_t txtLength; // string plus termination
extern size_t txtLengthpadded; // string plus padded zero characters
extern char *toencrypt, *encrypted, *todecrypt, *decrypted;
extern char iniVector[];

extern int aesdec (void);
extern void set_decrypt_variables(void);
extern void free_dec_strings (void);

extern int aesenc (void);
extern int close_gcrypt (void);
extern void crypt_error_notification (const char *errstr);

extern QString Lockfilename, userpath;
extern bool setpwd;

extern Note note[];

//used while reencrypting data files due to a change in password
QFileInfo reencfileInfo;

extern size_t szt;

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

void create_lockfile ()
{
    QFile file(Lockfilename);
    file.open(QFile::WriteOnly);
    file.close();
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
    close_gcrypt();

    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
//move and shift the widgets when the window size changes
{
int w, h;

    w = width();
    h = height();

    tb->resize(w - 320, 30);

    //resize the trees
    contree->resize(contree->width(), h - 340);
    listree->resize(contree->width(), h - 340);

    //resize the tabcontainer
    tabcontainer->resize(w - 320, h - 100);
    noteditor->setGeometry(10, 10, tabcontainer->width()- 20, tabcontainer->height()- 55);
    contacteditor->setGeometry(10, 10, tabcontainer->width()- 20, tabcontainer->height()- 55);
    listeditor->setGeometry(10, 10, tabcontainer->width()- 20, tabcontainer->height()- 55);
    apptable->setGeometry(10, 10, tabcontainer->width()- 20, tabcontainer->height()- 55);
    anntable->setGeometry(10, 10, tabcontainer->width()- 20, tabcontainer->height()- 55);

    //resize and reposition the search widgets
    searchtxtbox->setGeometry(120, 10, tabcontainer->width()-220, 25);
    srchbut->setGeometry(tabcontainer->width()-90, 10, 80, 25);

    //move the status text area
    statustext->setGeometry(10, h - 30, w, 25);

    srchresults->setGeometry(10, 45, tabcontainer->width() - 20, tabcontainer->height() - 65 - 25);

    //this makes the font size box visible on expanding the window and fixes
    //a bug which would prevent it showing if the user has clicked the extension
    //button on the toolbar
    comboFont->setVisible(true);
    comboSize->setVisible(true);

    //pass the event up the chain
    QWidget::resizeEvent(event);
}

void MainWindow::make_password_test_file (QString phrase)
{
QString Testfilename, s;
int i, k;
FILE *testf;

    i = phrase.toUtf8().size();
    toencrypt = (char *) malloc (i + 1);
    strcpy(toencrypt, "");
    strcpy (toencrypt, phrase.toUtf8().data());

    Testfilename.append (Homepath);
    s = QString ("/Checkpwd.tdj");
    Testfilename.append (s);

    testf = fopen (Testfilename.toUtf8().data(), "w");
    fwrite (&iniVector, sizeof (char), 16, testf);
    fwrite (&i, sizeof (int), 1, testf);

    k = aesenc ();
    if (k == 1) {
        crypt_error_notification ("Error in encryption of test data.");
    }

    //save the encrypted data
    fwrite (encrypted, txtLengthpadded, 1, testf);

    free ((char *)encrypted);
    free ((char *)toencrypt);

    fflush(testf);
    fclose (testf);
}

void MainWindow::test_password ()
{
QString Testfilename, s, serr;
QString Decstr;
QMessageBox msgBox;
int k, testln;
FILE *testf;

    Testfilename.append (Homepath);
    s = QString ("/Checkpwd.tdj");
    Testfilename.append (s);

    testf = fopen (Testfilename.toUtf8().data(), "r");
    szt = fread (&iniVector, sizeof (char), 16, testf);
    szt = fread (&testln, sizeof (int), 1, testf);

    txtLength = testln;
    set_decrypt_variables();
    //read the data to be decrypted
    szt = fread (todecrypt, txtLengthpadded, 1, testf);
    //decrypt the data and put it in decrypted
    k = aesdec ();
    if (k == 1) {
        crypt_error_notification ("Error in decryption of test data.");
    }

    Decstr.clear();
    Decstr.append(decrypted);

    //display the string
    Cnfdialog = new QDialog (this);

    lbl4 = new QLabel(tr("Confirm encoded phrase"), Cnfdialog);
    ledt4 = new QLineEdit (Cnfdialog);
    ledt4->setText(Decstr);
    lbl4->setBuddy(ledt4);
    ok2but = new QPushButton(tr("&Confirm"));
    cncl2but = new QPushButton(tr("&Reject"));

    connect (ok2but, SIGNAL(clicked()), this, SLOT(confirm_phrase()));
    connect (cncl2but, SIGNAL(clicked()), this, SLOT(reject_phrase()));

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

    //free the buffers
    free_dec_strings ();

}

void MainWindow::re_encrypt_notes_appt_data ()
{
int i;
QDir dir;
QString path;
QStringList fltr;
QFileInfoList list;
int inivecflag = 1;

    dir.setFilter(QDir::Files);
    dir.setPath(Homepath);

    //create list of Notes files
    fltr << "Notes*.tdj";
    dir.setNameFilters(fltr);
    list = dir.entryInfoList();//list of Notes files

    //read and re-encrypt each file
    for (i = 0; i < list.size(); i++) {
        reencfileInfo = list.at(i);//file at position i in the list
        read_journal_file (inivecflag);
        write_journal_file (inivecflag);
    }

    fltr.clear();
    list.clear();

    //create list of Appt files
    fltr << "Appointments*.tdj";
    dir.setNameFilters(fltr);
    list = dir.entryInfoList();

    //read and re-encrypt each file
    for (i = 0; i < list.size(); i++) {
        reencfileInfo = list.at(i);
        read_appt_file (inivecflag);
        write_appt_file (inivecflag);
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
    txtfile.append ("/Notes.txt");
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
        read_journal_file (1);

        fname = reencfileInfo.baseName();//filename without path and extension : "Notes-xxxx-xx"
        Month = fname.remove (0, 11);//remove leading part : "Notes-xxxx-". Only "xx" remains

        fname = reencfileInfo.baseName();//filename without path and extension : "Notes-xxxx-xx"
        Year = fname.remove(0, 6);//remove leading part of Notefilename : "Notes-". Only "xxxx-xx" remains
        Year.truncate(4);//remove trailing part : "-xx". Only "xxxx" remains.

        for (j = 1; j <= 31; j++) {
            if (note[j].length > 0) {
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
    s = QString (tr("Notes data saved to %1")).arg(txtfile);
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
    txtfile.append ("/Lists.txt");
    QFile file(txtfile);
    ok = file.open(QFile::WriteOnly);
    if (ok == false)
        return;
    QTextStream out(&file);

    //number of categories
    toplevelcount = listree->topLevelItemCount();
    out << "Number of lists:" << toplevelcount << "\n";

    //loop over lists
    for (i = 0; i < toplevelcount; i++){
        it = listree->topLevelItem(i);

        doc = new QTextDocument ();
        doc->setHtml(it->text(1));
        s = doc->toPlainText();
        delete doc;

        out << "\nName of list:";
        out << s;
        out << "\n";
    }

    file.close();
    s = QString (tr("Lists data saved to %1")).arg(txtfile);
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
    actionTextBold->setShortcut(Qt::CTRL + Qt::Key_B);
    actionTextBold->setPriority(QAction::LowPriority);
    QFont bold;
    bold.setBold(true);
    actionTextBold->setFont(bold);
    connect(actionTextBold, SIGNAL(triggered()), this, SLOT(textBold()));
    tb->addAction(actionTextBold);
    actionTextBold->setCheckable(true);

    actionTextItalic = new QAction(QIcon::fromTheme("", QIcon(":/images/textitalic.png")), tr("&Italic"), this);
    actionTextItalic->setPriority(QAction::LowPriority);
    actionTextItalic->setShortcut(Qt::CTRL + Qt::Key_I);
    QFont italic;
    italic.setItalic(true);
    actionTextItalic->setFont(italic);
    connect(actionTextItalic, SIGNAL(triggered()), this, SLOT(textItalic()));
    tb->addAction(actionTextItalic);
    actionTextItalic->setCheckable(true);

    actionTextUnderline = new QAction(QIcon::fromTheme("", QIcon(":/images/textunder.png")), tr("&Underline"), this);
    actionTextUnderline->setShortcut(Qt::CTRL + Qt::Key_U);
    actionTextUnderline->setPriority(QAction::LowPriority);
    QFont underline;
    underline.setUnderline(true);
    actionTextUnderline->setFont(underline);
    connect(actionTextUnderline, SIGNAL(triggered()), this, SLOT(textUnderline()));
    tb->addAction(actionTextUnderline);
    actionTextUnderline->setCheckable(true);

    QActionGroup *grp = new QActionGroup(this);
    connect(grp, SIGNAL(triggered(QAction*)), this, SLOT(textAlign(QAction*)));

    actionAlignLeft = new QAction(QIcon::fromTheme("", QIcon(":/images/textleft.png")),tr("&Left"), grp);
    actionAlignCenter = new QAction(QIcon::fromTheme("",QIcon(":/images/textcenter.png")),tr("C&enter"), grp);
    actionAlignRight = new QAction(QIcon::fromTheme("",QIcon(":/images/textright.png")),tr("&Right"), grp);
    actionAlignJustify = new QAction(QIcon::fromTheme("",QIcon(":/images/textjustify.png")),tr("&Justify"), grp);

    actionAlignLeft->setShortcut(Qt::CTRL + Qt::Key_L);
    actionAlignLeft->setCheckable(true);
    actionAlignLeft->setPriority(QAction::LowPriority);
    actionAlignCenter->setShortcut(Qt::CTRL + Qt::Key_E);
    actionAlignCenter->setCheckable(true);
    actionAlignCenter->setPriority(QAction::LowPriority);
    actionAlignRight->setShortcut(Qt::CTRL + Qt::Key_R);
    actionAlignRight->setCheckable(true);
    actionAlignRight->setPriority(QAction::LowPriority);
    actionAlignJustify->setShortcut(Qt::CTRL + Qt::Key_J);
    actionAlignJustify->setCheckable(true);
    actionAlignJustify->setPriority(QAction::LowPriority);

    tb->addActions(grp->actions());

    QAction *actionInsertImage= new QAction(QIcon::fromTheme("", QIcon(":/images/insert-image.png")), tr("&Insert image"), this);
    actionInsertImage->setPriority(QAction::LowPriority);
    connect(actionInsertImage, SIGNAL(triggered()), this, SLOT(insertImage()));
    actionInsertImage->setCheckable(true);

    tb->addAction(actionInsertImage);

    QPixmap pix(16, 16);
    pix.fill(Qt::black);
    actionTextColor = new QAction(pix, tr("&Color..."), this);
    connect(actionTextColor, SIGNAL(triggered()), this, SLOT(textColor()));
    tb->addAction(actionTextColor);

    comboFont = new QFontComboBox(tb);
    tb->addWidget(comboFont);
    connect(comboFont, SIGNAL(activated(QString)), this, SLOT(textFamily(QString)));

    comboSize = new QComboBox(tb);
    comboSize->setObjectName("comboSize");
    tb->addWidget(comboSize);
    comboSize->setEditable(true);

    QFontDatabase db;
    foreach(int size, db.standardSizes())
    comboSize->addItem(QString::number(size));

    connect(comboSize, SIGNAL(activated(QString)), this, SLOT(textSize(QString)));
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
    fmt.setFontFamily(f);
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
