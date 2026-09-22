/*
  The Daily Journal - storage and crypto layer (Phase 3, v4-0.4).

  Qt-Core-only. Implements the encrypted-field codec, the TDJ2 container, the
  key/salt/IV ownership (CryptoManager) and the read/write facade
  (TdjEncryptedFile). No globals: every secret lives in a CryptoManager
  instance owned by the caller (a MainWindow member in the application).
*/

#include "tdjstore.h"
#include <gcrypt.h>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <cstring>

//number of PBKDF2 iterations. ~0.6 s on desktop hardware with SHA-256.
enum { kPbkdf2Iterations = 300000 };

//legacy field codec constants (AES-128-CBC)
enum { kFieldKeyLength = 16, kFieldBlockLength = 16 };

int CryptoManager::init()
{
    return gcry_check_version (GCRYPT_VERSION) ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Encrypted-field codec.
//
// One field, as written since the beginning (v4-0.1) up to here:
//
//   int32 len           the length of the plain text in UTF-8 bytes,
//                       excluding the trailing NUL
//   ciphertext          AES-128-CBC of [plain]['x' filler][NUL], padded so
//                       that the whole block is a multiple of the block size.
//
// The only change in Phase 1 is that len now always stores the UTF-8 *byte*
// count of the text (the legacy code stored QString::length() character
// counts, which desynced the stream whenever a field contained multibyte
// characters). ASCII data is byte-for-byte identical to the old format.
//
// Unlike the original, each call opens and closes its own cipher handle, so
// the function is reentrant and no IV is ever reused for different fields.
// ---------------------------------------------------------------------------

bool tdj_write_field(QByteArray &out, const QByteArray &plain,
                     const char *key, const char *iv)
{
    const qint32 len = plain.size();

    //block-align the plain text plus one trailing NUL, exactly like aesenc()
    int padding = int(kFieldBlockLength) - (len % int(kFieldBlockLength)) - 1;
    QByteArray padded = plain;
    padded.reserve(len + padding + 1);
    for (int i = 0; i < padding; i++)
        padded.append(char('x'));
    padded.append(char(0));
    Q_ASSERT(padded.size() % int(kFieldBlockLength) == 0);

    QByteArray cipher(padded.size(), Qt::Uninitialized);

    gcry_cipher_hd_t hd;
    gcry_error_t err = gcry_cipher_open(&hd, GCRY_CIPHER_AES128,
                                        GCRY_CIPHER_MODE_CBC, 0);
    if (!err) err = gcry_cipher_setkey(hd, key, kFieldKeyLength);
    if (!err) err = gcry_cipher_setiv(hd, iv, kFieldBlockLength);
    if (!err) err = gcry_cipher_encrypt(hd, cipher.data(),
                                        padded.size(), padded.constData(),
                                        padded.size());
    if (hd) gcry_cipher_close(hd);
    if (err)
        return false;

    //length prefix in host byte order, mimicking the legacy fwrite of an int
    out.append(reinterpret_cast<const char *>(&len), sizeof(qint32));
    out.append(cipher);

    return true;
}

int tdj_read_field_len(QIODevice &f, qint32 &len)
{
    char lbuf[4];
    qint64 got = f.read(lbuf, 4);
    if (got == 0 && f.atEnd())
        return TdjFieldEof;
    if (got != 4)
        return TdjFieldCorrupt;

    memcpy(&len, lbuf, 4);
    if (len < 0 || len > kMaxFieldBytes)
        return TdjFieldCorrupt;

    return TdjFieldOk;
}

int tdj_read_field_body(QIODevice &f, qint32 len, QByteArray &out,
                        const char *key, const char *iv)
{
    //the encrypted block contains plain + 'x' filler + NUL, block-aligned
    int padding = int(kFieldBlockLength) - (len % int(kFieldBlockLength)) - 1;
    qint64 total = qint64(len) + padding + 1;
    if (total <= 0 || total > qint64(kMaxFieldBytes) + int(kFieldBlockLength))
        return TdjFieldCorrupt;
    if (total > f.bytesAvailable())
        return TdjFieldCorrupt;   //field claims more data than the file holds

    QByteArray cipher = f.read(total);
    if (cipher.size() != total)
        return TdjFieldCorrupt;

    QByteArray plain(total, Qt::Uninitialized);

    gcry_cipher_hd_t hd = nullptr;
    gcry_error_t err = gcry_cipher_open(&hd, GCRY_CIPHER_AES128,
                                        GCRY_CIPHER_MODE_CBC, 0);
    if (!err) err = gcry_cipher_setkey(hd, key, kFieldKeyLength);
    if (!err) err = gcry_cipher_setiv(hd, iv, kFieldBlockLength);
    if (!err) err = gcry_cipher_decrypt(hd, plain.data(), total,
                                        cipher.constData(), total);
    if (hd) gcry_cipher_close(hd);
    if (err)
        return TdjFieldCorrupt;

    //truncate the padding and NUL
    out = plain.sliced(0, len);
    return TdjFieldOk;
}

//write a complete file atomically: the data goes to a temporary file in the
//same directory first and is then renamed over the target, so a crash or disk
//full error can never leave a truncated file in place of the old one.
bool tdj_write_atomic_file(const QString &path, const QByteArray &body)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(body) != body.size())
        return false;
    return f.commit();
}

// ---------------------------------------------------------------------------
// Plain (unencrypted) field I/O. The TDJ2 container encrypts the whole
// serialised body at once, so inside the container fields are just
// `int32 byte-count` followed by UTF-8 bytes (no per-field crypto).
// ---------------------------------------------------------------------------
void tdj_append_field(QByteArray &out, const QByteArray &utf8)
{
    qint32 len = utf8.size();
    out.append(reinterpret_cast<const char *>(&len), 4);
    out.append(utf8);
}

int tdj_read_plain_body(QIODevice &f, qint32 len, QByteArray &out)
{
    if (len < 0 || len > kMaxFieldBytes)
        return TdjFieldCorrupt;
    if (qint64(len) > f.bytesAvailable())
        return TdjFieldCorrupt;

    QByteArray raw = f.read(len);
    if (raw.size() != len)
        return TdjFieldCorrupt;

    //no trailing NUL: this must mirror tdj_read_field_body so that the TDJ2
    //and legacy read paths of a store return identical content
    out = raw;
    return TdjFieldOk;
}

int tdj_detect_format(QIODevice &f)
{
    char magic[4];
    qint64 got = f.read(magic, 4);
    if (got == 0 && f.atEnd())
        return TdjFieldEof;
    if (got != 4)
        return TdjFieldCorrupt;
    if (memcmp(magic, "TDJ2", 4) == 0)
        return 1;
    return 2;
}

// ---------------------------------------------------------------------------
// TDJ2 container
//
//   magic  "TDJ2"       4 bytes
//   version u32          (host byte order, =1)   [offset 4]
//   fileType u8                                  [offset 8]
//   salt    16 bytes                             [offset 9]
//   nonce   12 bytes      fresh random every save [offset 25]
//   plen    u64          ciphertext length        [offset 37]
//   ciphertext + 16-byte GCM tag                 [offset 45]
//
// The whole serialised body (the same len-prefixed field stream the legacy
// format used, but unencrypted) is AES-256-GCM encrypted as one block with a
// fresh random nonce. The GCM tag IS the integrity check: a wrong password
// or a tampered file fails to authenticate and is reported as corrupt, which
// is what replaces the Checkpwd.tdj verification file.
//
// Key = PBKDF2-HMAC-SHA256(password UTF-8, salt, 300 000 iterations) → 32 B.
// Salt is fixed for the lifetime of a database and identical in every file;
// a fresh salt is minted when the database is first created. (Argon2id is
// specified in the roadmap but this libgcrypt build lacks it, so PBKDF2 with
// a high iteration count is the active KDF.)
// ---------------------------------------------------------------------------

void tdj2_derive_key(const QString &password, const char *salt16, char *key32)
{
    QByteArray pass = password.toUtf8();
    gcry_error_t err = gcry_kdf_derive(pass.constData(), pass.size(),
                                       GCRY_KDF_PBKDF2, GCRY_MD_SHA256,
                                       salt16, 16, kPbkdf2Iterations, 32, key32);
    //gcry_kdf_derive fails only on impossible inputs here (fixed length),
    //but zero the key defensively so a broken derivation can never encrypt
    //silently with garbage. The caller decides whether/how to inform the user.
    if (err)
        memset(key32, 0, 32);
}

int tdj2_read_salt(QIODevice &f, QByteArray &salt)
{
    //f is positioned after the magic (tdj_detect_format already consumed the
    //4 magic bytes), mirroring tdj2_read_container's convention
    QByteArray first = f.read(4 + 1 + 16);   //version+fileType+salt
    if (first.size() < 21)
        return TdjFieldCorrupt;
    quint32 version;
    memcpy(&version, first.constData(), 4);
    if (version != 1)
        return TdjFieldCorrupt;
    salt = first.sliced(5, 16);
    return TdjFieldOk;
}

// ---------------------------------------------------------------------------
// CryptoManager
// ---------------------------------------------------------------------------

void CryptoManager::setDbSalt(const char *salt16)
{
    memcpy(m_dbSalt, salt16, 16);
    m_dbSaltValid = true;
}

void CryptoManager::initDbSalt(const QString &dirPath)
{
    QDir dir(dirPath);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.tdj"));

    const QFileInfoList list = dir.entryInfoList();
    for (const QFileInfo &fi : list) {
        if (fi.fileName() == "Checkpwd.tdj")
            continue;
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        int fmt = tdj_detect_format(f);
        QByteArray salt;
        if (fmt == 1 && tdj2_read_salt(f, salt) == TdjFieldOk) {
            memcpy(m_dbSalt, salt.constData(), 16);
            m_dbSaltValid = true;
            f.close();
            return;
        }
        f.close();
    }

    //no TDJ2 file found: mint a fresh database salt
    gcry_create_nonce(m_dbSalt, 16);
    m_dbSaltValid = true;
}

void CryptoManager::deriveSessionKey(const QString &password)
{
    tdj2_derive_key(password, m_dbSalt, m_aesSymKey32);
}

void CryptoManager::deriveIntoNewKey(const QString &password)
{
    tdj2_derive_key(password, m_dbSalt, m_newAesSymKey32);
}

void CryptoManager::setSessionKey(const char *key32)
{
    memcpy(m_aesSymKey32, key32, 32);
}

void CryptoManager::copySessionToOldAndNew()
{
    memcpy(m_oldAesSymKey32, m_aesSymKey32, 32);
    memcpy(m_newAesSymKey32, m_aesSymKey32, 32);
}

void CryptoManager::copySessionToOld()
{
    memcpy(m_oldAesSymKey32, m_aesSymKey32, 32);
}

void CryptoManager::commitNewToSession()
{
    memcpy(m_aesSymKey32, m_newAesSymKey32, 32);
}

void CryptoManager::setLegacyKey(const QString &password)
{
    //same layout the old startup code produced: the first 16 UTF-8 bytes of
    //the password, zero-padded (legacy used an AES-128 key derived thus)
    QByteArray k = password.toUtf8();
    if (k.size() > 16)
        k.truncate(16);
    k.resize(16, '\0');
    memcpy(m_legacyKey, k.constData(), 16);
}

void CryptoManager::setLegacyIv(const char *iv16)
{
    memcpy(m_legacyIv, iv16, 16);
}

int CryptoManager::readContainer(QIODevice &f, const char *key32,
                                 QByteArray &plain)
{
    //f is positioned after the magic (detect_format already consumed 4 bytes)
    unsigned char hdr[41];   //version(4)+fileType(1)+salt(16)+nonce(12)+plen(8)
    if (f.read(reinterpret_cast<char *>(hdr), 41) != 41)
        return TdjFieldCorrupt;

    quint32 version;
    memcpy(&version, hdr, 4);
    if (version != 1)
        return TdjFieldCorrupt;

    quint64 plen;
    memcpy(&plen, hdr + 33, 8);
    const quint64 kMax = qint64(kMaxFieldBytes) + 16;   //cipher + GCM tag
    if (plen < 16 || plen > kMax)                       //plen includes the tag
        return TdjFieldCorrupt;

    QByteArray cipher = f.read(qint64(plen));
    if (quint64(cipher.size()) != plen)
        return TdjFieldCorrupt;

    const char *salt = reinterpret_cast<const char *>(hdr) + 5;
    const char *nonce = reinterpret_cast<const char *>(hdr) + 21;

    //reject a file that claims a different database salt than the one this
    //session authenticated against (protects against mixed-db mixes)
    if (m_dbSaltValid && memcmp(salt, m_dbSalt, 16) != 0)
        return TdjFieldCorrupt;

    //remember the database salt of this file
    memcpy(m_dbSalt, salt, 16);
    m_dbSaltValid = true;

    //TDJ2 uses its own AES-256-GCM handle (never the shared AES-128-CBC one)
    gcry_cipher_hd_t hd = nullptr;
    gcry_error_t err = gcry_cipher_open(&hd, GCRY_CIPHER_AES256,
                                        GCRY_CIPHER_MODE_GCM, 0);
    if (err) return TdjFieldCorrupt;

    quint64 ptlen = plen - 16;
    QByteArray pt(ptlen, Qt::Uninitialized);

    if (!err) err = gcry_cipher_setkey(hd, key32, 32);
    if (!err) err = gcry_cipher_setiv(hd, nonce, 12);
    if (!err) err = gcry_cipher_decrypt(hd, pt.data(), ptlen,
                                        cipher.constData(), ptlen);
    if (!err) {
        //the last 16 bytes of the cipher read are the GCM tag; a wrong key or
        //a tampered file makes checktag fail. This is the whole-file password
        //check that replaces Checkpwd.tdj.
        err = gcry_cipher_checktag(hd, cipher.constData() + ptlen, 16);
    }

    gcry_cipher_close(hd);
    if (err)
        return TdjFieldCorrupt;

    plain = pt;
    return TdjFieldOk;
}

bool CryptoManager::writeContainerSigned(const QString &path, quint8 fileType,
                                         const char *key32,
                                         const QByteArray &plain)
{
    QByteArray cipher(plain.size(), Qt::Uninitialized);
    char nonce[12];
    gcry_create_nonce(nonce, 12);

    gcry_cipher_hd_t hd = nullptr;
    gcry_error_t err = gcry_cipher_open(&hd, GCRY_CIPHER_AES256,
                                        GCRY_CIPHER_MODE_GCM, 0);
    if (err) return false;

    if (!err) err = gcry_cipher_setkey(hd, key32, 32);
    if (!err) err = gcry_cipher_setiv(hd, nonce, 12);
    if (!err) err = gcry_cipher_encrypt(hd, cipher.data(), plain.size(),
                                        plain.constData(), plain.size());

    char tag[16];
    if (!err) err = gcry_cipher_gettag(hd, tag, 16);

    gcry_cipher_close(hd);
    if (err)
        return false;

    QByteArray body;
    body.append("TDJ2", 4);
    quint32 version = 1;
    body.append(reinterpret_cast<const char *>(&version), 4);
    body.append(char(fileType));
    body.append(m_dbSalt, 16);
    body.append(nonce, 12);
    quint64 plen = quint64(plain.size()) + 16;
    body.append(reinterpret_cast<const char *>(&plen), 8);
    body.append(cipher);
    body.append(tag, 16);

    return tdj_write_atomic_file(path, body);
}

bool CryptoManager::writeContainer(const QString &path, quint8 fileType,
                                   const QByteArray &plain, int inivecflag)
{
    return writeContainerSigned(path, fileType, writeKey32(inivecflag), plain);
}

int CryptoManager::scanDbState(const QString &dirPath)
{
    QDir dir(dirPath);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.tdj"));

    bool hasTdj2 = false, hasLegacy = false;
    const QFileInfoList list = dir.entryInfoList();
    for (const QFileInfo &fi : list) {
        if (fi.fileName() == "Checkpwd.tdj")
            continue;
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        int fmt = tdj_detect_format(f);
        f.close();
        if (fmt == 1) hasTdj2 = true;
        else if (fmt == 2) hasLegacy = true;
    }

    if (hasTdj2 && hasLegacy) return TdjDbMixed;
    if (hasTdj2) return TdjDbTdj2;
    if (hasLegacy) return TdjDbLegacy;
    return TdjDbFresh;
}

bool CryptoManager::verifyPasswordKey(const QString &dirPath, const char *key32)
{
//The GCM tag of any existing TDJ2 store is the whole-file password check:
//only the correct key can authenticate and decrypt it.
    QDir dir(dirPath);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList("*.tdj"));

    const QFileInfoList list = dir.entryInfoList();
    for (const QFileInfo &fi : list) {
        if (fi.fileName() == "Checkpwd.tdj")
            continue;
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        int fmt = tdj_detect_format(f);
        if (fmt != 1) {
            f.close();
            continue;
        }
        QByteArray body;
        int r = readContainer(f, key32, body);
        f.close();
        if (r == TdjFieldOk)
            return true;
        return false;//a TDJ2 file that fails the tag: wrong password
    }
    return true;//no TDJ2 store present - the caller decides
}

bool CryptoManager::verifyPassword(const QString &dirPath)
{
    return verifyPasswordKey(dirPath, m_aesSymKey32);
}

// ---------------------------------------------------------------------------
// TdjEncryptedFile
// ---------------------------------------------------------------------------

TdjEncryptedFile::TdjEncryptedFile(CryptoManager &crypto)
    : m_crypto(crypto)
{
}

int TdjEncryptedFile::open(const QString &path, const char *key32)
{
    if (!QFileInfo::exists(path))
        return TdjFieldEof;                    //a store that is simply absent
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly))
        return TdjFieldCorrupt;

    int fmt = tdj_detect_format(m_file);
    if (fmt == 1) {//TDJ2: decrypt the whole payload, then read plain fields
        QByteArray plain;
        if (m_crypto.readContainer(m_file, key32, plain) != TdjFieldOk)
            return TdjFieldCorrupt;
        m_body = plain;
        m_buf.setBuffer(&m_body);
        m_buf.open(QIODevice::ReadOnly);
        m_fs.dev = &m_buf;
        m_fs.format = 1;
    }
    else if (fmt == 2) {//legacy encrypted stream: keep the QFile as the device
        //legacy files have no magic — they start with a 16-byte IV; detect
        //consumed the first 4 bytes so rewind to read the full IV.
        m_file.seek(0);
        QByteArray iv;
        iv.resize(16);
        if (m_file.read(iv.data(), 16) != 16)
            return TdjFieldCorrupt;
        m_crypto.setLegacyIv(iv.constData());
        m_fs.dev = &m_file;
        m_fs.format = 2;
        m_fs.key = m_crypto.legacyKey();
        m_fs.iv = m_crypto.legacyIv();
        //peek the first field length: garbage that cannot be a legacy stream
        //is reported as corrupt here instead of failing on the first read.
        qint32 firstLen;
        int lk = tdj_read_field_len(m_file, firstLen);
        if (lk == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        m_file.seek(16);                  //back to the true start of fields
    }
    else if (fmt == TdjFieldEof) {
        return TdjFieldEof;               //an empty file is a cleanly empty store
    }
    else {
        return TdjFieldCorrupt;
    }
    return TdjFieldOk;
}

bool TdjEncryptedFile::write(CryptoManager &crypto, const QString &path,
                             quint8 fileType, const QByteArray &body,
                             int inivecflag)
{
    return crypto.writeContainer(path, fileType, body, inivecflag);
}

// ---------------------------------------------------------------------------
// StorageManager - the six store models and their field serialization.
// ---------------------------------------------------------------------------

bool tdj_html_has_text(const QString &html)
{
    //an <img> reference is visible content: QTextDocument substitutes the
    //image for the U+FFFC placeholder character, so such a note is "text"
    if (html.contains(QRegularExpression("<img\\b",
            QRegularExpression::CaseInsensitiveOption)))
        return true;

    QString s = html;
    static const QRegularExpression headBlock(
        "<(style|script|head)[^>]*>.*?</\\1\\s*>",
        QRegularExpression::CaseInsensitiveOption |
        QRegularExpression::DotMatchesEverythingOption);
    s.remove(headBlock);
    static const QRegularExpression comment("<!--.*?-->",
        QRegularExpression::DotMatchesEverythingOption);
    s.remove(comment);
    static const QRegularExpression tag("<[^>]*>");
    s.remove(tag);

    //an entity (&nbsp;, &amp;, &#NN;) is content to QTextDocument even though
    //some render as whitespace on screen
    if (s.contains(QRegularExpression("&(?:[a-zA-Z]+|#[0-9]+|#x[0-9a-fA-F]+);")))
        return true;

    for (const QChar &c : s)
        if (!c.isSpace())
            return true;
    return false;
}

int tdj_next_appointment_minutes(const int times[48], int nowMinute)
{
    int best = -1;

    for (int row = 0; row < 48; row++) {
        const int t = times[row];
        if (t == 0)//blank row
            continue;

        const int hh = t / 100, mm = t % 100;
        if (mm > 59)
            continue;
        int apptMin;
        if (hh == 24 && mm == 0)//24:00 means the end of the day
            apptMin = 24 * 60;
        else if (hh > 23)
            continue;
        else
            apptMin = hh * 60 + mm;

        const int delta = apptMin - nowMinute;
        if (delta <= 5)//passed, now, or within the alarm lead
            continue;

        if (best < 0 || delta < best)
            best = delta;
    }

    return best;
}

void StorageManager::clearAll()
{
    for (int i = 0; i < 32; i++) {
        m_note[i].data.clear();
        m_note[i].hasText = false;
        m_appt[i].total = 0;
        for (int r = 0; r < 49; r++) {
            m_appt[i].apptime[r].clear();
            m_appt[i].apptdesc[r].clear();
        }
    }
    m_daily.total = 0;
    for (int r = 0; r < 49; r++) {
        m_daily.apptime[r].clear();
        m_daily.apptdesc[r].clear();
    }
    for (int i = 0; i < 367; i++) {
        m_ann[i].date.clear();
        m_ann[i].month.clear();
        m_ann[i].description.clear();
    }
    m_maxAnns = 0;
    m_attachments.clear();
}

int StorageManager::loadJournal(CryptoManager &crypto, const QString &path,
                                int inivecflag)
{
    for (int i = 1; i < 32; i++) {
        m_note[i].data.clear();
        m_note[i].hasText = false;
    }

    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(inivecflag));
    if (r != TdjFieldOk)
        return r;//Eof (no file) or Corrupt
    TdjFieldSource &fs = store.source();

    if (fs.format == 2) {
        //legacy journal: all 31 day lengths come first as plain ints, then
        //the body of each non-empty day in day order
        qint32 lens[32] = {};
        for (int i = 1; i <= 31; i++) {
            int r2 = fs.readLen(lens[i]);
            if (r2 == TdjFieldEof)
                break;//file ends cleanly here, remaining days are empty
            if (r2 == TdjFieldCorrupt)
                return TdjFieldCorrupt;
        }
        for (int i = 1; i <= 31; i++) {
            if (lens[i] == 0)
                continue;
            QByteArray plain;
            if (fs.readBody(lens[i], plain) == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            m_note[i].data = QString::fromUtf8(plain);
            m_note[i].hasText = tdj_html_has_text(m_note[i].data);
        }
        return TdjFieldOk;
    }

    //TDJ2: interleaved [len][body] forward stream, one field per day
    for (int i = 1; i <= 31; i++) {
        qint32 len;
        int r2 = fs.readLen(len);
        if (r2 == TdjFieldEof)
            return TdjFieldOk;//clean end, remaining days are empty
        if (r2 == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (len == 0)
            continue;
        QByteArray plain;
        if (fs.readBody(len, plain) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        m_note[i].data = QString::fromUtf8(plain);
        m_note[i].hasText = tdj_html_has_text(m_note[i].data);
    }
    return TdjFieldOk;
}

bool StorageManager::saveJournal(CryptoManager &crypto, const QString &path,
                                 int inivecflag)
{
    QByteArray body;
    for (int i = 1; i <= 31; i++) {
        QByteArray utf8 = m_note[i].data.toUtf8();
        if (utf8.isEmpty()) {//empty length prefix, no data
            tdj_append_field(body, QByteArray());
            continue;
        }
        tdj_append_field(body, utf8);
    }

    if (inivecflag == 0) {//normal save: a month with no visible content leaves
        bool any = false; //no file behind (matches the pre-r2 behaviour). The
        for (int i = 1; i <= 31; i++) { //gate is derived from the data itself,
            if (tdj_html_has_text(m_note[i].data)) { //so a stale hasText cache
                any = true;//can never resurrect an emptied data file
                break;
            }
        }
        if (!any) {
            QFile::remove(path);
            return true;
        }
    }

    return TdjEncryptedFile::write(crypto, path, Tdj2Notes, body, inivecflag);
}

int StorageManager::loadApptMonth(CryptoManager &crypto, const QString &path,
                                  int inivecflag)
{
    for (int i = 1; i < 32; i++) {
        m_appt[i].total = 0;
        for (int row = 0; row < 49; row++) {
            m_appt[i].apptime[row].clear();
            m_appt[i].apptdesc[row].clear();
        }
    }

    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(inivecflag));
    if (r != TdjFieldOk)
        return r;
    TdjFieldSource &fs = store.source();

    for (int i = 1; i <= 31; i++) {
        for (int row = 0; row < 48; row++) {
            QByteArray time, desc;
            qint32 len;
            int k = fs.readLen(len);
            if (k == TdjFieldEof || k == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            if (fs.readBody(len, time) == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            k = fs.readLen(len);
            if (k == TdjFieldEof || k == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            if (fs.readBody(len, desc) == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            m_appt[i].apptime[row + 1] = QString::fromUtf8(time);
            m_appt[i].apptdesc[row + 1] = QString::fromUtf8(desc);
            if (m_appt[i].apptime[row + 1].size() +
                m_appt[i].apptdesc[row + 1].size() > 0)
                (m_appt[i].total)++;
        }
    }
    return TdjFieldOk;
}

bool StorageManager::saveApptMonth(CryptoManager &crypto, const QString &path,
                                   int inivecflag)
{
    QByteArray body;
    for (int i = 1; i <= 31; i++) {
        for (int row = 0; row < 48; row++) {
            tdj_append_field(body, m_appt[i].apptime[row + 1].toUtf8());
            tdj_append_field(body, m_appt[i].apptdesc[row + 1].toUtf8());
        }
    }

    if (inivecflag == 0) {//normal save: an empty month leaves no file behind
        bool any = false;
        for (int i = 1; i <= 31 && !any; i++)
            for (int row = 0; row < 48 && !any; row++)
                any = !m_appt[i].apptime[row + 1].isEmpty()
                   || !m_appt[i].apptdesc[row + 1].isEmpty();
        if (!any) {
            QFile::remove(path);
            return true;
        }
    }

    return TdjEncryptedFile::write(crypto, path, Tdj2Appointments, body,
                                   inivecflag);
}

int StorageManager::loadDailyApps(CryptoManager &crypto, const QString &path)
{
    m_daily.total = 0;
    for (int row = 0; row < 49; row++) {
        m_daily.apptime[row].clear();
        m_daily.apptdesc[row].clear();
    }

    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(0));
    if (r != TdjFieldOk)
        return r;
    TdjFieldSource &fs = store.source();

    for (int row = 0; row < 48; row++) {
        QByteArray time, desc;
        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (fs.readBody(len, time) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (fs.readBody(len, desc) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        m_daily.apptime[row + 1] = QString::fromUtf8(time);
        m_daily.apptdesc[row + 1] = QString::fromUtf8(desc);
        if (m_daily.apptime[row + 1].size() + m_daily.apptdesc[row + 1].size() > 0)
            (m_daily.total)++;
    }
    return TdjFieldOk;
}

bool StorageManager::saveDailyApps(CryptoManager &crypto, const QString &path,
                                   int inivecflag)
{
    QByteArray body;
    for (int row = 0; row < 48; row++) {
        tdj_append_field(body, m_daily.apptime[row + 1].toUtf8());
        tdj_append_field(body, m_daily.apptdesc[row + 1].toUtf8());
    }

    if (inivecflag == 0) {//normal save: no repeating appointments, no file
        bool any = false;
        for (int row = 0; row < 48 && !any; row++)
            any = !m_daily.apptime[row + 1].isEmpty()
               || !m_daily.apptdesc[row + 1].isEmpty();
        if (!any) {
            QFile::remove(path);
            return true;
        }
    }

    return TdjEncryptedFile::write(crypto, path, Tdj2DailyAppts, body,
                                   inivecflag);
}

int StorageManager::loadAnns(CryptoManager &crypto, const QString &path)
{
    m_maxAnns = 0;
    for (int i = 1; i < 367; i++) {
        m_ann[i].date.clear();
        m_ann[i].month.clear();
        m_ann[i].description.clear();
    }

    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(0));
    if (r != TdjFieldOk)
        return r;
    TdjFieldSource &fs = store.source();

    for (int i = 1; i < 367; i++) {
        QByteArray date, month, desc;
        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (fs.readBody(len, date) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (fs.readBody(len, month) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (fs.readBody(len, desc) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        m_ann[i].date = QString::fromUtf8(date);
        m_ann[i].month = QString::fromUtf8(month);
        m_ann[i].description = QString::fromUtf8(desc);
        if (m_ann[i].description.size() > 0)
            m_maxAnns++;
    }
    return TdjFieldOk;
}

bool StorageManager::saveAnns(CryptoManager &crypto, const QString &path,
                              int inivecflag)
{
    if (inivecflag == 0) {//normal save: an anniversary set with no entries
        bool any = false; //leaves no file behind (mirrors the month stores).
        for (int i = 1; i < 367; i++) {//the only content criterion is a
            if (!m_ann[i].description.isEmpty()) {//non-empty description
                any = true;//(get_anniversary_items saves a row solely on it)
                break;
            }
        }
        if (!any) {
            QFile::remove(path);
            return true;
        }
    }

    QByteArray body;
    for (int i = 1; i < 367; i++) {
        tdj_append_field(body, m_ann[i].date.toUtf8());
        tdj_append_field(body, m_ann[i].month.toUtf8());
        tdj_append_field(body, m_ann[i].description.toUtf8());
    }
    return TdjEncryptedFile::write(crypto, path, Tdj2Anns, body, inivecflag);
}

static bool store_field_exists_and_sane(TdjFieldSource &fs, qint32 *outLen)
//read one length prefix; helper for the contacts/lists readers
{
    qint32 len;
    int k = fs.readLen(len);
    if (k == TdjFieldEof || k == TdjFieldCorrupt)
        return false;
    *outLen = len;
    return true;
}

int StorageManager::loadContacts(CryptoManager &crypto, const QString &path,
                                 QVector<TdjGroup> &groups)
{
    groups.clear();
    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(0));
    if (r != TdjFieldOk)
        return r;
    TdjFieldSource &fs = store.source();

    int toplevelcount;
    if (fs.readRaw(reinterpret_cast<char *>(&toplevelcount), 4) != 1)
        return TdjFieldCorrupt;
    if (toplevelcount < 0 || toplevelcount > kMaxFieldBytes)
        return TdjFieldCorrupt;

    for (int i = 0; i < toplevelcount; i++) {
        TdjGroup g;
        qint32 len;
        QByteArray name, gdesc;
        if (!store_field_exists_and_sane(fs, &len))
            return TdjFieldCorrupt;
        if (fs.readBody(len, name) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (!store_field_exists_and_sane(fs, &len))
            return TdjFieldCorrupt;
        if (fs.readBody(len, gdesc) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        g.name = QString::fromUtf8(name);
        g.desc = QString::fromUtf8(gdesc);

        int contact_count;
        if (fs.readRaw(reinterpret_cast<char *>(&contact_count), 4) != 1)
            return TdjFieldCorrupt;
        if (contact_count < 0 || contact_count > kMaxFieldBytes)
            return TdjFieldCorrupt;

        for (int j = 0; j < contact_count; j++) {
            TdjItem it;
            QByteArray cname, cdata;
            if (!store_field_exists_and_sane(fs, &len))
                return TdjFieldCorrupt;
            if (fs.readBody(len, cname) == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            if (!store_field_exists_and_sane(fs, &len))
                return TdjFieldCorrupt;
            if (fs.readBody(len, cdata) == TdjFieldCorrupt)
                return TdjFieldCorrupt;
            it.name = QString::fromUtf8(cname);
            it.data = QString::fromUtf8(cdata);
            g.items.append(it);
        }
        groups.append(g);
    }
    return TdjFieldOk;
}

bool StorageManager::saveContacts(CryptoManager &crypto, const QString &path,
                                  int inivecflag,
                                  const QVector<TdjGroup> &groups)
{
    if (inivecflag == 0 && groups.isEmpty()) {//normal save: no contacts ->
        QFile::remove(path);                   //no file (mirrors month stores)
        return true;
    }

    QByteArray body;
    int toplevelcount = groups.size();
    body.append(reinterpret_cast<const char *>(&toplevelcount), 4);

    for (const TdjGroup &g : groups) {
        tdj_append_field(body, g.name.toUtf8());
        tdj_append_field(body, g.desc.toUtf8());
        int contact_count = g.items.size();
        body.append(reinterpret_cast<const char *>(&contact_count), 4);
        for (const TdjItem &it : g.items) {
            tdj_append_field(body, it.name.toUtf8());
            tdj_append_field(body, it.data.toUtf8());
        }
    }

    return TdjEncryptedFile::write(crypto, path, Tdj2Contacts, body, inivecflag);
}

int StorageManager::loadLists(CryptoManager &crypto, const QString &path,
                              QVector<TdjList> &lists)
{
    lists.clear();
    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(0));
    if (r != TdjFieldOk)
        return r;
    TdjFieldSource &fs = store.source();

    int toplevelcount;
    if (fs.readRaw(reinterpret_cast<char *>(&toplevelcount), 4) != 1)
        return TdjFieldCorrupt;
    if (toplevelcount < 0 || toplevelcount > kMaxFieldBytes)
        return TdjFieldCorrupt;

    for (int i = 0; i < toplevelcount; i++) {
        TdjList l;
        QByteArray name, data;
        qint32 len;
        if (!store_field_exists_and_sane(fs, &len))
            return TdjFieldCorrupt;
        if (fs.readBody(len, name) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        if (!store_field_exists_and_sane(fs, &len))
            return TdjFieldCorrupt;
        if (fs.readBody(len, data) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        l.name = QString::fromUtf8(name);
        l.data = QString::fromUtf8(data);
        lists.append(l);
    }
    return TdjFieldOk;
}

bool StorageManager::saveLists(CryptoManager &crypto, const QString &path,
                               int inivecflag, const QVector<TdjList> &lists)
{
    if (inivecflag == 0 && lists.isEmpty()) {//normal save: no lists -> no file
        QFile::remove(path);              //(mirrors month stores)
        return true;
    }

    QByteArray body;
    int toplevelcount = lists.size();
    body.append(reinterpret_cast<const char *>(&toplevelcount), 4);

    for (const TdjList &l : lists) {
        tdj_append_field(body, l.name.toUtf8());
        tdj_append_field(body, l.data.toUtf8());
    }

    return TdjEncryptedFile::write(crypto, path, Tdj2Lists, body, inivecflag);
}

// ---------------------------------------------------------------------------
// Encrypted attachment store. One TDJ2 container (Tdj2Attachments) whose body
// is a field stream: for each attachment, [len][id][len][image bytes]. The id
// is the content address "tdj-image:" + lowercase hex SHA-256, so importing
// the same bytes twice produces the same id and the store never duplicates.
// ---------------------------------------------------------------------------

QStringList tdj_attachment_refs(const QString &html)
{
    static const QRegularExpression ref(
        "tdj-image:[0-9a-fA-F]+",
        QRegularExpression::CaseInsensitiveOption);
    QStringList out;
    QRegularExpressionMatchIterator it = ref.globalMatch(html);
    while (it.hasNext()) {
        QString id = it.next().captured(0);
        if (!out.contains(id, Qt::CaseInsensitive))
            out.append(id);
    }
    return out;
}

int StorageManager::loadAttachments(CryptoManager &crypto, const QString &path,
                                    int inivecflag)
{
    m_attachments.clear();

    TdjEncryptedFile store(crypto);
    int r = store.open(path, crypto.readKey32(inivecflag));
    if (r != TdjFieldOk)
        return r;//Eof (no store yet) or Corrupt

    TdjFieldSource &fs = store.source();
    while (true) {
        qint32 len;
        int k = fs.readLen(len);
        if (k == TdjFieldEof)
            break;//clean end
        if (k == TdjFieldCorrupt || len > kMaxFieldBytes)
            return TdjFieldCorrupt;
        QByteArray id;
        if (fs.readBody(len, id) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        k = fs.readLen(len);
        if (k == TdjFieldEof || k == TdjFieldCorrupt || len > kMaxFieldBytes)
            return TdjFieldCorrupt;
        QByteArray bytes;
        if (fs.readBody(len, bytes) == TdjFieldCorrupt)
            return TdjFieldCorrupt;
        m_attachments.insert(QString::fromUtf8(id), bytes);
    }
    return TdjFieldOk;
}

bool StorageManager::saveAttachments(CryptoManager &crypto, const QString &path,
                                     int inivecflag)
{
    if (inivecflag == 0 && m_attachments.isEmpty()) {
        QFile::remove(path);//no attachments -> no file (mirrors the month stores)
        return true;
    }

    QByteArray body;
    QHash<QString, QByteArray>::const_iterator i = m_attachments.constBegin();
    while (i != m_attachments.constEnd()) {
        tdj_append_field(body, i.key().toUtf8());
        tdj_append_field(body, i.value());
        ++i;
    }
    return TdjEncryptedFile::write(crypto, path, Tdj2Attachments, body,
                                   inivecflag);
}

QString StorageManager::putAttachment(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > kMaxAttachmentBytes)
        return QString();

    QString id = "tdj-image:"
        + QString::fromLatin1(QCryptographicHash::hash(bytes,
                            QCryptographicHash::Sha256).toHex());
    m_attachments.insert(id, bytes);
    return id;
}

bool StorageManager::getAttachment(const QString &id, QByteArray &out) const
{
    QHash<QString, QByteArray>::const_iterator i = m_attachments.constFind(id);
    if (i == m_attachments.constEnd())
        return false;
    out = i.value();
    return true;
}

QSet<QString> StorageManager::attachmentIds() const
{
    QSet<QString> ids;
    QHash<QString, QByteArray>::const_iterator i = m_attachments.constBegin();
    while (i != m_attachments.constEnd()) {
        ids.insert(i.key());
        ++i;
    }
    return ids;
}

void StorageManager::pruneAttachments(const QSet<QString> &keep)
{
    QMutableHashIterator<QString, QByteArray> i(m_attachments);
    while (i.hasNext()) {
        i.next();
        if (!keep.contains(i.key()))
            i.remove();
    }
}