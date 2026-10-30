#include "../src/app/MainWindowTherionConsoleBuilder.h"
#include "../src/app/MainWindowTherionConsoleController.h"

#include <QElapsedTimer>
#include <QClipboard>
#include <QFile>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTest>

#include <memory>

using namespace TherionStudio;

class TherionConsoleOutputTest final : public QObject
{
    Q_OBJECT

private slots:
    void boundsLargeCompilerTranscript();
    void copiesCompleteTranscriptAfterPreviewEviction();
    void resetsBothPreviewAndTranscriptForNextCompile();
    void refusesPartialCopyAfterLogFailure();
};

void TherionConsoleOutputTest::copiesCompleteTranscriptAfterPreviewEviction()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("logs/last-compile.log"));
    TherionConsoleOutputStore store(path);
    const auto widgets = MainWindowTherionConsoleBuilder::build({});
    const std::unique_ptr<QWidget> root(widgets.rootWidget);
    MainWindowTherionConsoleController controller;
    controller.setOutputStore(&store);
    controller.bindWidgets(widgets.consoleView, nullptr, nullptr, nullptr, nullptr);
    QString output = QStringLiteral("FIRST diagnostic\n");
    for (int line = 0; line < 12000; ++line) {
        output += QStringLiteral("survey %1\n").arg(line);
    }
    output += QStringLiteral("LAST diagnostic\n");
    controller.appendProcessStandardOutput(output);
    QVERIFY(!widgets.consoleView->toPlainText().contains(QStringLiteral("FIRST diagnostic")));
    QCOMPARE(controller.copyConsoleOutputToClipboard(),
             MainWindowTherionConsoleController::CopyOutputResult::Copied);
    QCOMPARE(QGuiApplication::clipboard()->text(), output);
    QFile log(path);
    QVERIFY(log.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(log.readAll()), output);
}

void TherionConsoleOutputTest::boundsLargeCompilerTranscript()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TherionConsoleOutputStore store(directory.filePath(QStringLiteral("last-compile.log")));
    const auto widgets = MainWindowTherionConsoleBuilder::build({});
    const std::unique_ptr<QWidget> root(widgets.rootWidget);
    MainWindowTherionConsoleController controller;
    controller.setOutputStore(&store);
    controller.bindWidgets(widgets.consoleView, nullptr, nullptr, nullptr, nullptr);
    QString batch;
    for (int index = 0; index < 1000; ++index) {
        batch += QStringLiteral("reading survey %1 ").arg(index) + QString(120, QLatin1Char('x')) + QLatin1Char('\n');
    }
    QElapsedTimer timer;
    timer.start();
    for (int batchIndex = 0; batchIndex < 50; ++batchIndex) {
        controller.appendProcessStandardOutput(batch);
    }
    qInfo("compiler transcript: input_lines=50000 retained_blocks=%d retained_characters=%d elapsed_ms=%lld",
          widgets.consoleView->document()->blockCount(),
          widgets.consoleView->document()->characterCount(),
          timer.elapsed());
    QVERIFY(widgets.consoleView->document()->blockCount() <= 10000);
    QVERIFY(widgets.consoleView->toPlainText().contains(QStringLiteral("reading survey 999")));
    QCOMPARE(QFileInfo(store.filePath()).size(), batch.toUtf8().size() * 50);
}

void TherionConsoleOutputTest::resetsBothPreviewAndTranscriptForNextCompile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TherionConsoleOutputStore store(directory.filePath(QStringLiteral("last-compile.log")));
    QTextBrowser view;
    MainWindowTherionConsoleController controller;
    controller.bindWidgets(&view, nullptr, nullptr, nullptr, nullptr);
    controller.setOutputStore(&store);
    controller.appendProcessStandardOutput(QStringLiteral("old output\n"));
    QVERIFY(controller.clearConsoleOutput());
    controller.appendConsoleLine(QStringLiteral("new run"));
    controller.appendProcessStandardOutput(QString::fromUtf8("žltý kôň\n"));
    controller.appendProcessStandardError(QStringLiteral("failure\n"), QStringLiteral("[stderr] %1"), directory.path());
    const QString expected = QString::fromUtf8("new run\nžltý kôň\n[stderr] failure\n");
    QCOMPARE(controller.copyConsoleOutputToClipboard(),
             MainWindowTherionConsoleController::CopyOutputResult::Copied);
    QCOMPARE(QGuiApplication::clipboard()->text(), expected);
    QVERIFY(!view.toPlainText().contains(QStringLiteral("old output")));
}

void TherionConsoleOutputTest::refusesPartialCopyAfterLogFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    TherionConsoleOutputStore store(directory.path());
    QTextBrowser view;
    MainWindowTherionConsoleController controller;
    controller.bindWidgets(&view, nullptr, nullptr, nullptr, nullptr);
    int errors = 0;
    controller.setOutputStore(&store, [&errors](const QString &message) {
        QVERIFY(!message.isEmpty());
        ++errors;
    });
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Compiler log error .*")));
    controller.appendProcessStandardOutput(QStringLiteral("not saved\n"));
    QCOMPARE(errors, 1);
    QVERIFY(view.toPlainText().contains(QStringLiteral("not saved")));
    QGuiApplication::clipboard()->setText(QStringLiteral("unchanged"));
    QCOMPARE(controller.copyConsoleOutputToClipboard(),
             MainWindowTherionConsoleController::CopyOutputResult::OutputReadFailed);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("unchanged"));
    QVERIFY(!store.errorString().isEmpty());
}

QTEST_MAIN(TherionConsoleOutputTest)
#include "TherionConsoleOutputTest.moc"
