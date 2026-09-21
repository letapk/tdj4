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
#include <QDir>
#include <QFileInfo>
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