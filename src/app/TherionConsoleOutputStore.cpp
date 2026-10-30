#include "TherionConsoleOutputStore.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
TherionConsoleOutputStore::TherionConsoleOutputStore(const QString &filePath)
    : file_(filePath)
{
}

bool TherionConsoleOutputStore::clear()
{
    file_.close();
    error_.clear();
    QDir().mkpath(QFileInfo(file_.fileName()).absolutePath());
    if (!file_.open(QIODevice::WriteOnly | QIODevice::Truncate, QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        error_ = file_.errorString();
        return false;
    }
    return true;
}

bool TherionConsoleOutputStore::append(const QString &text)
{
    if (!error_.isEmpty()) {
        return false;
    }
    if (!file_.isOpen() && !clear()) {
        return false;
    }
    const QByteArray bytes = text.toUtf8();
    qint64 offset = 0;
    while (offset < bytes.size()) {
        const qint64 written = file_.write(bytes.constData() + offset, bytes.size() - offset);
        if (written <= 0) {
            error_ = file_.errorString();
            return false;
        }
        offset += written;
    }
    if (!file_.flush()) {
        error_ = file_.errorString();
        return false;
    }
    return true;
}

bool TherionConsoleOutputStore::readAll(QString *text)
{
    text->clear();
    if (!error_.isEmpty()) {
        return false;
    }
    if (!file_.isOpen()) {
        return true;
    }
    QFile reader(file_.fileName());
    if (!reader.open(QIODevice::ReadOnly)) {
        error_ = reader.errorString();
        return false;
    }
    const QByteArray bytes = reader.readAll();
    if (reader.error() != QFileDevice::NoError) {
        error_ = reader.errorString();
        return false;
    }
    *text = QString::fromUtf8(bytes);
    return true;
}

QString TherionConsoleOutputStore::filePath() const
{
    return file_.fileName();
}

QString TherionConsoleOutputStore::errorString() const
{
    return error_;
}
}
