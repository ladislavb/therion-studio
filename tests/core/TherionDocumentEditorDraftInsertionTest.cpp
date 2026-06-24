#include "../../src/core/TherionDocumentEditor.h"

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionDocumentEditorDraftInsertionTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsPointInsertionWhenScrapIsUnclosed();
    void rejectsLineInsertionWhenScrapIsUnclosed();
    void rejectsAreaInsertionWhenScrapIsUnclosed();
};

void TherionDocumentEditorDraftInsertionTest::rejectsPointInsertionWhenScrapIsUnclosed()
{
    const QString source = QStringLiteral("scrap broken -projection plan\n");
    QVector<TherionSourceTextEdit> edits;
    int insertedLineNumber = 0;
    QString errorMessage;

    const bool ok = TherionDocumentEditor::appendDraftGeometryEdits(source,
                                                                    QStringLiteral("point"),
                                                                    QVector<QPointF>{QPointF(10.0, 20.0)},
                                                                    &edits,
                                                                    &insertedLineNumber,
                                                                    &errorMessage);

    QVERIFY(!ok);
    QVERIFY(edits.isEmpty());
    QCOMPARE(insertedLineNumber, 0);
    QVERIFY(errorMessage.contains(QStringLiteral("endscrap")));
    QVERIFY(errorMessage.contains(QStringLiteral("line 1")));
}

void TherionDocumentEditorDraftInsertionTest::rejectsLineInsertionWhenScrapIsUnclosed()
{
    const QString source = QStringLiteral(
        "scrap closed\n"
        "endscrap\n"
        "scrap broken -projection plan\n");
    QVector<TherionSourceTextEdit> edits;
    int insertedLineNumber = 0;
    QString errorMessage;

    const bool ok = TherionDocumentEditor::appendDraftLineGeometryEdits(source,
                                                                        QStringList{
                                                                            QStringLiteral("10 20"),
                                                                            QStringLiteral("30 40"),
                                                                        },
                                                                        &edits,
                                                                        &insertedLineNumber,
                                                                        &errorMessage);

    QVERIFY(!ok);
    QVERIFY(edits.isEmpty());
    QCOMPARE(insertedLineNumber, 0);
    QVERIFY(errorMessage.contains(QStringLiteral("endscrap")));
    QVERIFY(errorMessage.contains(QStringLiteral("line 3")));
}

void TherionDocumentEditorDraftInsertionTest::rejectsAreaInsertionWhenScrapIsUnclosed()
{
    const QString source = QStringLiteral("scrap broken -projection plan\n");
    QVector<TherionSourceTextEdit> edits;
    int insertedLineNumber = 0;
    QString errorMessage;

    const bool ok = TherionDocumentEditor::appendDraftAreaGeometryEdits(source,
                                                                        QStringList{
                                                                            QStringLiteral("10 20"),
                                                                            QStringLiteral("30 40"),
                                                                            QStringLiteral("50 60"),
                                                                        },
                                                                        &edits,
                                                                        &insertedLineNumber,
                                                                        &errorMessage);

    QVERIFY(!ok);
    QVERIFY(edits.isEmpty());
    QCOMPARE(insertedLineNumber, 0);
    QVERIFY(errorMessage.contains(QStringLiteral("endscrap")));
    QVERIFY(errorMessage.contains(QStringLiteral("line 1")));
}
}

int runTherionDocumentEditorDraftInsertionTest(int argc, char **argv)
{
    TherionDocumentEditorDraftInsertionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionDocumentEditorDraftInsertionTest.moc"
