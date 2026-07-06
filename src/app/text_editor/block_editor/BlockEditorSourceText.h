#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct TherionParsedLine;
class TherionSourceDocument;
class TherionSourceLogicalDocument;
struct TherionSourceTextEdit;

struct BlockEditorLogicalLine
{
    int startLine = 0;
    int endLine = 0;
    QString text;
};

QString blockEditorSourceLineEnding(const QString &contents);
QStringList blockEditorNormalizedSourceLines(const QString &contents);
QString blockEditorJoinSourceLines(const QString &originalContents, const QStringList &lines);
QVector<BlockEditorLogicalLine> blockEditorBuildLogicalLines(const QStringList &lines);
bool blockEditorResolveLogicalLineAtLine(const QStringList &lines,
                                         int lineNumber,
                                         BlockEditorLogicalLine *logicalLine);
TherionParsedLine blockEditorParsedLineForLogicalLine(const BlockEditorLogicalLine &logicalLine,
                                                      const TherionSourceDocument *sourceDocument,
                                                      const TherionSourceLogicalDocument *logicalDocument);
bool blockEditorSourceLineRangeReplacementEdit(const QString &contents,
                                               int startLine,
                                               int endLine,
                                               const QStringList &replacementLines,
                                               TherionSourceTextEdit *edit);
bool blockEditorSourceReplacementEdit(const QString &contents,
                                      const QString &updatedContents,
                                      TherionSourceTextEdit *edit);
}
