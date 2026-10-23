#pragma once

#include "../core/ProjectJoinList.h"

#include <QColor>
#include <QElapsedTimer>
#include <QIcon>
#include <QSet>
#include <QWidget>

#include <optional>

class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTimer;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QToolButton;
class QTreeView;

namespace TherionStudio
{
// Joins page of the project navigation sidebar: cross-survey equates for one projection.
class ProjectJoinsPanel final : public QWidget
{
    Q_OBJECT

public:
    // Text colours for rows joined by Therion `join` commands; chosen by the application theme.
    struct ConnectionColors
    {
        QColor automaticScrapJoin;
        QColor countedScrapJoin;
        QColor pointLineJoin;
    };

    explicit ProjectJoinsPanel(QWidget *parent = nullptr);

    void setProjectIndex(const ProjectIndexSnapshot &snapshot);
    // Whether double-click / Enter opens the second drawing's map in a separate window; the context menu offers the
    // other choice.
    void setActivationOpensSeparateWindow(bool separateWindow);
    void setConnectionColors(const ConnectionColors &colors);
    // The line above the list saying where station positions come from, with a button to choose the target config
    // when none is selected.
    void setDataStatus(const QString &text, bool offerTargetConfig);
    // Scraps fitted by the measured legs that came with the station positions, or none without positions; rows show
    // what they need (projectJoinPairNeed), faded when the positions are out of date.
    void setConnectionData(const std::optional<QSet<QString>> &fittedScraps, bool outOfDate);
    // Shows a busy bar, the seconds since the run started and Cancel while Therion computes station positions.
    void setRunInProgress(bool running);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void joinRequested(const TherionStudio::ProjectJoinPair &pair, bool separateWindow);
    // A join-only row between two scraps: no station is shared, so placement needs computed station positions.
    void joinOnlyRequested(const TherionStudio::ProjectJoinGroup &group, bool separateWindow);
    void sourceLocationRequested(const QString &sourceFile, int lineNumber);
    void statusMessageRequested(const QString &message);
    void targetConfigRequested();
    void runCancelRequested();

private:
    void rebuildRows();
    void activateRow(const QModelIndex &proxyIndex);
    void showContextMenu(const QPoint &position);
    const ProjectJoinPair *pairForIndex(const QModelIndex &proxyIndex) const;
    const ProjectJoinGroup *groupForIndex(const QModelIndex &proxyIndex) const;
    QStandardItem *pairItem(const ProjectJoinPair &pair, const QString &text, int groupIndex, int pairIndex) const;
    QString unavailableReason(const ProjectJoinPair &pair) const;
    QString sideDescription(const ProjectJoinSide &side) const;
    QStandardItem *joinOnlyItem(const ProjectJoinGroup &group, int groupIndex) const;
    // Bold marks an automatic scrap join backed by an equate or nosurvey leg (`linkedByStation`).
    void applyConnectionStyle(QStandardItem *item, ProjectJoinConnection connection, bool linkedByStation) const;
    QStringList connectionDescription(ProjectJoinConnection connection, const QVector<ProjectJoinCommand> &joins) const;
    QString linkDescription(const ProjectEquateLink &link) const;
    void openJoinOnlyScraps(const ProjectJoinGroup &group);
    [[nodiscard]] const QSet<QString> *fittedScraps() const { return fittedScraps_.has_value() ? &*fittedScraps_ : nullptr; }
    void updateLegend();
    QIcon ruleWarningIcon() const;
    void applyRuleWarning(QStandardItem *item,
                          const QVector<ProjectJoinRuleViolation> &violations,
                          QStringList *toolTip) const;
    // The need symbol, unless the row already shows the rule warning.
    void applyConnectionNeed(QStandardItem *item, ProjectConnectionNeed need, QStringList *toolTip) const;
    [[nodiscard]] QIcon connectionNeedIcon(ProjectConnectionNeed need, bool faded) const;

    ProjectIndexSnapshot snapshot_;
    QVector<ProjectJoinGroup> groups_;
    QLabel *dataStatusLabel_ = nullptr;
    QPushButton *targetConfigButton_ = nullptr;
    QWidget *runProgress_ = nullptr;
    QLabel *runElapsedLabel_ = nullptr;
    QTimer *runElapsedTimer_ = nullptr;
    QElapsedTimer runElapsed_;
    std::optional<QSet<QString>> fittedScraps_;
    bool dataOutOfDate_ = false;
    QComboBox *projectionCombo_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    // Key below the list: what each row colour, font and marker means; the header button folds it away.
    QWidget *legendBox_ = nullptr;
    QToolButton *legendToggle_ = nullptr;
    QWidget *legend_ = nullptr;
    QGridLayout *legendGrid_ = nullptr;
    ConnectionColors colors_;
    QTreeView *tree_ = nullptr;
    QStandardItemModel *model_ = nullptr;
    QSortFilterProxyModel *proxyModel_ = nullptr;
    bool activationOpensSeparateWindow_ = false;
    bool contextMenuFromKeyboard_ = false;
};
}
