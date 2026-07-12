#include "../../src/core/TherionSourceFormatter.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionSourceFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesNestedTherionBlocksToTabs();
    void preservesEmptyRowsCodeBodyAndLineEndings();
    void isIdempotent();
};

void TherionSourceFormatterTest::normalizesNestedTherionBlocksToTabs()
{
    const QString source = QStringLiteral(
        "  scrap s1\n"
        " # wall outline\n"
        " line wall\n"
        "     366.5 -904.7\n"
        " subtype presumed\n"
        "endline\n"
        "  line chimney -close on\n"
        "   557.0 -470.7\n"
        " endline\n"
        " endscrap\n");

    const QString expected = QStringLiteral(
        "scrap s1\n"
        "\t# wall outline\n"
        "\tline wall\n"
        "\t\t366.5 -904.7\n"
        "\t\tsubtype presumed\n"
        "\tendline\n"
        "\tline chimney -close on\n"
        "\t\t557.0 -470.7\n"
        "\tendline\n"
        "endscrap\n");

    QCOMPARE(TherionSourceFormatter::formatIndentation(source), expected);
}

void TherionSourceFormatterTest::preservesEmptyRowsCodeBodyAndLineEndings()
{
    const QString source = QStringLiteral(
        " survey demo\r\n"
        "  code tex\r\n"
        "    \\therionRawCommand  value\r\n"
        "\t  literal code indentation\r\n"
        "  endcode\r\n"
        "   \r\n"
        " endsurvey");

    const QString expected = QStringLiteral(
        "survey demo\r\n"
        "\tcode tex\r\n"
        "    \\therionRawCommand  value\r\n"
        "\t  literal code indentation\r\n"
        "\tendcode\r\n"
        "   \r\n"
        "endsurvey");

    QCOMPARE(TherionSourceFormatter::formatIndentation(source), expected);
}

void TherionSourceFormatterTest::isIdempotent()
{
    const QString source = QStringLiteral(
        "scrap s1\n"
        "\tline wall\n"
        "\t\t0 0\n"
        "\tendline\n"
        "endscrap\n");

    QCOMPARE(TherionSourceFormatter::formatIndentation(source), source);
}
}

int runTherionSourceFormatterTest(int argc, char **argv)
{
    TherionSourceFormatterTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionSourceFormatterTest.moc"
