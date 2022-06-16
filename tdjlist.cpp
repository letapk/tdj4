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

//Last modified  17 October 2014

#include "tdj.h"
#include <QTextCodec>

void MainWindow::save_list ()
//item changed
{
QString s;

    s = listeditor->toHtml();
    cur_list->setText(1, s);
    modify_name(cur_list);
}

void MainWindow::set_list (QTreeWidgetItem *it)
//item clicked
{
QString s;

    save_list  ();

    cur_list = it;
    show_list();
}

void MainWindow::add_list()
{
QString s;

    if (listreeempty == false) {
        save_list();
    }

    con_item = new QTreeWidgetItem (listree);

    s.clear();
    s.append(tr("New list"));
    con_item->setText(0, s);
    s.clear();
    s.append(tr("New list"));
    con_item->setText(1, s);

    cur_list = con_item;

    listree->setCurrentItem(cur_list);
    listreeempty = false;

    statustext->setText(tr("Added an empty list"));
    show_list();
}

void MainWindow::show_list ()
{
QString s;
int i;

    //bring lists tab to foreground
    i = tabcontainer->indexOf(listed);
    tabcontainer->setCurrentIndex(i);

    //first way
    s.clear();
    s.append(cur_list->text(1));
    listeditor->setHtml(s);

    //second way
    //QTextDocument *doc = new QTextDocument ();
    //doc->setHtml(cur_list->text(1));
    //listeditor->setDocument(doc);

    //third way
    //QByteArray data = (cur_list->text(1)).toUtf8();
    //QTextCodec *codec = Qt::codecForHtml(data);
    //s = codec->toUnicode(data);
    //listeditor->setHtml(s);

}

void MainWindow::del_list ()
{
int j;
QTreeWidgetItem *above, *below;
QTextDocument *doc;
QString s;

    doc = new QTextDocument ();
    doc->setHtml(cur_list->text(1));
    s = doc->toPlainText();

    if (s.length() != 0){
        statustext->setText(tr("List contains data. Please delete that, first."));
        return;
    }

    j = listree->indexOfTopLevelItem(cur_list);

    above = listree->itemAbove(cur_list);
    below = listree->itemBelow(cur_list);

    if (above != NULL){//there is an item above
        listree->takeTopLevelItem(j);
        statustext->setText(tr("List deleted"));

        listree->setCurrentItem(above);
        cur_list = above;

        listreeempty = false;
        show_list();
    }
    else if (below != NULL) {//no item above but there is an item below
        listree->takeTopLevelItem(j);
        statustext->setText(tr("List deleted"));

        listree->setCurrentItem(cur_list);
        cur_list = below;

        listreeempty = false;
        show_list();
    }
    else {//cur_list is the last item
        listree->takeTopLevelItem(j);
        listreeempty = true;
        statustext->setText(tr("Last list deleted"));
    }
}
