#include "TherionRunnerOutputLinker.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrlQuery>

namespace TherionStudio
{
namespace
{
constexpr auto kTherionRunnerOutputLinkScheme = "therion-log";
constexpr auto kTherionRunnerOutputLinkHost = "open";

QString canonicalOrAbsoluteFilePath(const QFileInfo &fileInfo)
{
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}
}

QVector<TherionRunnerOutputLinker::Link>
TherionRunnerOutputLinker::sourceLinksForText(const QString &text, const QString &workingDirectory)
{
    QVector<Link> links;
    if (text.isEmpty()) {
        return links;
    }

    static const QRegularExpression sourceLocationPattern(
        QStringLiteral(R"((^|\s)([^\s\[\]]+\.(?:th2|th|thconfig))\s*\[(\d+)\])"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator it = sourceLocationPattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString compilerPath = match.captured(2);
        const int lineNumber = match.captured(3).toInt();
        if (compilerPath.trimmed().isEmpty() || lineNumber <= 0) {
            continue;
        }

        Link link;
        link.start = match.capturedStart(2);
        link.length = match.capturedLength(2);
        link.location.path = absolutePathForCompilerPath(compilerPath, workingDirectory);
        link.location.lineNumber = lineNumber;
        if (link.isValid()) {
            links.append(link);
        }
    }

    return links;
}

bool TherionRunnerOutputLinker::containsCompilerError(const QString &text)
{
    static const QRegularExpression compilerErrorPattern(
        QStringLiteral(R"(\berror\s+--)"),
        QRegularExpression::CaseInsensitiveOption);
    return compilerErrorPattern.match(text).hasMatch();
}

bool TherionRunnerOutputLinker::containsCompilerWarning(const QString &text)
{
    static const QRegularExpression compilerWarningPattern(
        QStringLiteral(R"((\bwarning\s+--|\[warning:))"),
        QRegularExpression::CaseInsensitiveOption);
    return compilerWarningPattern.match(text).hasMatch();
}

QString TherionRunnerOutputLinker::absolutePathForCompilerPath(const QString &compilerPath,
                                                               const QString &workingDirectory)
{
    const QString trimmedPath = compilerPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(trimmedPath);
    if (fileInfo.isRelative() && !workingDirectory.trimmed().isEmpty()) {
        fileInfo = QFileInfo(QDir(workingDirectory).filePath(trimmedPath));
    }
    return canonicalOrAbsoluteFilePath(fileInfo);
}

QUrl TherionRunnerOutputLinker::urlForSourceLocation(const SourceLocation &location)
{
    if (!location.isValid()) {
        return QUrl();
    }

    QUrl url;
    url.setScheme(QString::fromLatin1(kTherionRunnerOutputLinkScheme));
    url.setHost(QString::fromLatin1(kTherionRunnerOutputLinkHost));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("path"), location.path);
    query.addQueryItem(QStringLiteral("line"), QString::number(location.lineNumber));
    url.setQuery(query);
    return url;
}

TherionRunnerOutputLinker::SourceLocation
TherionRunnerOutputLinker::sourceLocationFromUrl(const QUrl &url)
{
    if (url.scheme() != QString::fromLatin1(kTherionRunnerOutputLinkScheme)
        || url.host() != QString::fromLatin1(kTherionRunnerOutputLinkHost)) {
        return {};
    }

    const QUrlQuery query(url);
    bool ok = false;
    const int lineNumber = query.queryItemValue(QStringLiteral("line")).toInt(&ok);
    if (!ok || lineNumber <= 0) {
        return {};
    }

    SourceLocation location;
    location.path = query.queryItemValue(QStringLiteral("path"));
    location.lineNumber = lineNumber;
    return location.isValid() ? location : SourceLocation{};
}
}
