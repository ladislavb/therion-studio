#include "../src/app/text_editor/TextEditorOptionValidation.h"

#include <QtTest/QtTest>

namespace
{
class TextEditorOptionValidationTest final : public QObject
{
    Q_OBJECT

private slots:
    void deduplicatesIdenticalSerializedOptions();
};
}

void TextEditorOptionValidationTest::deduplicatesIdenticalSerializedOptions()
{
    const TherionStudio::TextEditorOptionValidationResult result =
        TherionStudio::validateAndSerializeCommandOptions(
            QStringLiteral("line"),
            {TherionStudio::TextEditorOptionRow{QStringLiteral("-clip"), QStringLiteral("off"), 1},
             TherionStudio::TextEditorOptionRow{QStringLiteral("-clip"), QStringLiteral("off"), 2},
             TherionStudio::TextEditorOptionRow{QStringLiteral("-close"), QStringLiteral("on"), 3}},
            {},
            {},
            {},
            {},
            false);

    QVERIFY(result.ok);
    QCOMPARE(result.serializedOptions,
             QStringList({QStringLiteral("-clip"),
                          QStringLiteral("off"),
                          QStringLiteral("-close"),
                          QStringLiteral("on")}));
}

int runTextEditorOptionValidationTest(int argc, char **argv)
{
    TextEditorOptionValidationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TextEditorOptionValidationTest.moc"
