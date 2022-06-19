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

void MainWindow::save_contact ()
//item changed
{
QString s;

    s = contacteditor->toHtml();
    if (catflag == 0){//contact data item
        cur_con->setText(1, s);
        modify_name (cur_con);
    }
    else {//category data item
        cur_cat->setText(1, s);
        modify_name(cur_cat);
    }
}

void MainWindow::modify_name (QTreeWidgetItem *it)
{
QString s1, s;
int i;
QTextDocument doc;

    s1 = it->text(1);
    doc.setHtml(s1);
    s = doc.toPlainText();

    for (i = 0; i < s.length(); i++){
        if (s[i] == QChar (QLatin1Char('\n')))
            s.chop (s.length() - i);
    }

    it->setText(0, s);
}

void MainWindow::show_contact ()
{
QString s;
int i;

    //bring contacts tab to foreground
    i = tabcontainer->indexOf(contacted);
    tabcontainer->setCurrentIndex(i);

    s.clear();

    if (catflag == 0){//contact selected
        s.append(cur_con->text(1));
        modify_name (cur_con);
    }
    else {//category selected
        s.append(cur_cat->text(1));
        modify_name(cur_cat);
    }

    contacteditor->setHtml(s);
}

void MainWindow::set_contact (QTreeWidgetItem *it)
//item clicked
{
QString s;
int i;

    //bring contacts tab to foreground
    i = tabcontainer->indexOf(contacted);
    tabcontainer->setCurrentIndex(i);

    save_contact  ();

    if (it->parent() != NULL) {//contact item
        cur_cat = it->parent();
        cur_con = it;
        catflag = 0;
    }
    else {//category item
        cur_cat = it;
        cur_con = it;
        catflag = 1;
    }

    show_contact();

}

void MainWindow::add_category()
{
QString s;

    if (contreeempty == false) {
        save_contact  ();
    }

    con_item = new QTreeWidgetItem (contree);

    s.clear();
    s.append(tr("New Group"));
    con_item->setText(0, s);
    s.clear();
    s.append(tr("New Group<p></p>Enter group description here. The first line becomes the group name in the tree on the left."));
    con_item->setText(1, s);

    cur_cat = con_item;
    catflag = 1;

    contree->setCurrentItem(cur_cat);

    contreeempty = false;

    statustext->setText(tr("Added an empty group"));
    show_contact();
}

void MainWindow::add_contact()
{
QString s;

    if (contreeempty == true) {
        statustext->setText(tr("Add a group first"));
        return;
    }

    save_contact  ();

    con_item = new QTreeWidgetItem ();

    s.clear();
    s.append(tr("New Contact"));
    con_item->setText(0, s);
    s.clear();
    s.append(tr("New Contact<p></p>Enter contact data here. The first line becomes the contact name in the tree on the left."));
    con_item->setText(1, s);

    cur_con = con_item;
    cur_cat->addChild(cur_con);
    catflag = 0;

    contree->setCurrentItem(cur_con);

    contreeempty = false;

    statustext->setText(tr("Added an empty contact"));
    show_contact();
}

void MainWindow::del_item()
{
int i, j;
QTreeWidgetItem *above, *below;

    if (contreeempty == true) {
        statustext->setText(tr("Nothing to delete!"));
        return;
    }

    if (catflag == 1) {//category selected
        i = cur_cat->childCount();

        if (i > 0) {//category contains contacts
            statustext->setText(tr("The group is not empty. Please delete the contacts."));
            return;
        }
        else {//empty category
            j = contree->indexOfTopLevelItem(cur_cat);
            above = contree->itemAbove(cur_cat);
            below = contree->itemBelow(cur_cat);

            if (above != NULL){//there is an item above
                contree->takeTopLevelItem(j);
                if (above->parent() == NULL) {//the item above is a category
                    cur_cat = above;
                    cur_con = above;
                    catflag = 1;
                }
                else {//the item above is a contact
                    cur_cat = above->parent();
                    cur_con = above;
                    catflag = 0;
                }
                contree->setCurrentItem(above);

                statustext->setText(tr("Group deleted"));
                contreeempty = false;
                show_contact();
            }
            else if (below != NULL) {//no item above but there is an item below
                contree->takeTopLevelItem(j);

                cur_cat = below;//which has to be a category
                cur_con = below;
                contree->setCurrentItem(cur_cat);
                catflag = 1;

                statustext->setText(tr("Group deleted"));
                contreeempty = false;
                show_contact();
            }
            else {//cur_cat is the last item
                contree->takeTopLevelItem(j);

                contreeempty = true;
                statustext->setText(tr("Last group deleted"));
            }
        }//empty category
    }//catflag = 1
    else {//contact selected
        cur_con = contree->currentItem();
        i = cur_cat->indexOfChild(cur_con);
        cur_cat->takeChild(i);

        contree->setCurrentItem(cur_cat);
        catflag = 1;

        statustext->setText(tr("Deleted contact"));
        contreeempty = false;
        show_contact();
    }
}
