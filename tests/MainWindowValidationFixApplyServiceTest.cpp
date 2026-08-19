#include "../src/app/MainWindowValidationFixApplyService.h"

#include <QCoreApplication>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

QVector<TherionSourceDiagnosticFix> sampleFixes()
{
    TherionSourceDiagnosticFix fix;
    fix.startOffset = 0;
    fix.length = 3;
    fix.replacementText = QStringLiteral("survey");
    fix.description = QStringLiteral("Sample fix");
    fix.expectedSourceDigest = QByteArrayLiteral("sample-source-digest");
    return {fix};
}

int runRejectsEmptyFixesTest()
{
    MainWindowValidationFixApplyContext context;
    context.applyFixesToTextPath = [](const QString &, const QVector<TherionSourceDiagnosticFix> &) {
        return true;
    };

    if (!expect(!MainWindowValidationFixApplyService::applyValidationFixes(QStringLiteral("/tmp/example.th"),
                                                                            QString(),
                                                                            {},
                                                                            context),
                "Validation-fix apply service should reject empty fix sets.")) {
        return 1;
    }

    return 0;
}

int runRejectsFixWithoutSourceSnapshotTest()
{
    int textCalls = 0;
    MainWindowValidationFixApplyContext context;
    context.applyFixesToTextPath = [&textCalls](const QString &, const QVector<TherionSourceDiagnosticFix> &) {
        ++textCalls;
        return true;
    };

    TherionSourceDiagnosticFix fix;
    fix.startOffset = 0;
    fix.length = 1;
    fix.replacementText = QStringLiteral("x");
    if (!expect(!MainWindowValidationFixApplyService::applyValidationFixes(QStringLiteral("/tmp/example.th"),
                                                                            QString(),
                                                                            {fix},
                                                                            context),
                "Validation-fix apply service should reject a fix without a source snapshot.")) {
        return 1;
    }
    if (!expect(textCalls == 0,
                "Validation-fix apply service should not route an unprotected fix to an editor.")) {
        return 1;
    }

    return 0;
}

int runMapPathRoutingTest()
{
    const QVector<TherionSourceDiagnosticFix> fixes = sampleFixes();
    QString appliedPath;
    int mapCalls = 0;
    int textCalls = 0;

    MainWindowValidationFixApplyContext context;
    context.applyFixesToMapPath = [&appliedPath, &mapCalls](const QString &path,
                                                            const QVector<TherionSourceDiagnosticFix> &receivedFixes) {
        ++mapCalls;
        appliedPath = path;
        return receivedFixes.size() == 1;
    };
    context.applyFixesToTextPath = [&textCalls](const QString &, const QVector<TherionSourceDiagnosticFix> &) {
        ++textCalls;
        return false;
    };

    const bool changed = MainWindowValidationFixApplyService::applyValidationFixes(QStringLiteral("/tmp/plan.th2"),
                                                                                    QString(),
                                                                                    fixes,
                                                                                    context);
    if (!expect(changed,
                "Validation-fix apply service should route TH2 paths through map callback.")) {
        return 1;
    }
    if (!expect(mapCalls == 1 && textCalls == 0,
                "Validation-fix apply service map-path routing changed unexpectedly.")) {
        return 1;
    }
    if (!expect(appliedPath == QStringLiteral("/tmp/plan.th2"),
                "Validation-fix apply service should pass the target map path through unchanged.")) {
        return 1;
    }

    return 0;
}

int runValidationDocumentFallbackPathTest()
{
    const QVector<TherionSourceDiagnosticFix> fixes = sampleFixes();
    QString appliedPath;

    MainWindowValidationFixApplyContext context;
    context.applyFixesToTextPath = [&appliedPath](const QString &path,
                                                  const QVector<TherionSourceDiagnosticFix> &receivedFixes) {
        appliedPath = path;
        return !receivedFixes.isEmpty();
    };

    const bool changed = MainWindowValidationFixApplyService::applyValidationFixes(QString(),
                                                                                    QStringLiteral("/tmp/fallback.th"),
                                                                                    fixes,
                                                                                    context);
    if (!expect(changed,
                "Validation-fix apply service should use validation-document path fallback when file path is empty.")) {
        return 1;
    }
    if (!expect(appliedPath == QStringLiteral("/tmp/fallback.th"),
                "Validation-fix apply service should route fallback path to text callback.")) {
        return 1;
    }

    return 0;
}

int runCurrentDocumentFallbackOrderTest()
{
    const QVector<TherionSourceDiagnosticFix> fixes = sampleFixes();
    int currentMapCalls = 0;
    int currentTextCalls = 0;

    MainWindowValidationFixApplyContext context;
    context.applyFixesToCurrentMap = [&currentMapCalls](const QVector<TherionSourceDiagnosticFix> &receivedFixes) {
        ++currentMapCalls;
        return !receivedFixes.isEmpty();
    };
    context.applyFixesToCurrentText = [&currentTextCalls](const QVector<TherionSourceDiagnosticFix> &) {
        ++currentTextCalls;
        return true;
    };

    const bool changed = MainWindowValidationFixApplyService::applyValidationFixes(QString(),
                                                                                    QString(),
                                                                                    fixes,
                                                                                    context);
    if (!expect(changed,
                "Validation-fix apply service should apply fixes on current map tab when no path is provided.")) {
        return 1;
    }
    if (!expect(currentMapCalls == 1 && currentTextCalls == 0,
                "Validation-fix apply service should not call current text callback when map callback succeeds.")) {
        return 1;
    }

    return 0;
}

int runCurrentTextFallbackTest()
{
    const QVector<TherionSourceDiagnosticFix> fixes = sampleFixes();
    int currentMapCalls = 0;
    int currentTextCalls = 0;

    MainWindowValidationFixApplyContext context;
    context.applyFixesToCurrentMap = [&currentMapCalls](const QVector<TherionSourceDiagnosticFix> &) {
        ++currentMapCalls;
        return false;
    };
    context.applyFixesToCurrentText = [&currentTextCalls](const QVector<TherionSourceDiagnosticFix> &receivedFixes) {
        ++currentTextCalls;
        return !receivedFixes.isEmpty();
    };

    const bool changed = MainWindowValidationFixApplyService::applyValidationFixes(QString(),
                                                                                    QString(),
                                                                                    fixes,
                                                                                    context);
    if (!expect(changed,
                "Validation-fix apply service should fall back to current text callback when current map callback fails.")) {
        return 1;
    }
    if (!expect(currentMapCalls == 1 && currentTextCalls == 1,
                "Validation-fix apply service fallback callback ordering changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runRejectsEmptyFixesTest() != 0) {
        return 1;
    }
    if (runRejectsFixWithoutSourceSnapshotTest() != 0) {
        return 1;
    }
    if (runMapPathRoutingTest() != 0) {
        return 1;
    }
    if (runValidationDocumentFallbackPathTest() != 0) {
        return 1;
    }
    if (runCurrentDocumentFallbackOrderTest() != 0) {
        return 1;
    }
    if (runCurrentTextFallbackTest() != 0) {
        return 1;
    }

    return 0;
}
