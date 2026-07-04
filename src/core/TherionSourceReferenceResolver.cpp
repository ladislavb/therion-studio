#include "TherionSourceReferenceResolver.h"

#include "TherionSourceLogicalDocument.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
QString canonicalOrAbsoluteFilePath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return canonicalPath;
    }

    const QFileInfo parentInfo(info.absolutePath());
    const QString canonicalParentPath = parentInfo.canonicalFilePath();
    if (!canonicalParentPath.isEmpty()) {
        return QDir(canonicalParentPath).filePath(info.fileName());
    }

    return info.absoluteFilePath();
}

QString normalizedTherionSourceReferencePath(QString referencePath)
{
    referencePath = referencePath.trimmed();
    if (referencePath.size() >= 2) {
        const QChar first = referencePath.front();
        const QChar last = referencePath.back();
        if ((first == QLatin1Char('"') || first == QLatin1Char('\'')) && last == first) {
            referencePath = referencePath.mid(1, referencePath.size() - 2);
        }
    }
    referencePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(referencePath);
}

TherionSourcePhysicalRange therionSourceReferencePathRange(const TherionSourceLogicalCommand &command)
{
    TherionSourcePhysicalRange range;
    range.lineNumber = command.startLineNumber;
    range.lineText = command.text;

    int start = 0;
    while (start < command.text.size() && command.text.at(start).isSpace()) {
        ++start;
    }
    while (start < command.text.size() && !command.text.at(start).isSpace()) {
        ++start;
    }
    while (start < command.text.size() && command.text.at(start).isSpace()) {
        ++start;
    }
    if (start >= command.text.size()) {
        return {};
    }

    int end = start;
    if (command.text.at(start) == QLatin1Char('"')) {
        ++end;
        while (end < command.text.size()) {
            const QChar ch = command.text.at(end);
            ++end;
            if (ch == QLatin1Char('"')) {
                break;
            }
        }
    } else {
        while (end < command.text.size() && !command.text.at(end).isSpace()) {
            ++end;
        }
    }

    range.columnNumber = start + 1;
    range.columnLength = end - start;
    range.startOffset = command.startOffset + start;
    range.length = range.columnLength;
    return range;
}

QString therionSourceReferencePathToken(const TherionSourceLogicalCommand &command)
{
    const TherionSourcePhysicalRange range = therionSourceReferencePathRange(command);
    if (range.columnNumber <= 0 || range.columnLength <= 0) {
        return {};
    }
    return range.lineText.mid(range.columnNumber - 1, range.columnLength);
}

QStringList therionSourceReferencePathCandidates(const QString &currentFilePath, const QString &referencePath)
{
    const QString trimmedReference = normalizedTherionSourceReferencePath(referencePath);
    if (trimmedReference.isEmpty()) {
        return {};
    }

    QStringList candidates;
    const QDir currentDirectory = QFileInfo(currentFilePath).dir();
    auto appendCandidate = [&candidates](const QString &path) {
        const QString normalizedPath = canonicalOrAbsoluteFilePath(path);
        if (!normalizedPath.isEmpty() && !candidates.contains(normalizedPath)) {
            candidates.append(normalizedPath);
        }
    };

    appendCandidate(currentDirectory.filePath(trimmedReference));

    const QFileInfo absoluteCandidate(trimmedReference);
    if (absoluteCandidate.isAbsolute()) {
        appendCandidate(trimmedReference);
    }

    if (QFileInfo(trimmedReference).suffix().isEmpty()) {
        appendCandidate(currentDirectory.filePath(trimmedReference + QStringLiteral(".th")));
    }

    return candidates;
}

QString resolveTherionSourceReferencePath(const QString &currentFilePath, const QString &referencePath)
{
    for (const QString &candidate : therionSourceReferencePathCandidates(currentFilePath, referencePath)) {
        if (QFileInfo(candidate).exists()) {
            return candidate;
        }
    }

    return QString();
}

QString resolveTherionSourceReferencePath(const QString &currentFilePath,
                                          const QString &referencePath,
                                          const QSet<QString> &knownFilePaths)
{
    for (const QString &candidate : therionSourceReferencePathCandidates(currentFilePath, referencePath)) {
        if (knownFilePaths.contains(candidate) || QFileInfo(candidate).exists()) {
            return candidate;
        }
    }

    return QString();
}
}
