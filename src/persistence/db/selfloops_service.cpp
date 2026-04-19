#include "selfloops_service.h"

#include "logger.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>

static const QString SELFLOOPS_UPLOAD_URL =
    QStringLiteral("https://www.selfloops.com/restapi/maximumtrainer/activities/upload.json");

// ── Gzip helper ───────────────────────────────────────────────────────────────
// Converts raw bytes to a valid gzip stream using the same algorithm as
// Util::zipFileHelperConvertToGzip (RFC 1952). Uses Qt's qCompress (zlib
// deflate) and rewraps the output with a proper gzip header/footer so the
// result is a standard .gz file that Selfloops expects.
static quint32 crc32buf(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (uchar byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return ~crc;
}

static QByteArray gzipCompress(const QByteArray &data)
{
    // qCompress produces: 4-byte big-endian original-size + 2-byte zlib header
    //                     + deflate stream + 4-byte zlib Adler-32 checksum.
    // Strip the first 6 bytes and the last 4 bytes to get the raw deflate stream.
    QByteArray deflated = qCompress(data);
    deflated.remove(0, 6);
    deflated.chop(4);

    // RFC 1952 gzip header: ID1 ID2 CM FLG MTIME(4) XFL OS
    QByteArray header;
    QDataStream ds1(&header, QIODevice::WriteOnly);
    ds1 << quint16(0x1f8b)   // ID1 + ID2
        << quint16(0x0800)   // CM=deflate, FLG=0
        << quint16(0x0000)   // MTIME low 2 bytes
        << quint16(0x0000)   // MTIME high 2 bytes
        << quint16(0x000b);  // XFL=0, OS=11 (NTFS/unknown)

    QByteArray footer;
    QDataStream ds2(&footer, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);
    ds2 << crc32buf(data)             // CRC-32 of uncompressed data
        << quint32(data.size());       // Size mod 2^32

    return header + deflated + footer;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void SelfloopsService::setCredentials(const QString &email, const QString &password)
{
    m_email    = email;
    m_password = password;
}

QNetworkAccessManager *SelfloopsService::networkManager()
{
    auto *mgr = qApp->property("NetworkManagerWS").value<QNetworkAccessManager *>();
    if (!mgr)
        LOG_WARN("SelfloopsService", QStringLiteral("NetworkManagerWS not available"));
    return mgr;
}

// ── uploadActivity ────────────────────────────────────────────────────────────

QNetworkReply *SelfloopsService::uploadActivity(const QString &filePath, const QString &note)
{
    QNetworkAccessManager *mgr = networkManager();
    if (!mgr)
        return nullptr;

    // Compress the activity file before upload (Selfloops expects gzip).
    const QString zipPath = filePath + QStringLiteral(".gz");
    {
        QFile inFile(filePath);
        QFile outFile(zipPath);
        if (!inFile.open(QIODevice::ReadOnly) || !outFile.open(QIODevice::WriteOnly)) {
            LOG_WARN("SelfloopsService",
                     QStringLiteral("uploadActivity: cannot compress file: ") + filePath);
            return nullptr;
        }
        outFile.write(gzipCompress(inFile.readAll()));
    }

    const QFileInfo fileInfo(zipPath);
    const QString   fileName = fileInfo.fileName();
    LOG_INFO("SelfloopsService", QStringLiteral("uploadActivity: ") + filePath);

    auto addField = [](QHttpMultiPart *mp, const QString &name, const QByteArray &body) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"") + name + QStringLiteral("\""));
        part.setBody(body);
        mp->append(part);
    };

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    addField(multiPart, QStringLiteral("email"), m_email.toUtf8());
    addField(multiPart, QStringLiteral("pw"),    m_password.toUtf8());
    addField(multiPart, QStringLiteral("note"),  note.toUtf8());

    // File part (gzip-compressed activity).
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"fitfile\"; filename=\"") +
                       fileName + QStringLiteral("\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/x-gzip"));

    QFile *file = new QFile(zipPath);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_WARN("SelfloopsService",
                 QStringLiteral("uploadActivity: cannot open compressed file: ") + zipPath);
        delete file;
        delete multiPart;
        return nullptr;
    }

    LOG_DEBUG("SelfloopsService",
              QStringLiteral("uploadActivity: compressed size ") +
              QString::number(file->size()));

    filePart.setBodyDevice(file);
    file->setParent(multiPart); // multiPart owns the file; both deleted with the reply.
    multiPart->append(filePart);

    QNetworkRequest request{QUrl(SELFLOOPS_UPLOAD_URL)};
    QNetworkReply *reply = mgr->post(request, multiPart);
    multiPart->setParent(reply);
    return reply;
}
