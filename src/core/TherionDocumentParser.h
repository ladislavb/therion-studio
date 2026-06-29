#pragma once

#include "TherionSourceText.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
enum class TherionTokenType
{
    Other,
    QuotedString,
    Comment
};

struct TherionParsedToken
{
    int start = 0;
    int length = 0;
    QString text;
    TherionTokenType type = TherionTokenType::Other;
};

struct TherionParsedLine
{
    int lineNumber = 0;
    QString rawText;
    QString directive;
    QStringList tokens;
    QVector<TherionParsedToken> tokenSpans;
    int commentStart = -1;
    QString commentText;
};

struct TherionParsedSourceLine
{
    int lineNumber = 0;
    int startOffset = 0;
    int textLength = 0;
    int lineEndingLength = 0;
    int endOffset = 0;
    QString text;
    QString lineEnding;
    TherionParsedLine parsed;

    [[nodiscard]] bool hasTokens() const;
    [[nodiscard]] bool isBlank() const;
    [[nodiscard]] bool isCommentOnly() const;
    [[nodiscard]] int absoluteTokenStart(const TherionParsedToken &token) const;
    [[nodiscard]] int absoluteTokenEnd(const TherionParsedToken &token) const;
};

struct TherionParsedSourceDocument
{
    QVector<TherionParsedSourceLine> lines;

    [[nodiscard]] QString toText() const;
    [[nodiscard]] QVector<TherionParsedLine> tokenLines() const;
};

class TherionDocumentParser final
{
public:
    [[nodiscard]] static TherionParsedLine parseLine(const QString &line, int lineNumber = 0);
    [[nodiscard]] static QVector<TherionParsedLine> parseText(const QString &text);
    [[nodiscard]] static QVector<TherionParsedLine> parseTokenLines(const QString &text);
    [[nodiscard]] static TherionParsedSourceDocument parseSourceDocument(const QString &text);
    [[nodiscard]] static TherionParsedSourceDocument parseSourceDocument(const TherionSourceText &sourceText);
    [[nodiscard]] static QStringList tokenizeLine(const QString &line);
};
}
