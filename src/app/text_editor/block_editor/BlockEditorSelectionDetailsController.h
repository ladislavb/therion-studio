#pragma once

#include <QChar>
#include <QString>
#include <QStringList>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextBrowser;
class QWidget;

namespace TherionStudio
{
struct TextEditorCommandMetadata;

enum class BlockEditorSelectionDetailsMode
{
    Unsupported,
    StructuredOptions,
    SimpleValue,
    DataHeader,
};

struct BlockEditorSelectionDetailsContext
{
    bool *tearingDown = nullptr;
    bool *detailsPopulating = nullptr;
    int *selectedLineNumber = nullptr;
    QString *selectedKind = nullptr;
    QString *baseStatusText = nullptr;
    QChar *commentMarker = nullptr;
    QWidget *editPanel = nullptr;
    QLabel *titleLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *primaryFieldLabel = nullptr;
    QLabel *secondaryFieldLabel = nullptr;
    QLabel *commentFieldLabel = nullptr;
    QLabel *optionsLabel = nullptr;
    QLabel *optionArgsLabel = nullptr;
    QStackedWidget *primaryFieldStack = nullptr;
    QLineEdit *idEdit = nullptr;
    QLabel *readOnlyValueLabel = nullptr;
    QLineEdit *additionalPositionalEdit = nullptr;
    QLineEdit *commentEdit = nullptr;
    QWidget *readingsTagEditor = nullptr;
    QStackedWidget *secondaryFieldStack = nullptr;
    QTableWidget *optionsTable = nullptr;
    QWidget *optionArgsPanel = nullptr;
    QPushButton *addOptionButton = nullptr;
    QPushButton *removeOptionButton = nullptr;
    QPushButton *dataRowsButton = nullptr;
    QTextBrowser *helpBrowser = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<bool(QStringList *)> loadNormalizedLines;
    std::function<bool(QString *, int *)> loadSourceSnapshot;
    std::function<void()> clearDetailsPane;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<bool(const QString &)> supportsDetailsPaneForKind;
    std::function<bool(const QString &)> isContainerBlockDirectiveForBlocks;
    std::function<bool(const QString &)> commandSupportsInlineIdField;
    std::function<bool(const QString &)> commandHasRequiredIdArgument;
    std::function<QStringList(const QString &)> commandArgumentSignaturesFor;
    std::function<void(BlockEditorSelectionDetailsMode)> setDetailsMode;
    std::function<void(const QString &, const QStringList &, const QStringList &)> setReadingsTagEditor;
    std::function<void(QLineEdit *, const QStringList &)> installLineEditCompleter;
    std::function<void()> refreshOptionArgumentEditors;
    std::function<void()> updateHelpForCurrentFocus;
    std::function<void()> refreshApplyState;
    std::function<QString(const char *)> translate;
};

class BlockEditorSelectionDetailsController final
{
public:
    explicit BlockEditorSelectionDetailsController(BlockEditorSelectionDetailsContext context);

    bool loadSelectionDetails(const QString &kind, int lineNumber);

private:
    BlockEditorSelectionDetailsContext context_;
};
}
