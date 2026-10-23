#include "ProjectJoinsPanel.h"

#include "LucideIconFactory.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace TherionStudio
{
namespace
{
constexpr int GroupIndexRole = Qt::UserRole + 1;
constexpr int PairIndexRole = Qt::UserRole + 2;
constexpr int RowKeyRole = Qt::UserRole + 3;

QString stationReferences(const QVector<ProjectEquateStation> &stations)
{
    QStringList references;
    references.reserve(stations.size());
    for (const ProjectEquateStation &station : stations) {
        references.append(station.reference);
    }
    return references.join(QStringLiteral(" = "));
}

QString groupKey(const ProjectJoinGroup &group)
{
    if (group.joinOnly.has_value()) {
        return QStringLiteral("join:%1:%2").arg(group.joinOnly->sourceFile, QString::number(group.joinOnly->lineNumber));
    }
    return QStringLiteral("%1:%2").arg(group.link.sourceFile, QString::number(group.link.lineNumber));
}

QString joinItemReferences(const ProjectJoinCommand &join)
{
    QStringList references;
    for (const ProjectJoinItem &item : join.items) {
        references.append(item.reference);
    }
    // Brackets make join-only rows stand apart from `A = B` equate rows.
    return QStringLiteral("(%1)").arg(references.join(QStringLiteral(" ~ ")));
}

// Every row keeps room for a symbol, so row text lines up whether a row shows one or not.
class ReservedIconDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (!option->features.testFlag(QStyleOptionViewItem::HasDecoration)) {
            const int extent = option->widget != nullptr
                ? option->widget->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, option->widget)
                : 16;
            option->features |= QStyleOptionViewItem::HasDecoration;
            option->decorationSize = QSize(extent, extent);
        }
    }
};

QString fileLocation(const QString &sourceFile, int lineNumber)
{
    return QStringLiteral("%1:%2").arg(QFileInfo(sourceFile).fileName(), QString::number(lineNumber));
}
}

ProjectJoinsPanel::ProjectJoinsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    dataStatusLabel_ = new QLabel(this);
    dataStatusLabel_->setObjectName(QStringLiteral("connectionsDataStatus"));
    dataStatusLabel_->setTextFormat(Qt::PlainText);
    dataStatusLabel_->setWordWrap(true);
    layout->addWidget(dataStatusLabel_);
    targetConfigButton_ = new QPushButton(tr("Select Target Config..."), this);
    targetConfigButton_->setObjectName(QStringLiteral("connectionsTargetConfigButton"));
    targetConfigButton_->setVisible(false);
    connect(targetConfigButton_, &QPushButton::clicked, this, &ProjectJoinsPanel::targetConfigRequested);
    layout->addWidget(targetConfigButton_, 0, Qt::AlignLeft);

    runProgress_ = new QWidget(this);
    auto *runProgressLayout = new QHBoxLayout(runProgress_);
    runProgressLayout->setContentsMargins(0, 0, 0, 0);
    runProgressLayout->setSpacing(6);
    auto *runBar = new QProgressBar(runProgress_);
    // No end is known, so the bar only shows that Therion is busy.
    runBar->setRange(0, 0);
    runBar->setTextVisible(false);
    runElapsedLabel_ = new QLabel(runProgress_);
    auto *cancelButton = new QPushButton(tr("Cancel"), runProgress_);
    cancelButton->setObjectName(QStringLiteral("connectionsRunCancelButton"));
    connect(cancelButton, &QPushButton::clicked, this, &ProjectJoinsPanel::runCancelRequested);
    runProgressLayout->addWidget(runBar, 1);
    runProgressLayout->addWidget(runElapsedLabel_);
    runProgressLayout->addWidget(cancelButton);
    runProgress_->setVisible(false);
    layout->addWidget(runProgress_);
    runElapsedTimer_ = new QTimer(this);
    runElapsedTimer_->setInterval(1000);
    connect(runElapsedTimer_, &QTimer::timeout, this, [this]() {
        runElapsedLabel_->setText(tr("%1 s").arg(runElapsed_.elapsed() / 1000));
    });

    auto *projectionRow = new QHBoxLayout;
    projectionRow->setContentsMargins(0, 0, 0, 0);
    projectionRow->setSpacing(6);
    auto *projectionLabel = new QLabel(tr("View:"), this);
    projectionCombo_ = new QComboBox(this);
    projectionCombo_->setToolTip(tr("Scrap projection whose drawings are connected"));
    projectionLabel->setBuddy(projectionCombo_);
    projectionRow->addWidget(projectionLabel);
    projectionRow->addWidget(projectionCombo_, 1);
    layout->addLayout(projectionRow);

    filterEdit_ = new QLineEdit(this);
    filterEdit_->setPlaceholderText(tr("Filter by survey or station"));
    filterEdit_->setClearButtonEnabled(true);
    layout->addWidget(filterEdit_);

    model_ = new QStandardItemModel(this);
    proxyModel_ = new QSortFilterProxyModel(this);
    proxyModel_->setSourceModel(model_);
    proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel_->setRecursiveFilteringEnabled(true);

    tree_ = new QTreeView(this);
    tree_->setItemDelegate(new ReservedIconDelegate(tree_));
    tree_->setMinimumWidth(0);
    tree_->setModel(proxyModel_);
    tree_->setRootIsDecorated(true);
    tree_->setAnimated(true);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setAlternatingRowColors(true);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->header()->hide();
    layout->addWidget(tree_, 1);

    legendBox_ = new QWidget(this);
    auto *legendLayout = new QVBoxLayout(legendBox_);
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->setSpacing(2);
    legendToggle_ = new QToolButton(legendBox_);
    legendToggle_->setObjectName(QStringLiteral("joinsLegendToggle"));
    legendToggle_->setText(tr("Key"));
    legendToggle_->setToolTip(tr("Show or hide what the row colours and markers mean"));
    legendToggle_->setCheckable(true);
    legendToggle_->setChecked(true);
    legendToggle_->setAutoRaise(true);
    legendToggle_->setArrowType(Qt::DownArrow);
    legendToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    legendLayout->addWidget(legendToggle_);
    legend_ = new QWidget(legendBox_);
    legend_->setObjectName(QStringLiteral("joinsLegend"));
    legendGrid_ = new QGridLayout(legend_);
    legendGrid_->setContentsMargins(6, 0, 0, 0);
    legendGrid_->setHorizontalSpacing(8);
    legendGrid_->setVerticalSpacing(2);
    legendGrid_->setColumnStretch(1, 1);
    legendLayout->addWidget(legend_);
    legendBox_->setVisible(false);
    layout->addWidget(legendBox_);
    connect(legendToggle_, &QToolButton::toggled, this, [this](bool shown) {
        legend_->setVisible(shown);
        legendToggle_->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
    });

    connect(projectionCombo_, &QComboBox::currentTextChanged, this, [this]() { rebuildRows(); });
    connect(filterEdit_, &QLineEdit::textChanged, proxyModel_, &QSortFilterProxyModel::setFilterFixedString);
    connect(tree_, &QTreeView::activated, this, &ProjectJoinsPanel::activateRow);
    connect(tree_, &QTreeView::customContextMenuRequested, this, &ProjectJoinsPanel::showContextMenu);
    // A keyboard-opened menu (Menu key, Shift+F10) must act on the current row, not the row under the pointer.
    tree_->installEventFilter(this);
    tree_->viewport()->installEventFilter(this);
}

void ProjectJoinsPanel::setProjectIndex(const ProjectIndexSnapshot &snapshot)
{
    snapshot_ = snapshot;
    const QString currentProjection = projectionCombo_->currentText();
    const QStringList projections = projectJoinProjections(snapshot_);
    {
        const QSignalBlocker blocker(projectionCombo_);
        projectionCombo_->clear();
        projectionCombo_->addItems(projections);
        const int keptIndex = projections.indexOf(currentProjection);
        projectionCombo_->setCurrentIndex(keptIndex >= 0 ? keptIndex : (projections.isEmpty() ? -1 : 0));
    }
    rebuildRows();
}

bool ProjectJoinsPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ContextMenu && (watched == tree_ || watched == tree_->viewport())) {
        contextMenuFromKeyboard_ = static_cast<QContextMenuEvent *>(event)->reason() == QContextMenuEvent::Keyboard;
    }
    return QWidget::eventFilter(watched, event);
}

void ProjectJoinsPanel::setActivationOpensSeparateWindow(bool separateWindow)
{
    activationOpensSeparateWindow_ = separateWindow;
}

void ProjectJoinsPanel::setDataStatus(const QString &text, bool offerTargetConfig)
{
    dataStatusLabel_->setText(text);
    targetConfigButton_->setVisible(offerTargetConfig);
}

void ProjectJoinsPanel::setRunInProgress(bool running)
{
    if (running == runProgress_->isVisibleTo(this)) {
        return;
    }
    runProgress_->setVisible(running);
    if (running) {
        runElapsed_.start();
        runElapsedLabel_->setText(tr("%1 s").arg(0));
        runElapsedTimer_->start();
    } else {
        runElapsedTimer_->stop();
    }
}

void ProjectJoinsPanel::setConnectionData(const std::optional<QSet<QString>> &fittedScraps, bool outOfDate)
{
    if (fittedScraps == fittedScraps_ && outOfDate == dataOutOfDate_) {
        return;
    }
    fittedScraps_ = fittedScraps;
    dataOutOfDate_ = outOfDate;
    rebuildRows();
}

void ProjectJoinsPanel::setConnectionColors(const ConnectionColors &colors)
{
    colors_ = colors;
    updateLegend();
    rebuildRows();
}

void ProjectJoinsPanel::updateLegend()
{
    while (QLayoutItem *child = legendGrid_->takeAt(0)) {
        delete child->widget();
        delete child;
    }
    // Each entry is styled like the rows it explains; the warning icon sits in its own column.
    const auto addEntry = [this](const QString &text, const QColor &color, bool bold, QLabel *icon = nullptr) {
        const int row = legendGrid_->rowCount();
        auto *label = new QLabel(text, legend_);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        QPalette labelPalette = label->palette();
        labelPalette.setColor(QPalette::WindowText, color);
        label->setPalette(labelPalette);
        QFont font = label->font();
        font.setBold(bold);
        label->setFont(font);
        if (icon != nullptr) {
            legendGrid_->addWidget(icon, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        }
        legendGrid_->addWidget(label, row, 1);
    };
    const QColor text = palette().color(QPalette::Active, QPalette::Text);
    addEntry(tr("Automatic Join"), colors_.automaticScrapJoin, true);
    addEntry(tr("Join with -count"), colors_.countedScrapJoin, false);
    addEntry(tr("Detailed Joins"), colors_.pointLineJoin, false);
    addEntry(tr("No scrap Joins"), text, false);
    addEntry(tr("Cannot be opened"), palette().color(QPalette::Disabled, QPalette::Text), false);
    auto *warning = new QLabel(legend_);
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    warning->setPixmap(ruleWarningIcon().pixmap(iconSize, iconSize));
    addEntry(tr("Joins not valid"), text, false, warning);
    for (const ProjectConnectionNeed need : {ProjectConnectionNeed::Approximate, ProjectConnectionNeed::ComputedPositions}) {
        auto *icon = new QLabel(legend_);
        icon->setPixmap(connectionNeedIcon(need, false).pixmap(iconSize, iconSize));
        addEntry(need == ProjectConnectionNeed::Approximate ? tr("Placed by its XVI only") : tr("Needs station positions"),
                 text,
                 false,
                 icon);
    }
    // Join-only rows are written `(A ~ B)`; the entry shows that form.
    addEntry(QStringLiteral("(A ~ B)  ") + tr("Join with no centreline tie"), text, false);
    legendBox_->setVisible(colors_.automaticScrapJoin.isValid());
}

QIcon ProjectJoinsPanel::ruleWarningIcon() const
{
    return style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this);
}

void ProjectJoinsPanel::applyRuleWarning(QStandardItem *item,
                                         const QVector<ProjectJoinRuleViolation> &violations,
                                         QStringList *toolTip) const
{
    if (violations.isEmpty()) {
        return;
    }
    item->setIcon(ruleWarningIcon());
    toolTip->append(tr("A join here breaks a join rule; see the Validation panel."));
}

QIcon ProjectJoinsPanel::connectionNeedIcon(ProjectConnectionNeed need, bool faded) const
{
    const QString name = need == ProjectConnectionNeed::Approximate ? QStringLiteral("circle-dashed")
                                                                     : QStringLiteral("database");
    const int extent = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    const QColor color = palette().color(faded ? QPalette::Disabled : QPalette::Active, QPalette::Text);
    return QIcon(renderLucidePixmap(name, color, extent, devicePixelRatioF()));
}

void ProjectJoinsPanel::applyConnectionNeed(QStandardItem *item, ProjectConnectionNeed need, QStringList *toolTip) const
{
    if (need == ProjectConnectionNeed::None) {
        return;
    }
    toolTip->append(need == ProjectConnectionNeed::Approximate
                        ? tr("A drawing is placed by its XVI, as its scrap has too few measured legs between station "
                             "points.")
                        : tr("Lines up once there are station positions; compile with \"export database -fmt sql\" "
                             "in the thconfig."));
    if (dataOutOfDate_) {
        toolTip->append(tr("The station positions are out of date."));
    }
    if (item->icon().isNull()) {
        item->setIcon(connectionNeedIcon(need, dataOutOfDate_));
    }
}

void ProjectJoinsPanel::rebuildRows()
{
    // Keep expansion and selection across background index refreshes.
    QSet<QString> expandedKeys;
    for (int row = 0; row < proxyModel_->rowCount(); ++row) {
        const QModelIndex index = proxyModel_->index(row, 0);
        if (tree_->isExpanded(index)) {
            expandedKeys.insert(index.data(RowKeyRole).toString());
        }
    }
    const QString currentKey = tree_->currentIndex().data(RowKeyRole).toString();

    groups_ = projectJoinGroups(snapshot_, projectionCombo_->currentText());
    model_->clear();
    for (int groupIndex = 0; groupIndex < groups_.size(); ++groupIndex) {
        const ProjectJoinGroup &group = groups_.at(groupIndex);
        const QString text = group.link.kind == ProjectStationLinkKind::NoSurvey
            ? QStringLiteral("%1 (nosurvey)").arg(stationReferences(group.link.stations))
            : stationReferences(group.link.stations);
        QStandardItem *groupItem = nullptr;
        if (group.joinOnly.has_value()) {
            groupItem = joinOnlyItem(group, groupIndex);
        } else if (group.pairs.size() == 1) {
            groupItem = pairItem(group.pairs.first(), text, groupIndex, 0);
        } else {
            groupItem = new QStandardItem(text);
            groupItem->setData(groupIndex, GroupIndexRole);
            QStringList toolTip = {linkDescription(group.link)};
            for (int pairIndex = 0; pairIndex < group.pairs.size(); ++pairIndex) {
                const ProjectJoinPair &pair = group.pairs.at(pairIndex);
                if (!pair.ruleViolations.isEmpty() && groupItem->icon().isNull()) {
                    applyRuleWarning(groupItem, pair.ruleViolations, &toolTip);
                }
                if (groupItem->icon().isNull()) {
                    applyConnectionNeed(groupItem, projectJoinPairNeed(pair, fittedScraps()), &toolTip);
                }
                groupItem->appendRow(pairItem(pair,
                                              stationReferences({pair.first.station, pair.second.station}),
                                              groupIndex,
                                              pairIndex));
            }
            groupItem->setToolTip(toolTip.join(QLatin1Char('\n')));
        }
        groupItem->setData(groupKey(group), RowKeyRole);
        model_->appendRow(groupItem);
    }

    for (int row = 0; row < proxyModel_->rowCount(); ++row) {
        const QModelIndex index = proxyModel_->index(row, 0);
        if (expandedKeys.contains(index.data(RowKeyRole).toString())) {
            tree_->expand(index);
        }
        if (!currentKey.isEmpty() && index.data(RowKeyRole).toString() == currentKey) {
            tree_->setCurrentIndex(index);
        }
    }
}

QStandardItem *ProjectJoinsPanel::pairItem(const ProjectJoinPair &pair,
                                           const QString &text,
                                           int groupIndex,
                                           int pairIndex) const
{
    auto *item = new QStandardItem(text);
    item->setData(groupIndex, GroupIndexRole);
    item->setData(pairIndex, PairIndexRole);
    item->setData(QStringLiteral("%1/%2").arg(groupIndex).arg(pairIndex), RowKeyRole);
    QStringList toolTip = {linkDescription(groups_.at(groupIndex).link),
                           sideDescription(pair.first),
                           sideDescription(pair.second)};
    toolTip += connectionDescription(pair.connection, pair.joins);
    applyConnectionStyle(item, pair.connection, true);
    applyRuleWarning(item, pair.ruleViolations, &toolTip);
    applyConnectionNeed(item, projectJoinPairNeed(pair, fittedScraps()), &toolTip);
    if (!pair.canOpen()) {
        item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
    }
    item->setToolTip(toolTip.join(QLatin1Char('\n')));
    return item;
}

QStandardItem *ProjectJoinsPanel::joinOnlyItem(const ProjectJoinGroup &group, int groupIndex) const
{
    auto *item = new QStandardItem(joinItemReferences(*group.joinOnly));
    item->setData(groupIndex, GroupIndexRole);
    QStringList toolTip = connectionDescription(group.joinOnlyConnection, {*group.joinOnly});
    toolTip.append(tr("No equate or nosurvey leg links these surveys."));
    applyConnectionStyle(item, group.joinOnlyConnection, false);
    applyRuleWarning(item, group.joinOnlyRuleViolations, &toolTip);
    applyConnectionNeed(item, projectJoinOnlyNeed(group, fittedScraps()), &toolTip);
    item->setToolTip(toolTip.join(QLatin1Char('\n')));
    return item;
}

QString ProjectJoinsPanel::linkDescription(const ProjectEquateLink &link) const
{
    const QString location = fileLocation(link.sourceFile, link.lineNumber);
    return link.kind == ProjectStationLinkKind::NoSurvey ? tr("nosurvey leg: %1").arg(location)
                                                         : tr("Equate: %1").arg(location);
}

void ProjectJoinsPanel::applyConnectionStyle(QStandardItem *item,
                                             ProjectJoinConnection connection,
                                             bool linkedByStation) const
{
    QColor color;
    switch (connection) {
    case ProjectJoinConnection::AutomaticScrapJoin:
        color = colors_.automaticScrapJoin;
        break;
    case ProjectJoinConnection::CountedScrapJoin:
        color = colors_.countedScrapJoin;
        break;
    case ProjectJoinConnection::PointLineJoin:
        color = colors_.pointLineJoin;
        break;
    case ProjectJoinConnection::None:
    case ProjectJoinConnection::MixedJoins:
        return;
    }
    if (color.isValid()) {
        item->setForeground(color);
    }
    if (connection == ProjectJoinConnection::AutomaticScrapJoin && linkedByStation) {
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
    }
}

QStringList ProjectJoinsPanel::connectionDescription(ProjectJoinConnection connection,
                                                     const QVector<ProjectJoinCommand> &joins) const
{
    QStringList lines;
    switch (connection) {
    case ProjectJoinConnection::None:
        return lines;
    case ProjectJoinConnection::AutomaticScrapJoin:
        lines.append(tr("Automatic scrap join"));
        break;
    case ProjectJoinConnection::CountedScrapJoin:
        lines.append(tr("Scrap join with -count"));
        break;
    case ProjectJoinConnection::PointLineJoin:
        lines.append(tr("Point or line join"));
        break;
    case ProjectJoinConnection::MixedJoins:
        lines.append(tr("More than one kind of join links these surveys."));
        break;
    }
    for (const ProjectJoinCommand &join : joins) {
        lines.append(tr("join: %1").arg(fileLocation(join.sourceFile, join.lineNumber)));
    }
    return lines;
}

void ProjectJoinsPanel::openJoinOnlyScraps(const ProjectJoinGroup &group)
{
    for (const ProjectStructureEntry &scrap : group.joinOnlyScraps) {
        emit sourceLocationRequested(scrap.sourceFile, scrap.lineNumber);
    }
    emit statusMessageRequested(tr("No station is shared by these drawings, so they open without alignment."));
}

QString ProjectJoinsPanel::sideDescription(const ProjectJoinSide &side) const
{
    switch (side.status) {
    case ProjectJoinSideStatus::Drawn:
        return tr("%1: %2 (scrap %3)")
            .arg(side.station.reference,
                 fileLocation(side.drawing->sourceFile, side.drawing->lineNumber),
                 side.drawing->scrapName);
    case ProjectJoinSideStatus::Unresolved:
        return tr("%1: station not found in any centreline").arg(side.station.reference);
    case ProjectJoinSideStatus::NotDrawn:
        return snapshot_.rootConfigPath.isEmpty()
            ? tr("%1: no station point in a drawing that Therion reads; no target config is selected, so those "
                 "drawings are unknown")
                  .arg(side.station.reference)
            : tr("%1: no station point in a drawing that Therion reads").arg(side.station.reference);
    case ProjectJoinSideStatus::NotDrawnInProjection:
        return tr("%1: not drawn in the %2 view").arg(side.station.reference, projectionCombo_->currentText());
    case ProjectJoinSideStatus::SurveyDrawnWithoutStation:
        return projectionCombo_->currentText() == QStringLiteral("extended")
            ? tr("%1: no station point in its survey's %2 drawings, and an extended elevation can only be aligned at a "
                 "shared station")
                  .arg(side.station.reference, projectionCombo_->currentText())
            : tr("%1: no station point in its survey's %2 drawings, so the drawing nearest the station opens")
                  .arg(side.station.reference, projectionCombo_->currentText());
    }
    return QString();
}

QString ProjectJoinsPanel::unavailableReason(const ProjectJoinPair &pair) const
{
    // A side that cannot be placed at all comes first; otherwise the extended view stops a side without its station.
    for (const ProjectJoinSide *side : {&pair.first, &pair.second}) {
        if (side->status != ProjectJoinSideStatus::Drawn && side->status != ProjectJoinSideStatus::SurveyDrawnWithoutStation) {
            return sideDescription(*side);
        }
    }
    return pair.first.status != ProjectJoinSideStatus::Drawn ? sideDescription(pair.first)
                                                             : sideDescription(pair.second);
}

const ProjectJoinGroup *ProjectJoinsPanel::groupForIndex(const QModelIndex &proxyIndex) const
{
    const QVariant groupIndex = proxyIndex.data(GroupIndexRole);
    if (!groupIndex.isValid() || groupIndex.toInt() < 0 || groupIndex.toInt() >= groups_.size()) {
        return nullptr;
    }
    return &groups_.at(groupIndex.toInt());
}

const ProjectJoinPair *ProjectJoinsPanel::pairForIndex(const QModelIndex &proxyIndex) const
{
    const ProjectJoinGroup *group = groupForIndex(proxyIndex);
    const QVariant pairIndex = proxyIndex.data(PairIndexRole);
    if (group == nullptr || !pairIndex.isValid() || pairIndex.toInt() >= group->pairs.size()) {
        return nullptr;
    }
    return &group->pairs.at(pairIndex.toInt());
}

void ProjectJoinsPanel::activateRow(const QModelIndex &proxyIndex)
{
    const ProjectJoinPair *pair = pairForIndex(proxyIndex);
    if (pair == nullptr) {
        const ProjectJoinGroup *group = groupForIndex(proxyIndex);
        if (group != nullptr && group->joinOnly.has_value()) {
            if (group->joinOnlyScraps.size() == 2) {
                emit joinOnlyRequested(*group, activationOpensSeparateWindow_);
            } else {
                openJoinOnlyScraps(*group);
            }
            return;
        }
        tree_->setExpanded(proxyIndex, !tree_->isExpanded(proxyIndex));
        return;
    }
    if (!pair->canOpen()) {
        emit statusMessageRequested(unavailableReason(*pair));
        return;
    }
    emit joinRequested(*pair, activationOpensSeparateWindow_);
}

void ProjectJoinsPanel::showContextMenu(const QPoint &position)
{
    const QModelIndex proxyIndex = contextMenuFromKeyboard_ ? tree_->currentIndex() : tree_->indexAt(position);
    const QPoint menuPosition = contextMenuFromKeyboard_ ? tree_->visualRect(proxyIndex).bottomLeft() : position;
    contextMenuFromKeyboard_ = false;
    const ProjectJoinGroup *group = groupForIndex(proxyIndex);
    if (group == nullptr) {
        return;
    }

    // Copies: a background index refresh may rebuild groups_ while the menu is open.
    const ProjectJoinGroup groupCopy = *group;
    if (groupCopy.joinOnly.has_value()) {
        QMenu menu(this);
        QAction *goToJoin = menu.addAction(tr("Go to Join in Source"));
        connect(goToJoin, &QAction::triggered, this, [this, groupCopy]() {
            emit sourceLocationRequested(groupCopy.joinOnly->sourceFile, groupCopy.joinOnly->lineNumber);
        });
        menu.addSeparator();
        for (const ProjectStructureEntry &scrap : groupCopy.joinOnlyScraps) {
            QAction *openScrap = menu.addAction(tr("Open %1").arg(QFileInfo(scrap.sourceFile).fileName()));
            connect(openScrap, &QAction::triggered, this, [this, scrap]() {
                emit sourceLocationRequested(scrap.sourceFile, scrap.lineNumber);
            });
        }
        menu.exec(tree_->viewport()->mapToGlobal(menuPosition));
        return;
    }
    const ProjectEquateLink link = group->link;
    const std::optional<ProjectJoinPair> pair = pairForIndex(proxyIndex) != nullptr
        ? std::optional<ProjectJoinPair>(*pairForIndex(proxyIndex))
        : std::nullopt;
    QMenu menu(this);
    if (pair.has_value()) {
        const bool separate = activationOpensSeparateWindow_;
        QAction *openJoin = menu.addAction(tr("Open Connection"));
        QAction *openOther = menu.addAction(separate ? tr("Open Connection in Tabs") : tr("Open Connection with Separate Window"));
        openJoin->setEnabled(pair->canOpen());
        openOther->setEnabled(pair->canOpen());
        connect(openJoin, &QAction::triggered, this, [this, pair, separate]() { emit joinRequested(*pair, separate); });
        connect(openOther, &QAction::triggered, this, [this, pair, separate]() { emit joinRequested(*pair, !separate); });
    }
    QAction *goToEquate = menu.addAction(link.kind == ProjectStationLinkKind::NoSurvey ? tr("Go to nosurvey Leg in Source")
                                                                                      : tr("Go to Equate in Source"));
    connect(goToEquate, &QAction::triggered, this, [this, link]() {
        emit sourceLocationRequested(link.sourceFile, link.lineNumber);
    });
    if (pair.has_value()) {
        menu.addSeparator();
        for (const ProjectJoinSide *side : {&pair->first, &pair->second}) {
            if (!side->drawing.has_value()) {
                continue;
            }
            const ProjectStationDrawingPoint drawing = *side->drawing;
            QAction *openDrawing = menu.addAction(tr("Open %1").arg(QFileInfo(drawing.sourceFile).fileName()));
            connect(openDrawing, &QAction::triggered, this, [this, drawing]() {
                emit sourceLocationRequested(drawing.sourceFile, drawing.lineNumber);
            });
        }
    }
    menu.exec(tree_->viewport()->mapToGlobal(menuPosition));
}
}
