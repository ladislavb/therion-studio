#pragma once

#include <QUrl>
#include <QVector>
#include <QString>

namespace TherionStudio
{
class TherionRunnerOutputLinker final
{
public:
    struct SourceLocation
    {
        QString path;
        int lineNumber = 0;

        bool isValid() const { return !path.trimmed().isEmpty() && lineNumber > 0; }
    };

    struct Link
    {
        int start = 0;
        int length = 0;
        SourceLocation location;

        bool isValid() const { return start >= 0 && length > 0 && location.isValid(); }
    };

    static QVector<Link> sourceLinksForText(const QString &text, const QString &workingDirectory);
    static bool containsCompilerError(const QString &text);
    static bool containsCompilerWarning(const QString &text);
    static QString absolutePathForCompilerPath(const QString &compilerPath, const QString &workingDirectory);
    static QUrl urlForSourceLocation(const SourceLocation &location);
    static SourceLocation sourceLocationFromUrl(const QUrl &url);
};
}
