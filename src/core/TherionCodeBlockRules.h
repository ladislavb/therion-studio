#pragma once

#include <QString>
#include <QVector>

namespace TherionStudio
{
class TherionSourceDocument;

// The language of a layout `code <type>` block, from Therion's code types (thlayout.h).
enum class TherionCodeLanguage
{
    None,
    Unknown,
    TeX,
    MetaPost,
};

// The language of the code block each line is raw content of, indexed by line number - 1; None outside code.
[[nodiscard]] QVector<TherionCodeLanguage> therionCodeLanguagesByLine(const TherionSourceDocument &document);

// Where a comment starts in a raw code line of that language, or -1. A line starting with `#` is a Therion comment in
// any language, as Therion drops it before the code is written out (thinput.cxx).
[[nodiscard]] int therionCodeCommentStart(const QString &text, TherionCodeLanguage language);
}
