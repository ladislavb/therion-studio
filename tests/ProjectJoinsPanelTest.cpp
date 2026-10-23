#include "../src/app/ProjectJoinsPanel.h"
#include "../src/core/ProjectJoinRules.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
ProjectEquateStation station(const QString &reference, const QString &namespacePath)
{
    return ProjectEquateStation{reference, reference + QLatin1Char('|') + namespacePath, namespacePath, true};
}

ProjectStationDrawingPoint drawing(const ProjectEquateStation &station, const QString &file, const QString &projection)
{
    return ProjectStationDrawingPoint{station.stationKey, file, 9, QStringLiteral("s"), QStringLiteral("survey"), projection};
}

ProjectIndexSnapshot joinSnapshot()
{
    const ProjectEquateStation a = station(QStringLiteral("1@upper"), QStringLiteral("upper"));
    const ProjectEquateStation b = station(QStringLiteral("2@lower"), QStringLiteral("lower"));
    const ProjectEquateStation c = station(QStringLiteral("3@side"), QStringLiteral("side"));
    ProjectIndexSnapshot snapshot;
    snapshot.equateLinks = {ProjectEquateLink{QStringLiteral("root.th"), 5, {a, b}},
                            ProjectEquateLink{QStringLiteral("root.th"), 6, {a, b, c}}};
    snapshot.stationDrawingPointsByStationKey[a.stationKey] = {drawing(a, QStringLiteral("upper.th2"), QStringLiteral("plan"))};
    snapshot.stationDrawingPointsByStationKey[b.stationKey] = {drawing(b, QStringLiteral("lower.th2"), QStringLiteral("plan")),
                                                               drawing(b, QStringLiteral("lower-ee.th2"), QStringLiteral("extended"))};
    snapshot.stationDrawingPointsByStationKey[c.stationKey] = {drawing(c, QStringLiteral("side-ee.th2"), QStringLiteral("extended"))};
    return snapshot;
}
}

class ProjectJoinsPanelTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsJoinsAndOpensAvailablePair()
    {
        ProjectJoinsPanel panel;
        panel.setProjectIndex(joinSnapshot());
        auto *combo = panel.findChild<QComboBox *>();
        auto *tree = panel.findChild<QTreeView *>();
        QVERIFY(combo != nullptr && tree != nullptr);
        QCOMPARE(combo->currentText(), QStringLiteral("plan"));
        QCOMPARE(combo->count(), 2);

        QAbstractItemModel *model = tree->model();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("1@upper = 2@lower"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("1@upper = 2@lower = 3@side"));
        QCOMPARE(model->rowCount(model->index(1, 0)), 3);

        QSignalSpy joinSpy(&panel, &ProjectJoinsPanel::joinRequested);
        QSignalSpy statusSpy(&panel, &ProjectJoinsPanel::statusMessageRequested);
        emit tree->activated(model->index(0, 0));
        QCOMPARE(joinSpy.count(), 1);
        const QList<QVariant> arguments = joinSpy.takeFirst();
        QCOMPARE(arguments.at(1).toBool(), false); // default: both drawings as tabs
        const ProjectJoinPair pair = arguments.at(0).value<ProjectJoinPair>();
        QCOMPARE(pair.first.drawing->sourceFile, QStringLiteral("upper.th2"));
        QCOMPARE(pair.second.drawing->sourceFile, QStringLiteral("lower.th2"));

        // 1@upper = 3@side: side is only drawn in the extended view.
        emit tree->activated(model->index(1, 0, model->index(1, 0)));
        QCOMPARE(joinSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 1);
        QVERIFY(statusSpy.takeFirst().at(0).toString().contains(QStringLiteral("3@side")));

        // The Settings option swaps double-click to open the second drawing in a separate window.
        panel.setActivationOpensSeparateWindow(true);
        emit tree->activated(model->index(0, 0));
        QCOMPARE(joinSpy.count(), 1);
        QCOMPARE(joinSpy.takeFirst().at(1).toBool(), true);
    }

    void opensARowWhoseSurveyDrawsTheViewWithoutItsStation()
    {
        // side has plan and extended scraps but draws 3@side in neither; upper only draws 1@upper in plan.
        ProjectIndexSnapshot snapshot = joinSnapshot();
        snapshot.stationDrawingPointsByStationKey.remove(station(QStringLiteral("3@side"), QStringLiteral("side")).stationKey);
        for (const QString &projection : {QStringLiteral("plan"), QStringLiteral("extended")}) {
            ProjectStructureEntry scrap;
            scrap.kind = ProjectStructureEntryKind::Scrap;
            scrap.objectId = QStringLiteral("side-") + projection;
            scrap.namespacePath = QStringLiteral("side");
            snapshot.entries.append(scrap);
            snapshot.scrapProjectionByObjectId.insert(scrap.objectId, projection);
            snapshot.stationPointsByScrapObjectId[scrap.objectId] = {ProjectScrapStationPoint{4, QStringLiteral("4@side")}};
        }
        ProjectJoinsPanel panel;
        panel.setProjectIndex(snapshot);
        auto *combo = panel.findChild<QComboBox *>();
        auto *tree = panel.findChild<QTreeView *>();
        QAbstractItemModel *model = tree->model();
        QSignalSpy joinSpy(&panel, &ProjectJoinsPanel::joinRequested);
        QSignalSpy statusSpy(&panel, &ProjectJoinsPanel::statusMessageRequested);

        // 1@upper = 3@side in plan opens; the nearest side scrap is chosen when the join opens.
        const QModelIndex planRow = model->index(1, 0, model->index(1, 0));
        QVERIFY(planRow.data(Qt::ToolTipRole).toString().contains(QStringLiteral("nearest")));
        emit tree->activated(planRow);
        QCOMPARE(joinSpy.count(), 1);
        QCOMPARE(joinSpy.takeFirst().at(0).value<ProjectJoinPair>().second.status,
                 ProjectJoinSideStatus::SurveyDrawnWithoutStation);

        // 2@lower = 3@side in extended does not, and says why.
        combo->setCurrentText(QStringLiteral("extended"));
        const QModelIndex extendedRow = model->index(2, 0, model->index(1, 0));
        QCOMPARE(extendedRow.data().toString(), QStringLiteral("2@lower = 3@side"));
        emit tree->activated(extendedRow);
        QCOMPARE(joinSpy.count(), 0);
        QVERIFY(statusSpy.takeFirst().at(0).toString().contains(QStringLiteral("extended elevation")));
    }

    void keyboardContextMenuTargetsTheCurrentRow()
    {
        ProjectJoinsPanel panel;
        panel.resize(400, 400);
        panel.setProjectIndex(joinSnapshot());
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        auto *tree = panel.findChild<QTreeView *>();
        tree->setFocus();
        // Row 1 is a multi-station group: its menu has no `Open Connection` (that is per pair), unlike row 0.
        tree->setCurrentIndex(tree->model()->index(1, 0));

        // Read the popup's actions, then close it so the modal menu returns.
        QStringList menuActions;
        QTimer::singleShot(0, [&menuActions]() {
            if (auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget())) {
                for (const QAction *action : menu->actions()) {
                    menuActions.append(action->text());
                }
                menu->close();
            }
        });
        // The Menu key / Shift+F10 sends a keyboard context-menu event to the focused view.
        QContextMenuEvent keyboardMenu(QContextMenuEvent::Keyboard, QPoint(0, 0), tree->mapToGlobal(QPoint(0, 0)));
        QCoreApplication::sendEvent(tree, &keyboardMenu);

        QVERIFY2(menuActions.contains(tr("Go to Equate in Source")), qPrintable(menuActions.join(QLatin1Char('|'))));
        QVERIFY2(!menuActions.contains(tr("Open Connection")), qPrintable(menuActions.join(QLatin1Char('|'))));
    }

    void coloursRowsByJoinKindAndOpensJoinOnlyScraps()
    {
        ProjectIndexSnapshot snapshot = joinSnapshot();
        const auto scrapItem = [](const QString &scrap, const QString &namespacePath, ProjectStructureEntryKind kind) {
            ProjectJoinItem item;
            item.reference = scrap + QLatin1Char('@') + namespacePath;
            item.kind = kind;
            item.scrapObjectId = scrap;
            item.namespacePath = namespacePath;
            item.resolved = true;
            return item;
        };
        snapshot.scrapProjectionByObjectId = {{QStringLiteral("su"), QStringLiteral("plan")},
                                              {QStringLiteral("sl"), QStringLiteral("plan")},
                                              {QStringLiteral("sf"), QStringLiteral("plan")}};
        ProjectStructureEntry far;
        far.kind = ProjectStructureEntryKind::Scrap;
        far.objectId = QStringLiteral("sf");
        far.sourceFile = QStringLiteral("far.th2");
        far.lineNumber = 2;
        ProjectStructureEntry upper = far;
        upper.objectId = QStringLiteral("su");
        upper.sourceFile = QStringLiteral("upper.th2");
        snapshot.entries = {upper, far};
        snapshot.joinCommands = {
            ProjectJoinCommand{QStringLiteral("maps.th"), 20, ProjectJoinMode::Scraps, 0,
                               {scrapItem(QStringLiteral("su"), QStringLiteral("upper"), ProjectStructureEntryKind::Scrap),
                                scrapItem(QStringLiteral("sl"), QStringLiteral("lower"), ProjectStructureEntryKind::Scrap)}},
            ProjectJoinCommand{QStringLiteral("maps.th"), 21, ProjectJoinMode::PointsAndLines, 0,
                               {scrapItem(QStringLiteral("su"), QStringLiteral("upper"), ProjectStructureEntryKind::Line),
                                scrapItem(QStringLiteral("sf"), QStringLiteral("far"), ProjectStructureEntryKind::Line)}}};
        snapshot.joinRuleViolations = projectJoinRuleViolations(snapshot.joinCommands);

        ProjectJoinsPanel panel;
        auto *legend = panel.findChild<QWidget *>(QStringLiteral("joinsLegend"));
        panel.setProjectIndex(snapshot);
        QVERIFY(!legend->isVisibleTo(&panel));
        const ProjectJoinsPanel::ConnectionColors colors{QColor(Qt::darkGreen), QColor(Qt::darkCyan), QColor(Qt::darkYellow)};
        panel.setConnectionColors(colors);
        QVERIFY(legend->isVisibleTo(&panel));
        // Nine entries styled like the rows, plus the warning and two connection-need icons; the Key button folds them
        // away.
        QCOMPARE(legend->findChildren<QLabel *>().size(), 12);
        auto *legendToggle = panel.findChild<QToolButton *>(QStringLiteral("joinsLegendToggle"));
        legendToggle->click();
        QVERIFY(!legend->isVisibleTo(&panel));
        legendToggle->click();
        QVERIFY(legend->isVisibleTo(&panel));

        auto *tree = panel.findChild<QTreeView *>();
        QAbstractItemModel *model = tree->model();
        // 1@upper = 2@lower: the two surveys are joined by an automatic scrap join.
        const QModelIndex joined = model->index(0, 0);
        QCOMPARE(joined.data(Qt::ForegroundRole).value<QBrush>().color(), colors.automaticScrapJoin);
        QVERIFY(joined.data(Qt::FontRole).value<QFont>().bold());
        QVERIFY(joined.data(Qt::ToolTipRole).toString().contains(QStringLiteral("maps.th:20")));

        // upper ~ far has only a wall join and no equate: listed as its own row.
        const QModelIndex joinOnly = model->index(model->rowCount() - 1, 0);
        QCOMPARE(joinOnly.data().toString(), QStringLiteral("(su@upper ~ sf@far)"));
        QCOMPARE(joinOnly.data(Qt::ForegroundRole).value<QBrush>().color(), colors.pointLineJoin);
        // Bold is reserved for an automatic scrap join backed by an equate or nosurvey leg.
        QVERIFY(!joinOnly.data(Qt::FontRole).value<QFont>().bold());
        // A single wall join breaks a join rule: the row carries the warning marker; the scrap join row does not.
        QVERIFY(!joinOnly.data(Qt::DecorationRole).value<QIcon>().isNull());
        QVERIFY(joinOnly.data(Qt::ToolTipRole).toString().contains(QStringLiteral("Validation")));
        QVERIFY(joined.data(Qt::DecorationRole).value<QIcon>().isNull());
        // Without station positions the join-only row also says it needs them; its icon stays the rule warning.
        QVERIFY(joinOnly.data(Qt::ToolTipRole).toString().contains(QStringLiteral("export database -fmt sql")));

        // Two scraps: opened as a pair, placed from computed station positions when they are available.
        QSignalSpy joinOnlySpy(&panel, &ProjectJoinsPanel::joinOnlyRequested);
        emit tree->activated(joinOnly);
        QCOMPARE(joinOnlySpy.count(), 1);
        const auto group = joinOnlySpy.at(0).at(0).value<ProjectJoinGroup>();
        QCOMPARE(group.joinOnlyScraps.at(1).sourceFile, QStringLiteral("far.th2"));

        // With positions whose legs fit no scrap, the drawn equate row is placed by its XVI only. This rebuilds the
        // rows, so earlier indexes are not used after it.
        panel.setConnectionData(QSet<QString>(), false);
        const QModelIndex approximate = model->index(0, 0);
        QVERIFY(!approximate.data(Qt::DecorationRole).value<QIcon>().isNull());
        QVERIFY(approximate.data(Qt::ToolTipRole).toString().contains(QStringLiteral("placed by its XVI")));
        panel.setConnectionData(QSet<QString>(), true);
        QVERIFY(model->index(0, 0).data(Qt::ToolTipRole).toString().contains(QStringLiteral("out of date")));
    }

    void linesUpRowTextWithAndWithoutASymbol()
    {
        // The row with the rule warning and a plain row are given the same width for the same text.
        QStandardItemModel model;
        auto *withIcon = new QStandardItem(QStringLiteral("1@a = 2@b"));
        withIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning));
        model.appendRow(withIcon);
        model.appendRow(new QStandardItem(QStringLiteral("1@a = 2@b")));
        ProjectJoinsPanel panel;
        auto *tree = panel.findChild<QTreeView *>();
        QStyleOptionViewItem option;
        option.initFrom(tree);
        option.widget = tree;
        // As the view sets it: its icon size, the small icon size by default.
        const int extent = tree->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, tree);
        option.decorationSize = QSize(extent, extent);
        QAbstractItemDelegate *delegate = tree->itemDelegate();
        QCOMPARE(delegate->sizeHint(option, model.index(1, 0)).width(),
                 delegate->sizeHint(option, model.index(0, 0)).width());
    }

    void offersTheTargetConfigOnlyWhenNoneIsSelected()
    {
        ProjectJoinsPanel panel;
        QSignalSpy requested(&panel, &ProjectJoinsPanel::targetConfigRequested);
        auto *button = panel.findChild<QPushButton *>(QStringLiteral("connectionsTargetConfigButton"));
        QVERIFY(button != nullptr);
        QVERIFY(!button->isVisibleTo(&panel));
        panel.setDataStatus(QStringLiteral("No target config"), true);
        QVERIFY(button->isVisibleTo(&panel));
        button->click();
        QCOMPARE(requested.count(), 1);
        panel.setDataStatus(QStringLiteral("Positions"), false);
        QVERIFY(!button->isVisibleTo(&panel));
    }

    void showsRunProgressWithCancel()
    {
        ProjectJoinsPanel panel;
        QSignalSpy cancelled(&panel, &ProjectJoinsPanel::runCancelRequested);
        auto *bar = panel.findChild<QProgressBar *>();
        QVERIFY(bar != nullptr);
        QVERIFY(!bar->isVisibleTo(&panel));
        panel.setRunInProgress(true);
        QVERIFY(bar->isVisibleTo(&panel));
        auto *cancel = panel.findChild<QPushButton *>(QStringLiteral("connectionsRunCancelButton"));
        QVERIFY(cancel != nullptr);
        cancel->click();
        QCOMPARE(cancelled.count(), 1);
        panel.setRunInProgress(false);
        QVERIFY(!bar->isVisibleTo(&panel));
    }

    void keepsProjectionAndFiltersRows()
    {
        ProjectJoinsPanel panel;
        panel.setProjectIndex(joinSnapshot());
        auto *combo = panel.findChild<QComboBox *>();
        auto *filter = panel.findChild<QLineEdit *>();
        auto *tree = panel.findChild<QTreeView *>();
        combo->setCurrentText(QStringLiteral("extended"));
        panel.setProjectIndex(joinSnapshot());
        QCOMPARE(combo->currentText(), QStringLiteral("extended"));

        filter->setText(QStringLiteral("SIDE"));
        QCOMPARE(tree->model()->rowCount(), 1);
        QCOMPARE(tree->model()->index(0, 0).data().toString(), QStringLiteral("1@upper = 2@lower = 3@side"));
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ProjectJoinsPanelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ProjectJoinsPanelTest.moc"
