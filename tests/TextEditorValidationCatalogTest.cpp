#include "../src/app/text_editor/TextEditorValidationCatalog.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/TherionSourceValidator.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
QStringList diagnosticCodes(const TherionSourceValidationResult &result)
{
    QStringList codes;
    for (const TherionSourceDiagnostic &diagnostic : result.diagnostics) {
        codes.append(diagnostic.code);
    }
    return codes;
}
}

class TextEditorValidationCatalogTest final : public QObject
{
    Q_OBJECT

private slots:
    void validatesStatisticsArgumentsFromApplicationCatalog_data();
    void validatesStatisticsArgumentsFromApplicationCatalog();
};

void TextEditorValidationCatalogTest::validatesStatisticsArgumentsFromApplicationCatalog_data()
{
    QTest::addColumn<QString>("statisticsCommand");
    QTest::addColumn<bool>("expectsUnknownItemDiagnostic");

    QTest::newRow("explo-length-off")
        << QStringLiteral("statistics explo-length off") << false;
    QTest::newRow("topo-length-hide")
        << QStringLiteral("statistics topo-length hide") << false;
    QTest::newRow("copyright-all")
        << QStringLiteral("statistics copyright all") << false;
    QTest::newRow("copyright-number")
        << QStringLiteral("statistics copyright 2") << false;
    QTest::newRow("unknown-item")
        << QStringLiteral("statistics tartampion off") << true;
    // The second argument is intentionally left unvalidated until its domain can
    // be modelled conditionally from the first argument.
    QTest::newRow("unvalidated-value-compromise")
        << QStringLiteral("statistics explo-length peut-etre") << false;
}

void TextEditorValidationCatalogTest::validatesStatisticsArgumentsFromApplicationCatalog()
{
    QFETCH(QString, statisticsCommand);
    QFETCH(bool, expectsUnknownItemDiagnostic);

    const CommandCatalogStore store;
    QVERIFY2(store.isCatalogAvailable(), "The application command catalog resource must be available.");
    const TherionSourceValidationCatalog catalog =
        validationCatalogFromCommandCatalog(store.catalogObject());

    const QString contents = QStringLiteral("layout test\n  %1\nendlayout\n").arg(statisticsCommand);
    TherionSourceDocumentMetadata metadata;
    metadata.sourceType = TherionSourceDocumentType::TherionConfig;
    const TherionSourceValidationResult result =
        TherionSourceValidator::validate(contents, catalog, metadata);
    const QStringList codes = diagnosticCodes(result);

    if (expectsUnknownItemDiagnostic) {
        QCOMPARE(codes, QStringList{QStringLiteral("unknown-argument-value")});
    } else {
        QVERIFY2(codes.isEmpty(), qPrintable(QStringLiteral("Unexpected diagnostics: %1").arg(codes.join(QStringLiteral(", ")))));
    }
}

int runTextEditorValidationCatalogTest(int argc, char **argv)
{
    TextEditorValidationCatalogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TextEditorValidationCatalogTest.moc"
