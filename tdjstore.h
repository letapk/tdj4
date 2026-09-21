#ifndef TDJSTORE_H
#define TDJSTORE_H

/*
  The Daily Journal - storage and crypto layer (Phase 3, v4-0.4).

  Qt-Core-only module. It has no dependency on tdj.h, MainWindow, or the
  widget stack, so it can be unit-tested (and linked) in isolation.

  Responsibilities:
    * encrypted-field codec (legacy AES-128-CBC, one field per block)
    * plain field {len,utf8} stream used inside a TDJ2 container
    * TDJ2 container: "TDJ2" header + PBKDF2 key + AES-256-GCM whole-body
    * all key / salt / IV ownership (CryptoManager) - no file-scope globals
    * TdjEncryptedFile: open+decrypt a store, or serialize+encrypt+save one

  Binary layouts and key semantics are unchanged from v4-0.3; this file only
  relocates them out of the GUI sources so the GUI can never touch a cipher
  handle, key buffer, salt, or magic byte directly.
*/

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QIODevice>
#include <QSet>
#include <QString>
#include <QVector>

//plain data models for the six store types. These used to be file-scope
//globals in the GUI sources (extern chains everywhere); StorageManager below
//owns them now, so they are Qt-Core-only plain data.
class Note
{
public:
    QString data;
    //cached flag: has the note any visible content at all? This is maintained
    //by StorageManager::loadJournal() and by the editor save path so that the
    //calendar formatting code does not have to build QTextDocuments on every
    //keystroke. It mirrors the old "length > 0 after stripping HTML" test,
    //except that an <img> reference also counts as content (matching
    //QTextDocument, which substitutes the image for the U+FFFC placeholder).
    bool hasText = false;
};

//appointments grid for the month on display, day begins from 1
class Appointment
{
public:
    //no. of appointments for this day
    int total = 0;
    //time and description data for each appointment, from 1-48, 0 is not used
    QString apptime[49], apptdesc[49];
};

class Anniversary
{
public:
    QString date, month, description;
};

//contacts / lists are model-driven in the GUI (QTreeWidget); serialization
//crosses the widget boundary through these plain exchange records
struct TdjItem { QString name, data; };
struct TdjGroup { QString name, desc; QVector<TdjItem> items; };
struct TdjList { QString name, data; };

//maximum size of one plain/cipher field read from (or written to) a file.
//Legacy readers happily malloc'd whatever length a (possibly corrupt) file
//claimed; worse, the old writer stored UTF-16 character counts while the
//cipher text was sized on UTF-8 bytes, so multi-byte notes caused a 16-byte
//stream desync per field. We bound every read so a broken file can never
//request a huge allocation or cause out-of-bounds access.
const qint32 kMaxFieldBytes = 8 * 1024 * 1024;

//ceiling for one image imported into the encrypted attachment store; a larger
//file is refused by putAttachment() (the old insertImage copied any file size
//into the data dir as plaintext - that copy is what T2 eliminates)
const qint32 kMaxAttachmentBytes = 16 * 1024 * 1024;

//extract every `tdj-image:<id>` reference from an editor-HTML string. Used by
//the orphan scan: an attachment is only kept while some stored content still
//references it.
QStringList tdj_attachment_refs(const QString &html);

//Qt-Core approximation of "does this editor HTML contain any visible content?"
//Mirrors QTextDocument::toPlainText().isEmpty() for the HTML the editors
//produce: <br>/<style>/<p> scaffolding is invisible, an <img> tag or a
//decoded entity (&nbsp; is invisible whitespace to the eye but QTextDocument
//counts it) marks the note as non-empty. Conservative - prefers to keep a
//stray (empty) month file over deleting one that holds an image.
bool tdj_html_has_text(const QString &html);

enum TdjFieldResult { TdjFieldEof = 0, TdjFieldOk = 1, TdjFieldCorrupt = -1 };

// 0=missing file, 1=TDJ2, 2=legacy
int  tdj_detect_format(QIODevice &f);

// legacy CBC codec (QIODevice& so QBuffer works for TDJ2 callers too)
// each call uses its own AES-128-CBC handle: no shared cipher state, so the
// same IV is never reused for unrelated fields and the module is reentrant
int  tdj_read_field_len(QIODevice &f, qint32 &len);
int  tdj_read_field_body(QIODevice &f, qint32 len, QByteArray &out,
                         const char *key, const char *iv);
bool tdj_write_field(QByteArray &out, const QByteArray &plain,
                     const char *key, const char *iv);

// plain (unencrypted) field I/O used by the TDJ2 container path
int  tdj_read_plain_body(QIODevice &f, qint32 len, QByteArray &out);
void tdj_append_field(QByteArray &out, const QByteArray &utf8);

//atomically replace `path` with `body` (temp file + rename), returns false on
//failure, leaving any existing file untouched
bool tdj_write_atomic_file(const QString &path, const QByteArray &body);

// PBKDF2-HMAC-SHA256: password + 16B salt → 32B key, 300 000 iterations.
// Zeroes key32 on failure so a broken derivation can never encrypt with
// garbage; the caller decides whether/how the user is informed.
void tdj2_derive_key(const QString &password, const char *salt16, char *key32);

// read the salt from an already-open TDJ2 file (header already peeked at "TDJ2")
int  tdj2_read_salt(QIODevice &f, QByteArray &salt);

// file types for the TDJ2 container file-type byte
enum Tdj2FileType { Tdj2Notes=1, Tdj2Appointments=2, Tdj2DailyAppts=3,
                    Tdj2Anns=4, Tdj2Contacts=5, Tdj2Lists=6, Tdj2Attachments=7 };

//database state used by the startup password flow
enum TdjDbState { TdjDbFresh = 0, TdjDbTdj2 = 1, TdjDbLegacy = 2, TdjDbMixed = 3 };

// ---------------------------------------------------------------------------
// Unified field reader. Each store file is either a legacy CBC stream (an
// IV then len-prefixed independently-CBC-encrypted fields) or a TDJ2 file
// whose whole payload was decrypted into a QBuffer. A single read loop can
// serve both: set dev/format once then read fields through this object.
// ---------------------------------------------------------------------------
class TdjFieldSource {
public:
    QIODevice *dev = nullptr;
    int format = 2;          //1=TDJ2 plain body, 2=legacy encrypted stream
    const char *key = nullptr;//legacy CBC key / IV are only used for format 2
    const char *iv = nullptr;

    int readLen(qint32 &len) { return tdj_read_field_len(*dev, len); }
    int readBody(qint32 len, QByteArray &out)
    {
        if (format == 1)
            return tdj_read_plain_body(*dev, len, out);
        return tdj_read_field_body(*dev, len, out,
                                   key ? key : "", iv ? iv : "");
    }
    //raw read used for the plain-text integer counts in contacts/lists
    int readRaw(char *buf, qint64 n)
    {
        qint64 got = dev->read(buf, n);
        if (got == n) return 1;
        return (got == 0 && dev->atEnd()) ? 0 : -1;
    }
};

// ---------------------------------------------------------------------------
// CryptoManager owns every secret and the database salt. There is exactly one
// instance (a MainWindow member); no file-scope buffer is ever shared.
//
//   - m_dbSalt[16]   fixed per-database random salt, identical in every file
//   - m_aesSymKey32  current session key
//   - m_old/new      keys in use during a password change or migration
//   - m_legacyKey/iv AES-128-CBC key + last-read IV for legacy-format files
// ---------------------------------------------------------------------------
class CryptoManager {
public:
    //return 0 when the gcrypt library is usable, 1 otherwise (must be called
    //before any other gcrypt use in the process)
    static int init();

    //database salt
    void initDbSalt(const QString &dirPath);       //reload from a TDJ2 file or mint
    void setDbSalt(const char *salt16);
    const char *dbSalt() const { return m_dbSalt; }
    bool dbSaltValid() const { return m_dbSaltValid; }

    //session keys (AES-256). inivecflag==1 => a re-encryption is in progress
    //(read with the old key, write with the new one)
    void deriveSessionKey(const QString &password);  //KDF against current db salt
    void deriveIntoNewKey(const QString &password);
    void setSessionKey(const char *key32);
    const char *sessionKey() const { return m_aesSymKey32; }
    const char *readKey32(int inivecflag) const  { return inivecflag == 1 ? m_oldAesSymKey32 : m_aesSymKey32; }
    const char *writeKey32(int inivecflag) const { return inivecflag == 1 ? m_newAesSymKey32 : m_aesSymKey32; }
    void copySessionToOldAndNew();                 //migration / fresh database
    void copySessionToOld();                       //password change: begin
    void commitNewToSession();                     //password change: done

    //legacy AES-128-CBC key/IV (used to read legacy-format files). The legacy
    //key is installed once from the password at login, the IV per file.
    void setLegacyKey(const QString &password);
    void setLegacyIv(const char *iv16);
    const char *legacyKey() const { return m_legacyKey; }
    const char *legacyIv() const { return m_legacyIv; }

    //TDJ2 container
    int  readContainer(QIODevice &f, const char *key32, QByteArray &plain);
    //write container with the key selected by inivecflag (writeKey32); used
    //by TdjEncryptedFile::write
    bool writeContainer(const QString &path, quint8 fileType,
                        const QByteArray &plain, int inivecflag);

    //database scan / password verification (startup flow)
    int  scanDbState(const QString &dirPath);
    bool verifyPassword(const QString &dirPath);
    //verify an explicit key (used by tests); confirms the session key when 0
    bool verifyPasswordKey(const QString &dirPath, const char *key32);

private:
    bool writeContainerSigned(const QString &path, quint8 fileType,
                              const char *key32, const QByteArray &plain);

    char m_dbSalt[16] = {0};
    bool m_dbSaltValid = false;
    char m_aesSymKey32[32] = {0};
    char m_oldAesSymKey32[32] = {0};
    char m_newAesSymKey32[32] = {0};
    char m_legacyKey[16] = {0};
    char m_legacyIv[16] = {0};
};

// ---------------------------------------------------------------------------
// TdjEncryptedFile - the read side of a store. open() detects the on-disk
// format, decrypts it (TDJ2 whole-body, or prepares the legacy field stream)
// and exposes a ready TdjFieldSource. The backing QFile/QBuffer live inside
// the object for its whole lifetime, so the source stays valid until the
// object is destroyed. The static write() is the canonical way to save a
// serialised body as a TDJ2 container.
// ---------------------------------------------------------------------------
class TdjEncryptedFile {
public:
    explicit TdjEncryptedFile(CryptoManager &crypto);

    //open+decrypt. Returns:
    //  TdjFieldOk      ready; source() may be read field by field
    //  TdjFieldEof     the file does not exist (not an error - silently skip)
    //  TdjFieldCorrupt exists but cannot be read / decrypted / validated
    int open(const QString &path, const char *key32);

    TdjFieldSource &source() { return m_fs; }
    int format() const { return m_fs.format; }

    //serialise-free writer: encrypt `body` as a TDJ2 container at `path`,
    //atomically. `path` must already include any ".new" re-encryption suffix.
    static bool write(CryptoManager &crypto, const QString &path,
                      quint8 fileType, const QByteArray &body, int inivecflag);

private:
    CryptoManager &m_crypto;
    TdjFieldSource m_fs;
    QFile m_file;
    QByteArray m_body;         //owns the decrypted TDJ2 payload
    QBuffer m_buf;             //field device over m_body
};

// ---------------------------------------------------------------------------
// StorageManager - owns the in-memory data models (no more file-scope
// buffers / extern chains in the GUI) and the per-store field serialization.
// The GUI never touches a binary layout, a length prefix, or a magic byte:
// it calls load*/save* and binds widgets through the typed accessors.
//
// Key selection follows the app convention: inivecflag==1 means a password
// change / migration is in progress (reads use the old key, writes the new
// one, and the target path already carries any ".new" suffix). Month-partitioned
// stores (journal, appointments) and the daily-repeating store remove their
// target file when all content is empty and inivecflag==0, matching the
// original "no file means no data" behaviour; the flat stores always write.
// ---------------------------------------------------------------------------
class StorageManager {
public:
    StorageManager() { clearAll(); }

    //typed model access (1..31 for notes/appointments, 1..366 anniversaries)
    Note        &note(int d)       { return m_note[d]; }
    const Note  &note(int d) const { return m_note[d]; }
    Appointment &appt(int d)       { return m_appt[d]; }
    const Appointment &appt(int d) const { return m_appt[d]; }
    Appointment &daily()           { return m_daily; }
    const Appointment &daily() const { return m_daily; }
    Anniversary &ann(int d)        { return m_ann[d]; }
    const Anniversary &ann(int d) const { return m_ann[d]; }
    int maxAnns() const          { return m_maxAnns; }

    //reset every model to the empty state
    void clearAll();

    //store load/save. load returns TdjFieldOk when the store decoded cleanly,
    //TdjFieldEof when there is no file, TdjFieldCorrupt on a broken file.
    int  loadJournal   (CryptoManager &crypto, const QString &path, int inivecflag);
    bool saveJournal   (CryptoManager &crypto, const QString &path, int inivecflag);
    int  loadApptMonth (CryptoManager &crypto, const QString &path, int inivecflag);
    bool saveApptMonth (CryptoManager &crypto, const QString &path, int inivecflag);
    int  loadDailyApps (CryptoManager &crypto, const QString &path);
    bool saveDailyApps (CryptoManager &crypto, const QString &path, int inivecflag);
    int  loadAnns      (CryptoManager &crypto, const QString &path);
    bool saveAnns      (CryptoManager &crypto, const QString &path, int inivecflag);

    int  loadContacts  (CryptoManager &crypto, const QString &path,
                        QVector<TdjGroup> &groups);
    bool saveContacts  (CryptoManager &crypto, const QString &path, int inivecflag,
                        const QVector<TdjGroup> &groups);
    int  loadLists     (CryptoManager &crypto, const QString &path,
                        QVector<TdjList> &lists);
    bool saveLists     (CryptoManager &crypto, const QString &path, int inivecflag,
                        const QVector<TdjList> &lists);

    // -- encrypted attachment store (T2) ------------------------------------
    // images live ONLY inside the database. Each attachment has a content
    // address ("tdj-image:" + hex SHA-256 of its bytes) and is referenced as
    // <img src="tdj-image:<id>"> from the editor HTML, so the data dir never
    // holds a plaintext image file and identical images deduplicate.
    int  loadAttachments(CryptoManager &crypto, const QString &path, int inivecflag);
    bool saveAttachments(CryptoManager &crypto, const QString &path, int inivecflag);
    //store `bytes` and return its id, or an empty string when refused
    //(empty input or larger than kMaxAttachmentBytes); overwriting an id
    //with identical bytes is a no-op
    QString putAttachment(const QByteArray &bytes);
    bool getAttachment(const QString &id, QByteArray &out) const;
    QSet<QString> attachmentIds() const;
    int  attachmentCount() const { return m_attachments.size(); }
    //drop every stored attachment whose id is not in `keep`
    void pruneAttachments(const QSet<QString> &keep);

private:
    Note m_note[32];
    Appointment m_appt[32];   //index 1..31 mirrors the note array
    Appointment m_daily;      //today's repeating appointments
    Anniversary m_ann[367];   //index 1..366
    int m_maxAnns = 0;
    QHash<QString, QByteArray> m_attachments;
};

#endif // TDJSTORE_H