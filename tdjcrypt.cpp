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

//Last modified 21 October 2014

#include "tdj.h"
#include <gcrypt.h>
#include <unistd.h>

#define GCRY_CIPHER GCRY_CIPHER_AES128   // Pick the cipher here

extern Note note[];
extern Appointment appointment[], dailyappt;
extern Anniversary anniversary [];
//used while reencrypting data files due to a change in password
extern QFileInfo reencfileInfo;

QString fname;

int gcry_mode;
gcry_error_t gcryError;
gcry_cipher_hd_t gcryCipherHd;
size_t szt;
size_t index1, keyLength, blkLength;

size_t txtLength; // string plus termination
size_t txtLengthpadded; // string plus padded zero characters
int padding, tdremainder;

char *toencrypt, *encrypted, *todecrypt, *decrypted;
FILE *encf;

char OldaesSymKey[16];//old password
char NewaesSymKey[16];//new password

char iniVector[17];//initialization vector
char newiniVector[17];//initialization vector

void set_decrypt_variables(void);
void crypt_error_notification (const char *errstr);
void free_dec_strings (void);

int aesenc (void);
int aesdec (void);

QMessageBox *msgBox;

int aesenc (void)
{
int i;

    //length excludes the end NULL
    txtLength = strlen (toencrypt);

    //tdremainder which exceeds multiple of blkLength
    tdremainder = txtLength % blkLength;
    //no of characters to pad
    padding = blkLength - tdremainder - 1;

    if (padding != 0) {
        //pad with required no. of 'x' characters at the end
        toencrypt = (char *) realloc (toencrypt, size_t (txtLength+padding));

        for (i = 0; i < padding; i++)
            toencrypt[txtLength+i] = 'x';

        toencrypt[txtLength+padding] = '\0';
    }

    txtLengthpadded = strlen(toencrypt)+1;

    //allocate memory for buffer to hold encrypted data
    encrypted = (char *) malloc (txtLengthpadded);
    strcpy (encrypted, "");

    gcryError = gcry_cipher_setkey(gcryCipherHd, NewaesSymKey, keyLength);
    if (gcryError) {
        //printf("gcry_cipher_setkey failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    gcryError = gcry_cipher_setiv(gcryCipherHd, newiniVector, blkLength);
    if (gcryError) {
        //printf("gcry_cipher_setiv failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    gcryError = gcry_cipher_encrypt(gcryCipherHd, encrypted, txtLengthpadded, toencrypt, txtLengthpadded);
    if (gcryError) {
        //printf("gcry_cipher_encrypt failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    return 0;
}

int aesdec (void)
{
    gcryError = gcry_cipher_setkey(gcryCipherHd, OldaesSymKey, keyLength);
    if (gcryError) {
        //printf("gcry_cipher_setkey failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    gcryError = gcry_cipher_setiv(gcryCipherHd, iniVector, blkLength);
    if (gcryError) {
        //printf("gcry_cipher_setiv failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    gcryError = gcry_cipher_decrypt(gcryCipherHd, decrypted, txtLengthpadded, todecrypt, txtLengthpadded);
    if (gcryError) {
        //printf("gcry_cipher_decrypt failed:  %s/%s\n",gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    decrypted[txtLength] = '\0';

    return 0;
}

int initialize_gcrypt (void)
{
QString s;

    gcry_mode = GCRY_CIPHER_MODE_CBC;

    if (!gcry_check_version (GCRYPT_VERSION)){
        //printf ("libgcrypt version mismatch\n");
        return 1;
    }

    gcryError = gcry_cipher_open(
        &gcryCipherHd, GCRY_CIPHER, gcry_mode, 0);
    if (gcryError) {
        //printf("gcry_cipher_open failed:  %s/%s\n", gcry_strsource(gcryError), gcry_strerror(gcryError));
        return 1;
    }

    keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER);
    blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER);

    return 0;
}

int close_gcrypt (void)
{
    gcry_cipher_close(gcryCipherHd);

    return 0;
}

void MainWindow::read_journal_file (int inivecflag)
{
int i, k;

    //clear all notes
    for (i = 1; i < 32; i++) {
        note[i].data.clear();
        note[i].length = 0;
    }

    if (inivecflag == 1) {//pwd change in progress, filename comes from file list
        fname.clear();
        fname.append(Homepath);
        fname.append("/");
        fname.append(reencfileInfo.fileName());
        encf = fopen (fname.toUtf8().data(), "r");
        //printf ("%s ", fname.toUtf8().data());
    }
    else {//normal data read in progress, filename is Notefilename
        encf = fopen (Notefilename.toUtf8().data(), "r");
    }

    if (encf == NULL)
        return;

    szt = fread (&iniVector, sizeof (char), 16, encf);
    for (i = 1; i <= 31; i++) {
        //read 31 values and put them into length of the corresponding notes
        szt = fread (&(note[i].length), sizeof (int), 1, encf);
    }

    for (i = 1; i <= 31; i++) {
        if (note[i].length > 0) {
            txtLength = note[i].length;
            set_decrypt_variables();
            //read the data to be decrypted
            szt = fread (todecrypt, txtLengthpadded, 1, encf);

            //decrypt the data and put it in decrypted
            k = aesdec ();
            if (k == 1) {
                crypt_error_notification ("Error in decryption of journal data.");
            }
            //put decrypted data into note for this day
            note[i].data.append(decrypted);

            //free the buffers
            free_dec_strings ();
        }
    }

    fclose (encf);
}

void MainWindow::write_journal_file (int inivecflag)
{
QByteArray text;
QTextDocument *doc;
QString s;
int i, j = 0;

    for (i = 1; i <= 31; i++) {
        s.clear();
        doc = new QTextDocument ();
        doc->setHtml(note[i].data);
        s = doc->toPlainText();
        //all this to find the actual length of the note
        j += s.length();
        delete doc;
    }

    if (inivecflag == 1) {//pwd change in progress, filename comes from file list
        fname.clear();
        fname.append(Homepath);
        fname.append("/");
        fname.append(reencfileInfo.fileName());
        encf = fopen (fname.toUtf8().data(), "w");
        //printf ("%s \n", fname.toUtf8().data());
        if (encf == NULL)
            return;
        fwrite (&newiniVector, sizeof (char), 16, encf);
    }
    else {//normal data write in progress, filename is Notefilename
        if (j == 0) {//nothing to save
            unlink (Notefilename.toUtf8().data());
            return;
        }
        encf = fopen (Notefilename.toUtf8().data(), "w");
        if (encf == NULL)
            return;
        fwrite (&iniVector, sizeof (char), 16, encf);
    }

    for (i = 1; i <= 31; i++) {
        fwrite (&(note[i].length), sizeof (int), 1, encf);
    }

    for (i = 1; i <= 31; i++) {
        if (note[i].length > 0) {

            text = note[i].data.toUtf8();
            //size excludes the end NULL
            toencrypt = (char *) malloc (text.size() + 1);
            strcpy(toencrypt, "");
            //toencrypt contains the data to be encrypted
            strcpy(toencrypt, text.data());

            j = aesenc ();
            if (j == 1) {
                crypt_error_notification ("Error in encryption of journal data.");
            }

            //save the encrypted data
            fwrite (encrypted, txtLengthpadded, 1, encf);

            free ((char *)encrypted);
            free ((char *)toencrypt);
        }
    }

    fflush(encf);
    fclose (encf);
}

void MainWindow::read_appt_file (int inivecflag)
{
int i, k, row;
QString *s;

    //clear all appointments
    for (i = 1; i <= 31; i++) {
         for(row = 0; row < 48; row++) {
             appointment[i].apptime[row+1].clear();
             appointment[i].apptdesc[row+1].clear();
         }
         appointment[i].total = 0;
    }

    if (inivecflag == 1) {//pwd change in progress, filename comes from file list
        fname.clear();
        fname.append(Homepath);
        fname.append("/");
        fname.append(reencfileInfo.fileName());
        encf = fopen (fname.toUtf8().data(), "r");
    }
    else {//normal data read in progress, filename is Apptfname
        encf = fopen (Appointmentsfilename.toUtf8().data(), "r");
    }

    if (encf == NULL)
        return;

    szt = fread (&iniVector, sizeof (char), 16, encf);

    for (i = 1; i <= 31; i++) {
         for(row = 0; row < 48; row++) {
             //read the size of the time
             szt = fread (&txtLength, sizeof (int), 1, encf);
             set_decrypt_variables();
             //read the encrypted time
             szt = fread (todecrypt, txtLengthpadded, 1, encf);

             k = aesdec ();
             if (k == 1) {
                 crypt_error_notification ("Error in decryption of appointments data.");
             }

             //assign the time to the appointment
             s = new QString (decrypted);
             appointment[i].apptime[row+1].append(s);
             delete s;

             //free the buffers
             free_dec_strings ();

             //read the size of the description
             szt = fread (&txtLength, sizeof (int), 1, encf);
             set_decrypt_variables();
             //read the encrypted description
             szt = fread (todecrypt, txtLengthpadded, 1, encf);

             k = aesdec ();
             if (k == 1) {
                 crypt_error_notification ("Error in decryption of appointments data.");
             }

             //assign the description to the appointment
             s = new QString (decrypted);
             appointment[i].apptdesc[row+1].append(s);
             delete s;

             //free the buffers
             free_dec_strings ();

             if (appointment[i].apptime[row+1].size() + appointment[i].apptdesc[row+1].size() > 0)
                 (appointment[i].total)++;
         }
    }

    fclose (encf);
}

void MainWindow::write_appt_file (int inivecflag)
{
QByteArray text;
int i, j = 0, row, len;

    for (i = 1; i <= 31; i++) {
        for (row = 0; row < 48; row++) {
            j += appointment[i].apptdesc[row+1].size();
        }
    }

    if (inivecflag == 1) {//pwd change in progress, filename comes from file list
        fname.clear();
        fname.append(Homepath);
        fname.append("/");
        fname.append(reencfileInfo.fileName());
        encf = fopen (fname.toUtf8().data(), "w");
        if (encf == NULL)
            return;
        fwrite (&newiniVector, sizeof (char), 16, encf);
    }
    else {//normal data write in progress, filename is Appointmentsfilename
        if (j == 0) {//nothing to save
            unlink (Appointmentsfilename.toUtf8().data());
            return;
        }
        encf = fopen (Appointmentsfilename.toUtf8().data(), "w");
        if (encf == NULL)
            return;
        fwrite (&iniVector, sizeof (char), 16, encf);
    }

    for (i = 1; i <= 31; i++) {
        for (row = 0; row < 48; row++) {
            //out << appointment[i].apptime[row+1] << "\n";
            text = appointment[i].apptime[row+1].toUtf8();

            //size excludes the end NULL
            toencrypt = (char *) malloc (text.size() + 1);
            strcpy(toencrypt, "");
            //toencrypt contains the data to be encrypted
            strcpy(toencrypt, text.data());

            j = aesenc ();
            if (j == 1) {
                crypt_error_notification ("Error in encryption of appointments data.");
            }

            //save the size of the data
            len = text.size();
            fwrite (&len, sizeof (int), 1, encf);
            //save the encrypted data
            fwrite (encrypted, txtLengthpadded, 1, encf);

            free ((char *)encrypted);
            free ((char *)toencrypt);

            //out << appointment[i].apptdesc[row+1] << "\n";
            text = appointment[i].apptdesc[row+1].toUtf8();

            //size excludes the end NULL
            toencrypt = (char *) malloc (text.size() + 1);
            strcpy(toencrypt, "");
            //toencrypt contains the data to be encrypted
            strcpy(toencrypt, text.data());

            j = aesenc ();
            if (j == 1) {
                crypt_error_notification ("Error in encryption of appointments data.");
            }

            //save the size of the data
            len = text.size();
            fwrite (&len, sizeof (int), 1, encf);
            //save the encrypted data
            fwrite (encrypted, txtLengthpadded, 1, encf);

            free ((char *)encrypted);
            free ((char *)toencrypt);
        }
    }

    fclose(encf);
}

void MainWindow::read_daily_appt_file ()
{
int k, row;
QString *s;

    //clear all appointments
    for(row = 0; row < 48; row++) {
        dailyappt.apptime[row+1].clear();
        dailyappt.apptdesc[row+1].clear();
    }
    dailyappt.total = 0;

    encf = fopen (DailyAppointmentsfilename.toUtf8().data(), "r");
    if (encf == NULL)
        return;

    szt = fread (&iniVector, sizeof (char), 16, encf);

    for(row = 0; row < 48; row++) {
        //read the size of the time
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted time
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of daily appointments data.");
        }

        //assign the time to the appointment
        s = new QString (decrypted);
        dailyappt.apptime[row+1].append(s);
        delete s;

        //free the buffers
        free_dec_strings ();

        //read the size of the description
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted description
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of daily appointments data.");
        }

        //assign the description to the appointment
        s = new QString (decrypted);
        dailyappt.apptdesc[row+1].append(s);
        delete s;

        //free the buffers
        free_dec_strings ();

        if (dailyappt.apptime[row+1].size() + dailyappt.apptdesc[row+1].size() > 0)
            (dailyappt.total)++;
    }
}

void MainWindow::write_daily_appt_file (int inivecflag)
{
QByteArray text;
int j = 0, len, row;

    for (row = 0; row < 48; row++) {
        j += dailyappt.apptdesc[row+1].size();
    }

    if (j == 0) {//nothing to save
        unlink (DailyAppointmentsfilename.toUtf8().data());
        return;
    }

    encf = fopen (DailyAppointmentsfilename.toUtf8().data(), "w");
    if (encf == NULL)
        return;

    if (inivecflag == 1) {//pwd change in progress
        fwrite (&newiniVector, sizeof (char), 16, encf);
    }
    else {//normal data write in progress
        fwrite (&iniVector, sizeof (char), 16, encf);
    }

    for (row = 0; row < 48; row++) {
        text = dailyappt.apptime[row+1].toUtf8();

        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        j = aesenc ();
        if (j == 1) {
            crypt_error_notification ("Error in encryption of daily appointments data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

        text = dailyappt.apptdesc[row+1].toUtf8();

        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        j = aesenc ();
        if (j == 1) {
            crypt_error_notification ("Error in encryption of appointments data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);
    }

    fclose(encf);
}

void MainWindow::read_ann_file ()
{
QString *s;
int i, k;

    max_anniversaries = 0;

    encf = fopen (Anniversaryfilename.toUtf8().data(), "r");

    if (encf == NULL)
        return;

    szt = fread (&iniVector, sizeof (char), 16, encf);

    for (i = 1; i < 367; i++) {
        anniversary[i].date.clear();
        anniversary[i].month.clear();
        anniversary[i].description.clear();
    }

    //read the holidays
    for (i = 1; i < 367; i++) {
        //read the size of the date
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted date
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of anniversary data.");
        }

        //assign the date to the anniversary
        s = new QString (decrypted);
        anniversary[i].date.append(s);
        delete s;

        //free the buffers
        free_dec_strings ();

        //read the size of the month
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted month
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of anniversary data.");
        }

        //assign the month to the anniversary
        s = new QString (decrypted);
        anniversary[i].month.append(s);
        delete s;

        //free the buffers
        free_dec_strings ();

        //read the size of the description
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted description
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of anniversary data.");
        }

        //assign the description to the anniversary
        s = new QString (decrypted);
        anniversary[i].description.append(s);
        delete s;

        //free the buffers
        free_dec_strings ();

        if (anniversary[i].description.size() > 0)
            max_anniversaries++;
    }

    fclose(encf);
}

void MainWindow::write_ann_file (int inivecflag)
{
QByteArray text;
int i, j = 0, k, len;

    for (i = 1; i < 367; i++) {
        j += anniversary[i].date.length();
    }

    if (j == 0) {//nothing to save
        unlink (Anniversaryfilename.toUtf8().data());
        return;
    }

    encf = fopen (Anniversaryfilename.toUtf8().data(), "w");
    if (encf == NULL)
        return;

    if (inivecflag == 1) {//pwd change in progress
        fwrite (&newiniVector, sizeof (char), 16, encf);
    }
    else {//normal data wrute in progress
        fwrite (&iniVector, sizeof (char), 16, encf);
    }

    //write each anniversary
    for (i = 1; i < 367; i++) {

        //date
        text = anniversary[i].date.toUtf8();
        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        k = aesenc ();
        if (k == 1) {
            crypt_error_notification ("Error in encryption of anniversary data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

        //month
        text = anniversary[i].month.toUtf8();
        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        k = aesenc ();
        if (k == 1) {
            crypt_error_notification ("Error in encryption of anniversary data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

        //description
        text = anniversary[i].description.toUtf8();
        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        k = aesenc ();
        if (k == 1) {
            crypt_error_notification ("Error in encryption of anniversary data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);
    }

    fclose(encf);
}

void MainWindow::read_contacts ()
{
QTreeWidgetItem *itcat, *itcon;
QString *s1, s;
QTextDocument doc;
int i, j, k, contact_count, toplevelcount;

    contree->clear();

    encf = fopen (Contactfilename.toUtf8().data(), "r");
    if (encf == NULL)
        return;

    szt = fread (&iniVector, sizeof (char), 16, encf);
    //number of groups
    szt = fread (&toplevelcount, sizeof (int), 1, encf);

    //loop over groups
    for (i = 0; i < toplevelcount; i++) {
        itcat = new QTreeWidgetItem (contree);
        contree->addTopLevelItem(itcat);

        //read the size of the name
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted group name
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of contacts data.");
        }

        //assign the group name to the tree
        s1 = new QString (decrypted);
        doc.setHtml(*s1);
        s = doc.toPlainText();

        itcat->setText(0, s);
        delete s1;

        //free the buffers
        free_dec_strings ();

        //read the size of the group description
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted group description
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of contacts data.");
        }

        //assign the group description to the tree
        s1 = new QString (decrypted);
        itcat->setText(1, *s1);
        delete s1;

        //free the buffers
        free_dec_strings ();

        szt = fread (&contact_count, sizeof (int), 1, encf);

        //loop over contacts for this group
        for (j = 0; j < contact_count; j++){
            itcon = new QTreeWidgetItem ();

            //read the size of the name
            szt = fread (&txtLength, sizeof (int), 1, encf);
            set_decrypt_variables();
            //read the encrypted name of this contact
            szt = fread (todecrypt, txtLengthpadded, 1, encf);

            k = aesdec ();
            if (k == 1) {
                crypt_error_notification("Error in decryption of contacts data.");
            }

            //assign the contact name to the tree
            s1 = new QString (decrypted);
            itcon->setText(0, *s1);
            delete s1;

            //free the buffers
            free_dec_strings ();

            //read the size of the contact data
            szt = fread (&txtLength, sizeof (int), 1, encf);
            set_decrypt_variables();
            //read the encrypted contact data
            szt = fread (todecrypt, txtLengthpadded, 1, encf);

            k = aesdec ();
            if (k == 1) {
                crypt_error_notification("Error in decryption of contacts data.");
            }

            //assign the contact data to the tree
            s1 = new QString (decrypted);
            itcon->setText(1, *s1);
            delete s1;

            //free the buffers
            free_dec_strings ();

            itcat->addChild(itcon);
        }
    }

    fclose(encf);

    contreeempty = false;
}

void MainWindow::write_contacts (int inivecflag)
{
QTreeWidgetItem *itcat, *itcon;
QByteArray text;
int i, j, k, len, toplevelcount, contact_count;

    //number of contact groups
    toplevelcount = contree->topLevelItemCount();

    if (toplevelcount == 0) {//nothing to save
        unlink (Contactfilename.toUtf8().data());
        return;
    }

    encf = fopen (Contactfilename.toUtf8().data(), "w");
    if (encf == NULL)
        return;

    if (inivecflag == 1) {//pwd change in progress
        fwrite (&newiniVector, sizeof (char), 16, encf);
    }
    else {//normal data write in progress
        fwrite (&iniVector, sizeof (char), 16, encf);
    }

    //number of groups
    fwrite (&toplevelcount, sizeof (int), 1, encf);

    //loop over groups
    for (i = 0; i < toplevelcount; i++){
        itcat = contree->topLevelItem(i);
        //number of contacts in this group
        contact_count = itcat->childCount();

        //name of group
        text = (itcat->text(0)).toUtf8();
        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        k = aesenc ();
        if (k == 1) {
            crypt_error_notification ("Error in encryption of contacts data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

        //description of group
        text = (itcat->text(1)).toUtf8();
        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        k = aesenc ();
        if (k == 1) {
            crypt_error_notification ("Error in encryption of contacts data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

        //no. of contacts in this group
        fwrite (&contact_count, sizeof (int), 1, encf);

        //loop over contacts of this group
        for (j = 0; j < contact_count; j++){
            itcon = itcat->child(j);

            //this is the name of the contact
            text = (itcon->text(0)).toUtf8();

            //size excludes the end NULL
            toencrypt = (char *) malloc (text.size() + 1);
            strcpy(toencrypt, "");
            //toencrypt contains the data to be encrypted
            strcpy(toencrypt, text.data());

            k = aesenc ();
            if (k == 1) {
                crypt_error_notification ("Error in encryption of contacts data.");
            }

            //save the size of the data
            len = text.size();
            fwrite (&len, sizeof (int), 1, encf);
            //save the encrypted data
            fwrite (encrypted, txtLengthpadded, 1, encf);

            free ((char *)encrypted);
            free ((char *)toencrypt);

            //this is the data for this contact
            text = itcon->text(1).toUtf8();
            //size excludes the end NULL
            toencrypt = (char *) malloc (text.size() + 1);
            strcpy(toencrypt, "");
            //toencrypt contains the data to be encrypted
            strcpy(toencrypt, text.data());

            k = aesenc ();
            if (k == 1) {
                crypt_error_notification ("Error in encryption of contacts data.");
            }

            //save the size of the data
            len = text.size();
            fwrite (&len, sizeof (int), 1, encf);
            //save the encrypted data
            fwrite (encrypted, txtLengthpadded, 1, encf);

            free ((char *)encrypted);
            free ((char *)toencrypt);
        }
    }

    fclose(encf);
}

void MainWindow::read_lists()
{
QTreeWidgetItem *it;
QString *s;
int i, k, toplevelcount;

    encf = fopen (Listfilename.toUtf8().data(), "r");
    if (encf == NULL)
        return;

    szt = fread (&iniVector, sizeof (char), 16, encf);
    //number of lists
    szt = fread (&toplevelcount, sizeof (int), 1, encf);

    //loop over list
    for (i = 0; i < toplevelcount; i++) {
        it = new QTreeWidgetItem (listree);
        listree->addTopLevelItem(it);

        //read the size of the list name
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted list name
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of lists data.");
        }

        //assign the list name to the tree
        s = new QString (decrypted);
        it->setText(0, *s);
        delete s;

        //free the buffers
        free_dec_strings ();

        //read the size of the list data
        szt = fread (&txtLength, sizeof (int), 1, encf);
        set_decrypt_variables();
        //read the encrypted list data
        szt = fread (todecrypt, txtLengthpadded, 1, encf);

        k = aesdec ();
        if (k == 1) {
            crypt_error_notification ("Error in decryption of lists data.");
        }

        //assign list data to tree
        s = new QString (decrypted);
        it->setText(1, *s);
        delete s;

        //free the buffers
        free_dec_strings ();
    }

    fclose(encf);

    listreeempty = false;
}

void MainWindow::write_lists(int inivecflag)
{
QByteArray text;
QTreeWidgetItem *it;
int i, j, len, toplevelcount;

    //number of categoriesqbytearray
    toplevelcount = listree->topLevelItemCount();

    if (toplevelcount == 0) {//nothing to save
        unlink (Listfilename.toUtf8().data());
        return;
    }

    encf = fopen (Listfilename.toUtf8().data(), "w");
    if (encf == NULL)
        return;

    if (inivecflag == 1) {//pwd change in progress
        fwrite (&newiniVector, sizeof (char), 16, encf);
    }
    else {//normal data write in progress
        fwrite (&iniVector, sizeof (char), 16, encf);
    }

    //number of lists
    fwrite (&toplevelcount, sizeof (int), 1, encf);

    //loop over lists
    for (i = 0; i < toplevelcount; i++) {
        it = listree->topLevelItem(i);

        //this is the name of the list
        text = (it->text(0)).toUtf8();

        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        j = aesenc ();
        if (j == 1) {
            crypt_error_notification ("Error in encryption of lists data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

        //this is the data in this list
        text = it->text(1).toUtf8().data();
        //size excludes the end NULL
        toencrypt = (char *) malloc (text.size() + 1);
        strcpy(toencrypt, "");
        //toencrypt contains the data to be encrypted
        strcpy(toencrypt, text.data());

        j = aesenc ();
        if (j == 1) {
            crypt_error_notification ("Error in encryption of lists data.");
        }

        //save the size of the data
        len = text.size();
        fwrite (&len, sizeof (int), 1, encf);
        //save the encrypted data
        fwrite (encrypted, txtLengthpadded, 1, encf);

        free ((char *)encrypted);
        free ((char *)toencrypt);

    }

    fclose(encf);
}

void set_decrypt_variables(void)
{
    //tdremainder which exceeds multiple of blkLength
    tdremainder = txtLength % blkLength;
    //no of characters to pad
    padding = blkLength - tdremainder - 1;
    //length to read after padding
    txtLengthpadded = txtLength + padding + 1;

    //allocate memory for buffer to hold data which is to be decrypted
    todecrypt = (char *) malloc(txtLengthpadded);
    decrypted = (char *) malloc(txtLengthpadded);

    strcpy (todecrypt, "");
    strcpy (decrypted, "");
}

void crypt_error_notification (const char *errstr)
{
QString s;

    s.append (errstr);

    msgBox = new QMessageBox ();
    msgBox->setText (s);
    msgBox->exec();
    delete msgBox;
}

void free_dec_strings (void)
{
    free ((char *)todecrypt);
    free ((char *)decrypted);

    tdremainder = txtLengthpadded = txtLength = padding = 0;
}
