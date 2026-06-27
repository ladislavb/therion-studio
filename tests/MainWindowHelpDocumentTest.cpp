#include "../src/app/MainWindowHelpDocument.h"

#include <QtTest/QtTest>

namespace
{
class MainWindowHelpDocumentTest final : public QObject
{
    Q_OBJECT

private slots:
    void parsesMarkdownSectionsAndAnchors();
};
}

void MainWindowHelpDocumentTest::parsesMarkdownSectionsAndAnchors()
{
    const QString markdown = QStringLiteral(
        "# Therion Studio User Manual\n"
        "\n"
        "## Contents\n"
        "\n"
        "## Visual Map Editing (`.th2`)\n"
        "\n"
        "### Navigation\n"
        "\n"
        "```text\n"
        "## Ignored Code Heading\n"
        "```\n"
        "\n"
        "## Visual Map Editing (`.th2`)\n");

    const QVector<TherionStudio::MainWindowHelpSection> sections =
        TherionStudio::parseMarkdownHelpSections(markdown);

    QCOMPARE(sections.size(), 5);
    QCOMPARE(sections.at(0).level, 1);
    QCOMPARE(sections.at(0).title, QStringLiteral("Therion Studio User Manual"));
    QCOMPARE(sections.at(0).anchor, QStringLiteral("therion-studio-user-manual"));
    QCOMPARE(sections.at(2).title, QStringLiteral("Visual Map Editing (.th2)"));
    QCOMPARE(sections.at(2).anchor, QStringLiteral("visual-map-editing-th2"));
    QCOMPARE(sections.at(3).level, 3);
    QCOMPARE(sections.at(3).anchor, QStringLiteral("navigation"));
    QCOMPARE(sections.at(4).anchor, QStringLiteral("visual-map-editing-th2-2"));

    const QString html = TherionStudio::markdownToHtmlWithHeadingAnchors(markdown);
    QVERIFY(html.contains(QStringLiteral("id=\"visual-map-editing-th2\"")));
    QVERIFY(html.contains(QStringLiteral("id=\"visual-map-editing-th2-2\"")));
    QVERIFY(!html.contains(QStringLiteral("ignored-code-heading")));
}

int runMainWindowHelpDocumentTest(int argc, char **argv)
{
    MainWindowHelpDocumentTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowHelpDocumentTest.moc"
