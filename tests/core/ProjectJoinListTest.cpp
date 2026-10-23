#include "../../src/core/ProjectJoinList.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectEquateStation station(const QString &reference, const QString &namespacePath, bool resolved = true)
{
    return ProjectEquateStation{reference, reference.toLower() + QLatin1Char('|') + namespacePath, namespacePath, resolved};
}

ProjectStationDrawingPoint drawing(const ProjectEquateStation &station, const QString &file, const QString &projection)
{
    return ProjectStationDrawingPoint{station.stationKey, file, 7, QStringLiteral("s"), QStringLiteral("survey"), projection};
}
}

class ProjectJoinListTest final : public QObject
{
    Q_OBJECT

private slots:
    void ordersProjections()
    {
        ProjectIndexSnapshot snapshot;
        const ProjectEquateStation a = station(QStringLiteral("1@a"), QStringLiteral("a"));
        for (const QString &projection : {QStringLiteral("elevation 315"), QStringLiteral("none-like"),
                                          QStringLiteral("elevation 90"), QStringLiteral("extended"),
                                          QStringLiteral("plan"), QStringLiteral("elevation")}) {
            snapshot.stationDrawingPointsByStationKey[a.stationKey].append(drawing(a, QStringLiteral("a.th2"), projection));
        }
        QCOMPARE(projectJoinProjections(snapshot),
                 QStringList({QStringLiteral("plan"), QStringLiteral("extended"), QStringLiteral("elevation"),
                              QStringLiteral("elevation 90"), QStringLiteral("elevation 315"),
                              QStringLiteral("none-like")}));
    }

    void buildsPairsWithSideStatus()
    {
        const ProjectEquateStation a = station(QStringLiteral("1@a"), QStringLiteral("a"));
        const ProjectEquateStation b = station(QStringLiteral("2@b"), QStringLiteral("b"));
        const ProjectEquateStation sameNamespaceAsA = station(QStringLiteral("3@a"), QStringLiteral("a"));
        const ProjectEquateStation c = station(QStringLiteral("4@c"), QStringLiteral("c"));
        const ProjectEquateStation missing = station(QStringLiteral("5@d"), QStringLiteral("d"), false);

        ProjectIndexSnapshot snapshot;
        snapshot.equateLinks.append(ProjectEquateLink{QStringLiteral("root.th"), 3, {a, b, sameNamespaceAsA, c}});
        snapshot.equateLinks.append(ProjectEquateLink{QStringLiteral("root.th"), 4, {b, missing}});
        snapshot.stationDrawingPointsByStationKey[a.stationKey] = {drawing(a, QStringLiteral("a-ee.th2"), QStringLiteral("extended")),
                                                                   drawing(a, QStringLiteral("a.th2"), QStringLiteral("plan"))};
        snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("b.th2"), QStringLiteral("plan"))};
        snapshot.stationDrawingPointsByStationKey[c.stationKey] = {drawing(c, QStringLiteral("c.th2"), QStringLiteral("extended"))};

        const QVector<ProjectJoinGroup> groups = projectJoinGroups(snapshot, QStringLiteral("plan"));
        QCOMPARE(groups.size(), 2);

        // a-b, a-c, b-3@a, b-c, 3@a-c; a and 3@a share a namespace so they do not pair.
        const QVector<ProjectJoinPair> &pairs = groups.at(0).pairs;
        QCOMPARE(pairs.size(), 5);
        QVERIFY(pairs.at(0).isAvailable());
        QCOMPARE(pairs.at(0).first.drawing->sourceFile, QStringLiteral("a.th2"));
        QCOMPARE(pairs.at(0).second.drawing->sourceFile, QStringLiteral("b.th2"));
        QCOMPARE(pairs.at(1).second.status, ProjectJoinSideStatus::NotDrawnInProjection);
        QVERIFY(!pairs.at(1).isAvailable());
        QCOMPARE(pairs.at(2).second.status, ProjectJoinSideStatus::NotDrawn);
        // A drawn equate is placed at its station; anything else needs computed positions.
        QVERIFY(!pairs.at(0).needsComputedPositions());
        QVERIFY(pairs.at(1).needsComputedPositions());
        ProjectJoinPair noSurvey = pairs.at(0);
        noSurvey.linkKind = ProjectStationLinkKind::NoSurvey;
        QVERIFY(noSurvey.needsComputedPositions());

        QCOMPARE(groups.at(1).pairs.size(), 1);
        QCOMPARE(groups.at(1).pairs.at(0).second.status, ProjectJoinSideStatus::Unresolved);
    }

    void standsInTheNearestScrapOfTheSurvey()
    {
        // 1@a is not drawn in plan, but survey a has two plan scraps; b draws 2@b.
        const ProjectEquateStation a = station(QStringLiteral("1@a"), QStringLiteral("a"));
        const ProjectEquateStation b = station(QStringLiteral("2@b"), QStringLiteral("b"));
        ProjectIndexSnapshot snapshot;
        snapshot.equateLinks.append(ProjectEquateLink{QStringLiteral("root.th"), 3, {a, b}});
        snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("b.th2"), QStringLiteral("plan"))};
        for (const QString &scrap : {QStringLiteral("far"), QStringLiteral("near"), QStringLiteral("elevation")}) {
            ProjectStructureEntry entry;
            entry.kind = ProjectStructureEntryKind::Scrap;
            entry.objectId = scrap;
            entry.namespacePath = QStringLiteral("A");
            entry.sourceFile = QStringLiteral("a.th2");
            snapshot.entries.append(entry);
            snapshot.scrapProjectionByObjectId.insert(scrap, scrap == QStringLiteral("elevation") ? QStringLiteral("extended")
                                                                                             : QStringLiteral("plan"));
            snapshot.stationPointsByScrapObjectId[scrap] = {ProjectScrapStationPoint{4, scrap + QStringLiteral("@a")}};
        }

        const ProjectJoinPair plan = projectJoinGroups(snapshot, QStringLiteral("plan")).at(0).pairs.at(0);
        QCOMPARE(plan.first.status, ProjectJoinSideStatus::SurveyDrawnWithoutStation);
        QCOMPARE(plan.first.surveyScraps.size(), 2);
        QVERIFY(!plan.isAvailable());
        QVERIFY(plan.canOpen());
        QVERIFY(plan.needsComputedPositions());
        // No offset in extended, so such rows do not open there.
        snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("b-ee.th2"), QStringLiteral("extended"))};
        const ProjectJoinPair extended = projectJoinGroups(snapshot, QStringLiteral("extended")).at(0).pairs.at(0);
        QCOMPARE(extended.first.status, ProjectJoinSideStatus::SurveyDrawnWithoutStation);
        QVERIFY(!extended.canOpen());

        QHash<QString, Th2StationPosition> positions = {{a.stationKey, {0.0, 0.0, 0.0}},
                                                        {QStringLiteral("far@a"), {0.0, 9.0, 0.0}},
                                                        {QStringLiteral("near@a"), {0.0, 0.0, 3.0}}};
        QCOMPARE(nearestSurveyScrap(snapshot, plan.first, positions).value_or(ProjectStructureEntry()).objectId,
                 QStringLiteral("near"));
        positions.remove(a.stationKey);
        QVERIFY(!nearestSurveyScrap(snapshot, plan.first, positions).has_value());
    }

    void fitsScrapsToMeasuredLegsAndSaysWhatRowsNeed()
    {
        // Scrap s draws three stations of two 5 m legs at twice the scale; t draws one station, so has no leg.
        ProjectIndexSnapshot snapshot;
        snapshot.scrapProjectionByObjectId = {{QStringLiteral("s"), QStringLiteral("plan")},
                                              {QStringLiteral("t"), QStringLiteral("plan")}};
        snapshot.stationPointsByScrapObjectId[QStringLiteral("s")] = {
            ProjectScrapStationPoint{1, QStringLiteral("1@a"), QPointF(0.0, 0.0)},
            ProjectScrapStationPoint{2, QStringLiteral("2@a"), QPointF(0.0, 10.0)},
            ProjectScrapStationPoint{3, QStringLiteral("3@a"), QPointF(10.0, 10.0)}};
        snapshot.stationPointsByScrapObjectId[QStringLiteral("t")] = {
            ProjectScrapStationPoint{1, QStringLiteral("1@b"), QPointF(0.0, 0.0)}};
        const QVector<Th2MeasuredLeg> legs = {
            Th2MeasuredLeg{QStringLiteral("1@a"), QStringLiteral("2@a"), {0.0, 5.0, 0.0}},
            Th2MeasuredLeg{QStringLiteral("2@a"), QStringLiteral("3@a"), {5.0, 0.0, 0.0}},
            Th2MeasuredLeg{QStringLiteral("1@b"), QStringLiteral("9@b"), {5.0, 0.0, 0.0}}};
        const QSet<QString> fitted = projectScrapsFittedByMeasuredLegs(snapshot, legs);
        QCOMPARE(fitted, QSet<QString>({QStringLiteral("s")}));

        ProjectJoinPair pair;
        pair.projection = QStringLiteral("plan");
        pair.first.status = ProjectJoinSideStatus::Drawn;
        pair.first.drawing = ProjectStationDrawingPoint{QStringLiteral("1@a"), QStringLiteral("a.th2"), 1, {}, {}, pair.projection, QStringLiteral("s")};
        pair.second.status = ProjectJoinSideStatus::Drawn;
        pair.second.drawing = ProjectStationDrawingPoint{QStringLiteral("1@b"), QStringLiteral("b.th2"), 1, {}, {}, pair.projection, QStringLiteral("s")};
        // A drawn equate needs nothing without positions (the status line covers that), nor when both scraps fit.
        QCOMPARE(projectJoinPairNeed(pair, nullptr), ProjectConnectionNeed::None);
        QCOMPARE(projectJoinPairNeed(pair, &fitted), ProjectConnectionNeed::None);
        pair.second.drawing->scrapObjectId = QStringLiteral("t");
        QCOMPARE(projectJoinPairNeed(pair, &fitted), ProjectConnectionNeed::Approximate);
        pair.linkKind = ProjectStationLinkKind::NoSurvey;
        QCOMPARE(projectJoinPairNeed(pair, nullptr), ProjectConnectionNeed::ComputedPositions);
        // A row that cannot be opened shows no need.
        pair.second.status = ProjectJoinSideStatus::NotDrawn;
        QCOMPARE(projectJoinPairNeed(pair, nullptr), ProjectConnectionNeed::None);

        ProjectJoinGroup joinOnly;
        ProjectStructureEntry scrapS;
        scrapS.objectId = QStringLiteral("s");
        ProjectStructureEntry scrapT;
        scrapT.objectId = QStringLiteral("t");
        joinOnly.joinOnlyScraps = {scrapS, scrapS};
        QCOMPARE(projectJoinOnlyNeed(joinOnly, nullptr), ProjectConnectionNeed::ComputedPositions);
        QCOMPARE(projectJoinOnlyNeed(joinOnly, &fitted), ProjectConnectionNeed::None);
        joinOnly.joinOnlyScraps = {scrapS, scrapT};
        QCOMPARE(projectJoinOnlyNeed(joinOnly, &fitted), ProjectConnectionNeed::Approximate);
    }

    void classifiesPairsByJoinAndListsJoinOnlyConnections()
    {
        const ProjectEquateStation a = station(QStringLiteral("1@a"), QStringLiteral("a"));
        const ProjectEquateStation b = station(QStringLiteral("2@b"), QStringLiteral("b"));
        ProjectIndexSnapshot snapshot;
        snapshot.equateLinks.append(ProjectEquateLink{QStringLiteral("root.th"), 3, {a, b}});
        snapshot.stationDrawingPointsByStationKey[a.stationKey] = {drawing(a, QStringLiteral("a.th2"), QStringLiteral("plan"))};
        snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("b.th2"), QStringLiteral("plan"))};
        snapshot.scrapProjectionByObjectId = {{QStringLiteral("sa"), QStringLiteral("plan")},
                                              {QStringLiteral("sb"), QStringLiteral("plan")},
                                              {QStringLiteral("sd"), QStringLiteral("plan")},
                                              {QStringLiteral("sbe"), QStringLiteral("extended")},
                                              {QStringLiteral("sde"), QStringLiteral("extended")}};
        ProjectStructureEntry scrapD;
        scrapD.kind = ProjectStructureEntryKind::Scrap;
        scrapD.objectId = QStringLiteral("sd");
        scrapD.name = QStringLiteral("SD");
        scrapD.sourceFile = QStringLiteral("d.th2");
        scrapD.lineNumber = 4;
        snapshot.entries.append(scrapD);

        const auto item = [](const QString &scrap, const QString &namespacePath, ProjectStructureEntryKind kind) {
            ProjectJoinItem joinItem;
            joinItem.reference = scrap;
            joinItem.kind = kind;
            joinItem.scrapObjectId = scrap;
            joinItem.namespacePath = namespacePath;
            joinItem.resolved = true;
            return joinItem;
        };
        ProjectJoinCommand automatic{QStringLiteral("maps.th"), 10, ProjectJoinMode::Scraps, 0,
                                     {item(QStringLiteral("sa"), QStringLiteral("a"), ProjectStructureEntryKind::Scrap),
                                      item(QStringLiteral("sb"), QStringLiteral("b"), ProjectStructureEntryKind::Scrap)}};
        ProjectJoinCommand wallsToD{QStringLiteral("maps.th"), 11, ProjectJoinMode::PointsAndLines, 0,
                                    {item(QStringLiteral("sa"), QStringLiteral("a"), ProjectStructureEntryKind::Line),
                                     item(QStringLiteral("sd"), QStringLiteral("d"), ProjectStructureEntryKind::Line)}};
        ProjectJoinCommand extendedOnly{QStringLiteral("maps.th"), 12, ProjectJoinMode::Scraps, 2,
                                        {item(QStringLiteral("sbe"), QStringLiteral("b"), ProjectStructureEntryKind::Scrap),
                                         item(QStringLiteral("sde"), QStringLiteral("d"), ProjectStructureEntryKind::Scrap)}};
        snapshot.joinCommands = {automatic, wallsToD, extendedOnly};

        QVector<ProjectJoinGroup> plan = projectJoinGroups(snapshot, QStringLiteral("plan"));
        QCOMPARE(plan.size(), 2);
        QCOMPARE(plan.at(0).pairs.first().connection, ProjectJoinConnection::AutomaticScrapJoin);
        QCOMPARE(plan.at(0).pairs.first().joins.first().lineNumber, 10);
        // a and d have no equate or nosurvey, so their wall join is listed on its own.
        QVERIFY(plan.at(1).joinOnly.has_value());
        QCOMPARE(plan.at(1).joinOnly->lineNumber, 11);
        QCOMPARE(plan.at(1).joinOnlyConnection, ProjectJoinConnection::PointLineJoin);
        QCOMPARE(plan.at(1).joinOnlyScraps.size(), 2);
        QCOMPARE(plan.at(1).joinOnlyScraps.at(1).sourceFile, QStringLiteral("d.th2"));

        // In the extended view only the counted join exists, and b-d has no link.
        const QVector<ProjectJoinGroup> extended = projectJoinGroups(snapshot, QStringLiteral("extended"));
        QCOMPARE(extended.at(0).pairs.first().connection, ProjectJoinConnection::None);
        QCOMPARE(extended.at(1).joinOnlyConnection, ProjectJoinConnection::CountedScrapJoin);

        // Two join kinds between the same surveys are an input error.
        ProjectJoinCommand alsoWalls = wallsToD;
        alsoWalls.items[1] = item(QStringLiteral("sb"), QStringLiteral("b"), ProjectStructureEntryKind::Line);
        snapshot.joinCommands.append(alsoWalls);
        plan = projectJoinGroups(snapshot, QStringLiteral("plan"));
        QCOMPARE(plan.at(0).pairs.first().connection, ProjectJoinConnection::MixedJoins);

        // A join between surveys linked by an equate does not appear again as join-only.
        snapshot.equateLinks.append(
            ProjectEquateLink{QStringLiteral("root.th"), 5, {a, station(QStringLiteral("9@d"), QStringLiteral("d"))}});
        plan = projectJoinGroups(snapshot, QStringLiteral("plan"));
        QCOMPARE(std::count_if(plan.cbegin(), plan.cend(), [](const ProjectJoinGroup &group) {
                     return group.joinOnly.has_value();
                 }),
                 0);
    }
};

int runProjectJoinListTest(int argc, char **argv)
{
    ProjectJoinListTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectJoinListTest.moc"
