#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QDebug>
#include <QtEndian>
#include <limits>

static constexpr qsizetype PesHeaderSize = 0x10;
static const QByteArray PesMagic = QByteArray::fromHex("0001015745535953");

static bool writeOutputFile(const QString &fileName, const QByteArray &data)
{
    QFile output(fileName);
    if (!output.open(QIODevice::WriteOnly)) {
        qWarning() << "ERROR: Could not open output file:" << fileName;
        return false;
    }

    if (output.write(data) != data.size()) {
        qWarning() << "ERROR: Could not write complete output file:" << fileName;
        return false;
    }

    return true;
}

static bool isPesZlibFile(const QByteArray &data)
{
    return data.size() >= PesHeaderSize && data.first(PesMagic.size()) == PesMagic;
}

static bool unzlibFile(const QString &fileName, const QByteArray &data)
{
    if (!isPesZlibFile(data)) {
        qWarning() << "ERROR: This is not a PES zlibbed file!";
        return false;
    }

    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    const quint32 compressedSize = qFromLittleEndian<quint32>(raw + 0x08);
    const quint32 uncompressedSize = qFromLittleEndian<quint32>(raw + 0x0C);
    const qsizetype availableCompressedSize = data.size() - PesHeaderSize;

    if (compressedSize > static_cast<quint64>(availableCompressedSize)) {
        qWarning() << "ERROR: Invalid compressed size in PES header.";
        return false;
    }

    QByteArray qtCompressed;
    qtCompressed.resize(4);
    qToBigEndian<quint32>(uncompressedSize, reinterpret_cast<uchar *>(qtCompressed.data()));
    qtCompressed.append(data.constData() + PesHeaderSize, compressedSize);

    const QByteArray processedData = qUncompress(qtCompressed);
    if (processedData.size() != static_cast<qsizetype>(uncompressedSize)) {
        qWarning() << "ERROR: Unzlibbing failed or produced an unexpected output size.";
        return false;
    }

    if (!writeOutputFile(fileName + ".unzlib", processedData))
        return false;

    qInfo() << "SUCCESS! File was successfully unzlibbed.";
    return true;
}

static bool zlibFile(const QString &fileName, const QByteArray &data)
{
    if (isPesZlibFile(data)) {
        qWarning() << "ERROR: This file is already zlibbed.";
        return false;
    }

    if (data.size() > static_cast<qsizetype>(std::numeric_limits<quint32>::max())) {
        qWarning() << "ERROR: Input file is too large for the PES 32-bit zlib header.";
        return false;
    }

    QByteArray qtCompressed = qCompress(data, -1);
    if (qtCompressed.size() < 4) {
        qWarning() << "ERROR: Zlibbing failed.";
        return false;
    }

    qtCompressed.remove(0, 4);

    if (qtCompressed.size() > static_cast<qsizetype>(std::numeric_limits<quint32>::max())) {
        qWarning() << "ERROR: Compressed data is too large for the PES 32-bit zlib header.";
        return false;
    }

    const quint32 compressedSize = static_cast<quint32>(qtCompressed.size());
    const quint32 uncompressedSize = static_cast<quint32>(data.size());

    QByteArray processedData;
    processedData.reserve(PesHeaderSize + qtCompressed.size());
    processedData.append(PesMagic);

    char sizeBuffer[4];
    qToLittleEndian<quint32>(compressedSize, reinterpret_cast<uchar *>(sizeBuffer));
    processedData.append(sizeBuffer, sizeof(sizeBuffer));
    qToLittleEndian<quint32>(uncompressedSize, reinterpret_cast<uchar *>(sizeBuffer));
    processedData.append(sizeBuffer, sizeof(sizeBuffer));
    processedData.append(qtCompressed);

    if (!writeOutputFile(fileName + ".zlib", processedData))
        return false;

    qInfo() << "SUCCESS! File was successfully zlibbed.";
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();

    if (arguments.size() < 3) {
        qInfo("||||||||||||||||||||||||||||||||||||||||||");
        qInfo("|                                        |");
        qInfo("|           PES ZlibTool v1.00           |");
        qInfo("|  (c) 2021 IT World Software Services   |");
        qInfo("|                                        |");
        qInfo("||||||||||||||||||||||||||||||||||||||||||");
        qInfo("");
        qInfo("  Usage: zlibtool [options] <input-pattern>");
        qInfo("");
        qInfo("[Options]");
        qInfo("  -z: zlib the input file(s)");
        qInfo("  -u: unzlib the input file(s)");
        qInfo("");
        qInfo("* Information");
        qInfo("  Wildcards like *.bin or folder/**/*.bin are supported");
        qInfo("  The result of a zlibbed file will have a .zlib extension");
        qInfo("  The result of an unzlibbed file will have a .unzlib extension");
        return 0;
    }

    const QString modeFlag = arguments.at(1);
    const QString inputPattern = arguments.at(2);

    bool zlibMode = false;
    if (modeFlag == "-z") {
        zlibMode = true;
    } else if (modeFlag != "-u") {
        qWarning() << "Missing or invalid mode (-z or -u).";
        return 1;
    }

    const QFileInfo fileInfo(inputPattern);
    QString basePath = fileInfo.path();
    if (basePath == ".")
        basePath = QDir::currentPath();

    const QString pattern = fileInfo.fileName();
    QDirIterator iterator(basePath, QStringList() << pattern, QDir::Files, QDirIterator::Subdirectories);

    bool foundFiles = false;
    bool allSucceeded = true;

    while (iterator.hasNext()) {
        foundFiles = true;
        const QString filePath = iterator.next();
        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "ERROR: Could not open input file:" << filePath;
            allSucceeded = false;
            continue;
        }

        const QByteArray data = file.readAll();
        const bool succeeded = zlibMode ? zlibFile(filePath, data) : unzlibFile(filePath, data);
        allSucceeded = allSucceeded && succeeded;
    }

    if (!foundFiles) {
        qWarning() << "No matching files found for pattern:" << inputPattern;
        return 1;
    }

    return allSucceeded ? 0 : 1;
}
