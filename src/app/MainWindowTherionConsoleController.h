#pragma once

#include <QClipboard>
#include <QColor>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QString>

#include "TherionRunnerOutputLinker.h"

namespace TherionStudio
{
class MainWindowTherionConsoleController final
{
public:
    enum class CopyOutputResult
    {
        Copied,
        Empty,
        ClipboardUnavailable,
        ConsoleUnavailable
    };

    void bindWidgets(QTextBrowser *consoleView,
                     QPushButton *runButton,
                     QPushButton *stopButton,
                     QLineEdit *workingDirectoryEdit,
                     QLineEdit *argumentsEdit)
    {
        consoleView_ = consoleView;
        runButton_ = runButton;
        stopButton_ = stopButton;
        workingDirectoryEdit_ = workingDirectoryEdit;
        argumentsEdit_ = argumentsEdit;
    }

    void appendConsoleLine(const QString &line) const
    {
        if (consoleView_ == nullptr) {
            return;
        }
        activeDiagnosticSeverity_ = DiagnosticSeverity::None;
        appendText(line + QLatin1Char('\n'), QString());
    }

    void appendProcessStandardOutput(const QString &output, const QString &workingDirectory = QString()) const
    {
        if (consoleView_ == nullptr || output.isEmpty()) {
            return;
        }
        appendDiagnosticText(output, workingDirectory);
    }

    void appendProcessStandardError(const QString &output,
                                    const QString &prefix,
                                    const QString &workingDirectory) const
    {
        if (consoleView_ == nullptr || output.isEmpty()) {
            return;
        }
        appendDiagnosticText(prefix.arg(output), workingDirectory);
    }

    CopyOutputResult copyConsoleOutputToClipboard() const
    {
        if (consoleView_ == nullptr) {
            return CopyOutputResult::ConsoleUnavailable;
        }

        const QString consoleOutput = consoleView_->toPlainText();
        if (consoleOutput.trimmed().isEmpty()) {
            return CopyOutputResult::Empty;
        }

        QClipboard *clipboard = QGuiApplication::clipboard();
        if (clipboard == nullptr) {
            return CopyOutputResult::ClipboardUnavailable;
        }

        clipboard->setText(consoleOutput);
        return CopyOutputResult::Copied;
    }

    bool clearConsoleOutput() const
    {
        if (consoleView_ == nullptr) {
            return false;
        }

        consoleView_->clear();
        return true;
    }

    void applyRunnerState(bool isRunning) const
    {
        if (runButton_ != nullptr) {
            runButton_->setEnabled(!isRunning);
        }
        if (stopButton_ != nullptr) {
            stopButton_->setEnabled(isRunning);
        }
        if (workingDirectoryEdit_ != nullptr) {
            workingDirectoryEdit_->setEnabled(!isRunning);
        }
        if (argumentsEdit_ != nullptr) {
            argumentsEdit_->setEnabled(!isRunning);
        }
    }

private:
    enum class DiagnosticSeverity
    {
        None,
        Warning,
        Error
    };

    DiagnosticSeverity diagnosticSeverityForLine(const QString &line) const
    {
        if (TherionRunnerOutputLinker::containsCompilerError(line)) {
            return DiagnosticSeverity::Error;
        }
        if (TherionRunnerOutputLinker::containsCompilerWarning(line)) {
            return DiagnosticSeverity::Warning;
        }
        return DiagnosticSeverity::None;
    }

    bool lineResetsDiagnosticSeverity(const QString &line) const
    {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()
            || trimmed == QStringLiteral(".")
            || trimmed == QStringLiteral("done")) {
            return true;
        }

        const QString lowered = trimmed.toLower();
        return lowered.startsWith(QStringLiteral("writing "))
            || lowered.startsWith(QStringLiteral("reading "))
            || lowered.startsWith(QStringLiteral("preprocessing "))
            || lowered.startsWith(QStringLiteral("configuration file:"))
            || lowered.startsWith(QStringLiteral("this is "))
            || lowered.startsWith(QStringLiteral("entering "));
    }

    QTextCharFormat diagnosticFormatForSeverity(DiagnosticSeverity severity) const
    {
        QTextCharFormat format;
        if (severity == DiagnosticSeverity::Error) {
            format.setForeground(QColor(QStringLiteral("#c62828")));
        } else if (severity == DiagnosticSeverity::Warning) {
            format.setForeground(QColor(QStringLiteral("#b26a00")));
        }
        return format;
    }

    void appendDiagnosticText(const QString &text, const QString &workingDirectory) const
    {
        qsizetype lineStart = 0;
        while (lineStart < text.size()) {
            const qsizetype lineEnd = text.indexOf(QLatin1Char('\n'), lineStart);
            const bool hasLineBreak = lineEnd >= 0;
            const QString line = hasLineBreak
                ? text.mid(lineStart, lineEnd - lineStart)
                : text.mid(lineStart);
            const DiagnosticSeverity detectedSeverity = diagnosticSeverityForLine(line);
            if (detectedSeverity != DiagnosticSeverity::None) {
                activeDiagnosticSeverity_ = detectedSeverity;
            } else if (lineResetsDiagnosticSeverity(line)) {
                activeDiagnosticSeverity_ = DiagnosticSeverity::None;
            }

            appendText(line + (hasLineBreak ? QStringLiteral("\n") : QString()),
                       workingDirectory,
                       diagnosticFormatForSeverity(activeDiagnosticSeverity_));
            if (!hasLineBreak) {
                break;
            }
            lineStart = lineEnd + 1;
        }
    }

    void appendText(const QString &text,
                    const QString &workingDirectory,
                    const QTextCharFormat &baseFormat = QTextCharFormat()) const
    {
        if (consoleView_ == nullptr || text.isEmpty()) {
            return;
        }

        const QVector<TherionRunnerOutputLinker::Link> links =
            TherionRunnerOutputLinker::sourceLinksForText(text, workingDirectory);
        QTextCursor cursor = consoleView_->textCursor();
        cursor.movePosition(QTextCursor::End);
        if (!consoleView_->document()->isEmpty()
            && (!cursor.atBlockStart() || !cursor.block().text().isEmpty())) {
            cursor.insertBlock();
        }

        const QTextCharFormat plainFormat = baseFormat;
        QTextCharFormat linkFormat = baseFormat;
        linkFormat.setAnchor(true);
        if (linkFormat.foreground().style() == Qt::NoBrush) {
            linkFormat.setForeground(consoleView_->palette().link());
        }
        linkFormat.setFontUnderline(true);

        int position = 0;
        for (const TherionRunnerOutputLinker::Link &link : links) {
            if (!link.isValid() || link.start < position || link.start >= text.size()) {
                continue;
            }
            if (link.start > position) {
                cursor.insertText(text.mid(position, link.start - position), plainFormat);
            }
            QTextCharFormat anchoredFormat = linkFormat;
            anchoredFormat.setAnchorHref(TherionRunnerOutputLinker::urlForSourceLocation(link.location).toString());
            cursor.insertText(text.mid(link.start, link.length), anchoredFormat);
            position = link.start + link.length;
        }
        if (position < text.size()) {
            cursor.insertText(text.mid(position), plainFormat);
        }

        consoleView_->setTextCursor(cursor);
        consoleView_->ensureCursorVisible();
    }

    QTextBrowser *consoleView_ = nullptr;
    QPushButton *runButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QLineEdit *workingDirectoryEdit_ = nullptr;
    QLineEdit *argumentsEdit_ = nullptr;
    mutable DiagnosticSeverity activeDiagnosticSeverity_ = DiagnosticSeverity::None;
};
}
