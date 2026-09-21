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

extern int get_month_int (QString s);


int anniversary_compare (const void *a, const void *b);

void MainWindow::fill_anniversary_items ()
{
int i, m;

    //fill all 366 table rows. The old code looped over the days of the year,
    //so in non-leap years the 29 February (row 366) was never displayed.
    for (i = 0; i < 366; i++){
        //new data stores a canonical month number; show its localized name.
        //Legacy files store a localized label which is shown as-is.
        m = get_month_int (m_store.ann(i+1).month);
        if (m >= 1 && m <= 12)
            anncol0[i].setText (get_month_name (m));
        else
            anncol0[i].setText (m_store.ann(i+1).month);
        anncol1[i].setText (m_store.ann(i+1).date);
        anncol2[i].setText (m_store.ann(i+1).description);
    }
}

void MainWindow::sort_anniversaries (void)
{
    if (m_store.maxAnns() > 0)
        qsort (&m_store.ann(1), m_store.maxAnns(), sizeof (Anniversary), anniversary_compare);
}

int anniversary_compare (const void *a, const void *b)
{
int idate, imonth;
int jdate, jmonth;
Anniversary *c, *d;

    c = (Anniversary *) a;
    d = (Anniversary *) b;

    //find out the integer month in which the holiday falls
    imonth = get_month_int (c->month);
    if (imonth == 0)
        return 0;
    jmonth = get_month_int (d->month);

    if (imonth <  jmonth)
        return -1;
    if (imonth >  jmonth)
        return 1;

    //if the months are the same, check the date
    if (imonth == jmonth) {
        idate = c->date.toInt();
        jdate = d->date.toInt();

        if (idate < jdate)
             return -1;
        if (idate == jdate)
             return 0;
        if (idate > jdate)
            return 1;
    }
    return 0;
}

void MainWindow::get_anniversary_items ()
{
int i, j, row, m;

    for (i = 1; i < 367; i++) {
        m_store.ann(i).date.clear();
        m_store.ann(i).month.clear();
        m_store.ann(i).description.clear();
    }

    //table rows run from 0 onwards
    //but are numbered from 1 onwards on the screen
    //scan all 366 rows (a leap-day entry in the last row was previously lost
    //in non-leap years)
    j = 1;
    for (row = 0; row < 366; row++) {
        i = anncol2[row].text().length();

        if (i != 0) {//something to save
            //store a canonical month number so sorting and colouring never
            //depend on the display locale; a legacy label that cannot be
            //parsed is kept verbatim
            m = get_month_int (anncol0[row].text());
            if (m >= 1 && m <= 12)
                m_store.ann(j).month = QString::number (m);
            else
                m_store.ann(j).month = anncol0[row].text();
            m_store.ann(j).date = anncol1[row].text();
            m_store.ann(j).description = anncol2[row].text();
            j++;
            i = 0;
        }
    }
}

void MainWindow::save_ann_cell (QTableWidgetItem *it)
{
int row, col;
QString s;

    row = anntable->currentRow();
    col = anntable->currentColumn();

    if (col == -1 || row == -1)
        return;

    s.clear();
    s = it->text();


    if (s.size() != 0) {//something in the cell
        if (col == 0) {
            anncol0[row].setText(s);
            s.clear();
        }
        if (col == 1){
            anncol1[row].setText(s);
            s.clear();
        }
        if (col == 2) {
            anncol2[row].setText(it->text());
            s.clear();
        }
    }
}

ComboBoxItemDelegate::ComboBoxItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

ComboBoxItemDelegate::~ComboBoxItemDelegate()
{
}

QWidget* ComboBoxItemDelegate::createEditor( QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
QDate d;

    // Create the combobox
    QComboBox *cb = new QComboBox(parent);

    if(index.column() == 0) {
        //populate it
        cb->addItem(QString (tr("")));
        cb->addItem(QString(tr("January")));
        cb->addItem(QString(tr("February")));
        cb->addItem(QString(tr("March")));
        cb->addItem(QString(tr("April")));
        cb->addItem(QString(tr("May")));
        cb->addItem(QString(tr("June")));
        cb->addItem(QString(tr("July")));
        cb->addItem(QString(tr("August")));
        cb->addItem(QString(tr("September")));
        cb->addItem(QString(tr("October")));
        cb->addItem(QString(tr("November")));
        cb->addItem(QString(tr("December")));
    }

    if(index.column() == 1) {
        cb->addItem(QString(tr("")));

        cb->addItem(QString(tr("1")));
        cb->addItem(QString(tr("2")));
        cb->addItem(QString(tr("3")));
        cb->addItem(QString(tr("4")));
        cb->addItem(QString(tr("5")));
        cb->addItem(QString(tr("6")));
        cb->addItem(QString(tr("7")));

        cb->addItem(QString(tr("8")));
        cb->addItem(QString(tr("9")));
        cb->addItem(QString(tr("10")));
        cb->addItem(QString(tr("11")));
        cb->addItem(QString(tr("12")));
        cb->addItem(QString(tr("13")));
        cb->addItem(QString(tr("14")));

        cb->addItem(QString(tr("15")));
        cb->addItem(QString(tr("16")));
        cb->addItem(QString(tr("17")));
        cb->addItem(QString(tr("18")));
        cb->addItem(QString(tr("19")));
        cb->addItem(QString(tr("20")));
        cb->addItem(QString(tr("21")));

        cb->addItem(QString(tr("22")));
        cb->addItem(QString(tr("23")));
        cb->addItem(QString(tr("24")));
        cb->addItem(QString(tr("25")));
        cb->addItem(QString(tr("26")));
        cb->addItem(QString(tr("27")));
        cb->addItem(QString(tr("28")));

        cb->addItem(QString(tr("29")));
        cb->addItem(QString(tr("30")));
        cb->addItem(QString(tr("31")));
    }

    if(index.column() == 2)
        return QStyledItemDelegate::createEditor(parent, option, index);

    return cb;
}

void ComboBoxItemDelegate::setEditorData ( QWidget *editor, const QModelIndex &index ) const
{
    if(QComboBox *cb = qobject_cast<QComboBox *>(editor)) {
        // get the index of the text in the combobox that matches the current value of the item
        QString currentText = index.data(Qt::EditRole).toString();
        int cbIndex = cb->findText(currentText);
        // if it is valid, adjust the combobox
        if(cbIndex >= 0)
            cb->setCurrentIndex(cbIndex);
    } else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void ComboBoxItemDelegate::setModelData ( QWidget *editor, QAbstractItemModel *model, const QModelIndex &index ) const
{
    if(QComboBox *cb = qobject_cast<QComboBox *>(editor))
        // save the current text of the combo box as the current value of the item
        model->setData(index, cb->currentText(), Qt::EditRole);
    else
        QStyledItemDelegate::setModelData(editor, model, index);
}
