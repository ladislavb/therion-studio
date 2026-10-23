#include "../../src/core/ProjectJoinRules.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectJoinItem item(const QString &scrap, ProjectStructureEntryKind kind = ProjectStructureEntryKind::Line)
{
    ProjectJoinItem joinItem;
    joinItem.reference = scrap;
    joinItem.kind = kind;
    joinItem.scrapObjectId = scrap;
    joinItem.resolved = true;
    return joinItem;
}

ProjectJoinCommand scrapJoin(int line, const QString &first, const QString &second, int count = 0)
{
    return ProjectJoinCommand{QStringLiteral("maps.th"), line, ProjectJoinMode::Scraps, count,
                              {item(first, ProjectStructureEntryKind::Scrap), item(second, ProjectStructureEntryKind::Scrap)}};
}

ProjectJoinCommand lineJoin(int line, const QVector<ProjectJoinItem> &items)
{
    return ProjectJoinCommand{QStringLiteral("maps.th"), line, ProjectJoinMode::PointsAndLines, 0, items};
}
}

class ProjectJoinRulesTest final : public QObject
{
    Q_OBJECT

private slots:
    void acceptsValidJoins()
    {
        // One scrap join, one counted scrap join, two wall joins, and joins inside one scrap.
        const QVector<ProjectJoinCommand> joins = {
            scrapJoin(1, QStringLiteral("a"), QStringLiteral("b")),
            scrapJoin(2, QStringLiteral("c"), QStringLiteral("d"), 2),
            lineJoin(3, {item(QStringLiteral("e")), item(QStringLiteral("f"))}),
            lineJoin(4, {item(QStringLiteral("f")), item(QStringLiteral("e"))}),
            lineJoin(5, {item(QStringLiteral("g")), item(QStringLiteral("g"))}),
        };
        QVERIFY(projectJoinRuleViolations(joins).isEmpty());
    }

    void ignoresUnresolvedItems()
    {
        ProjectJoinItem unresolved = item(QStringLiteral("b"));
        unresolved.resolved = false;
        QVERIFY(projectJoinRuleViolations({lineJoin(1, {item(QStringLiteral("a")), unresolved})}).isEmpty());
    }

    void reportsEachBrokenRule()
    {
        const ProjectJoinCommand mixedScrap = scrapJoin(1, QStringLiteral("a"), QStringLiteral("b"));
        const ProjectJoinCommand mixedWall = lineJoin(2, {item(QStringLiteral("b")), item(QStringLiteral("a"))});
        // An automatic and a counted scrap join are both scrap joins.
        const ProjectJoinCommand repeatedFirst = scrapJoin(3, QStringLiteral("c"), QStringLiteral("d"));
        const ProjectJoinCommand repeatedSecond = scrapJoin(4, QStringLiteral("d"), QStringLiteral("c"), 2);
        const ProjectJoinCommand single = lineJoin(5, {item(QStringLiteral("e")), item(QStringLiteral("f"))});
        const QVector<ProjectJoinRuleViolation> violations =
            projectJoinRuleViolations({mixedScrap, mixedWall, repeatedFirst, repeatedSecond, single});

        QCOMPARE(violations.size(), 3);
        QCOMPARE(violations.at(0).kind, ProjectJoinRuleViolationKind::MixedJoinKinds);
        QCOMPARE(violations.at(0).firstScrapObjectId, QStringLiteral("a"));
        QCOMPARE(violations.at(0).secondScrapObjectId, QStringLiteral("b"));
        QCOMPARE(violations.at(0).joins.size(), 2);
        QCOMPARE(violations.at(1).kind, ProjectJoinRuleViolationKind::RepeatedScrapJoin);
        QCOMPARE(violations.at(1).joins.size(), 2);
        QCOMPARE(violations.at(2).kind, ProjectJoinRuleViolationKind::SinglePointLineJoin);
        QCOMPARE(violations.at(2).joins.first().lineNumber, 5);

        QCOMPARE(projectJoinRuleViolationsFor(violations, {single}).size(), 1);
        QCOMPARE(projectJoinRuleViolationsFor(violations, {mixedWall, single}).size(), 2);
        QVERIFY(projectJoinRuleViolationsFor(violations, {scrapJoin(9, QStringLiteral("x"), QStringLiteral("y"))})
                    .isEmpty());
    }

    void countsOneCommandForEveryScrapPairItJoins()
    {
        // A three-way junction in one command joins three scrap pairs once each.
        const ProjectJoinCommand junction =
            lineJoin(1, {item(QStringLiteral("a")), item(QStringLiteral("b")), item(QStringLiteral("c"))});
        const ProjectJoinCommand secondWall = lineJoin(2, {item(QStringLiteral("a")), item(QStringLiteral("b"))});
        const QVector<ProjectJoinRuleViolation> violations = projectJoinRuleViolations({junction, secondWall});
        QCOMPARE(violations.size(), 2);
        QCOMPARE(violations.at(0).secondScrapObjectId, QStringLiteral("c"));
        QCOMPARE(violations.at(1).firstScrapObjectId, QStringLiteral("b"));
    }
};

int runProjectJoinRulesTest(int argc, char **argv)
{
    ProjectJoinRulesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectJoinRulesTest.moc"
