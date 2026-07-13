#include "TherionSourceFormatter.h"

#include "TherionSourceDocument.h"

#include <QtGlobal>

namespace TherionStudio
{
namespace
{
bool isCodeBlockBody(const TherionSourceDocumentLine &line)
{
    return line.currentBlockDirective == QStringLiteral("code") && !line.closesBlock;
}

int formattedIndentationDepth(const TherionSourceDocumentLine &line)
{
    int depth = line.blockStackBefore.size();
    if (line.closesBlock && !line.hasUnmatchedClose()) {
        --depth;
    }
    return qMax(0, depth);
}

QString withoutLeadingIndentation(const QString &line)
{
    int firstContentIndex = 0;
    while (firstContentIndex < line.size()) {
        const QChar character = line.at(firstContentIndex);
        if (character != QLatin1Char(' ') && character != QLatin1Char('\t')) {
            break;
        }
        ++firstContentIndex;
    }
    return line.mid(firstContentIndex);
}
}

QString TherionSourceFormatter::formatIndentation(const QString &contents)
{
    const TherionSourceDocument document = TherionSourceDocument::fromText(contents);
    QString formatted;
    formatted.reserve(contents.size());

    for (const TherionSourceDocumentLine &line : document.lines()) {
        const QString &sourceText = line.sourceLine.text;
        if (line.role == TherionSourceLineRole::Empty || isCodeBlockBody(line)) {
            formatted += sourceText;
        } else {
            formatted += QString(formattedIndentationDepth(line), QLatin1Char('\t'));
            formatted += withoutLeadingIndentation(sourceText);
        }
        formatted += line.sourceLine.lineEnding;
    }

    return formatted;
}
}
