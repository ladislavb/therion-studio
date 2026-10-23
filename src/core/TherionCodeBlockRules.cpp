#include "TherionCodeBlockRules.h"

#include "TherionSourceDocument.h"

#include <QRegularExpression>

namespace TherionStudio
{
namespace
{
TherionCodeLanguage codeLanguageFromOpeningLine(const TherionSourceDocumentLine *openingLine)
{
    if (openingLine == nullptr) {
        return TherionCodeLanguage::Unknown;
    }
    const QString type = openingLine->sourceLine.parsed.tokens.value(1);
    if (type == QStringLiteral("metapost") || type == QStringLiteral("mpost")) {
        return TherionCodeLanguage::MetaPost;
    }
    if (type == QStringLiteral("tex-map") || type == QStringLiteral("tex-atlas")) {
        return TherionCodeLanguage::TeX;
    }
    return TherionCodeLanguage::Unknown;
}

// TeX: `\` escapes the next character, so `\%` is a percent sign and `\\%` a line break then a comment.
int texCommentStart(const QString &text)
{
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) == QLatin1Char('\\')) {
            ++index;
        } else if (text.at(index) == QLatin1Char('%')) {
            return index;
        }
    }
    return -1;
}

// MetaPost: a `%` inside a "..." string is text; a string ends at the next quote or the end of the line.
int metaPostCommentStart(const QString &text)
{
    bool insideString = false;
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) == QLatin1Char('"')) {
            insideString = !insideString;
        } else if (!insideString && text.at(index) == QLatin1Char('%')) {
            return index;
        }
    }
    return -1;
}
}

QVector<TherionCodeLanguage> therionCodeLanguagesByLine(const TherionSourceDocument &document)
{
    QVector<TherionCodeLanguage> languages;
    languages.reserve(document.lines().size());
    for (const TherionSourceDocumentLine &line : document.lines()) {
        languages.append(line.isRawCodeContent() && !line.blockStackBefore.isEmpty()
                             ? codeLanguageFromOpeningLine(
                                   document.lineAtLineNumber(line.blockStackBefore.constLast().lineNumber))
                             : TherionCodeLanguage::None);
    }
    return languages;
}

int therionCodeCommentStart(const QString &text, TherionCodeLanguage language)
{
    if (language == TherionCodeLanguage::None) {
        return -1;
    }
    const int firstNonBlank = text.indexOf(QRegularExpression(QStringLiteral(R"(\S)")));
    if (firstNonBlank >= 0 && text.at(firstNonBlank) == QLatin1Char('#')) {
        return firstNonBlank;
    }

    switch (language) {
    case TherionCodeLanguage::TeX:
        return texCommentStart(text);
    case TherionCodeLanguage::MetaPost:
        return metaPostCommentStart(text);
    case TherionCodeLanguage::None:
    case TherionCodeLanguage::Unknown:
        break;
    }
    return -1;
}
}
