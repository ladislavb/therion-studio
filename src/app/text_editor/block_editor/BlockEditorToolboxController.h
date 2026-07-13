#pragma once

#include <QString>
#include <QStringList>

#include <functional>

#include "../../../core/TherionSourceSnapshotCache.h"

class QComboBox;
class QGraphicsScene;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorCommandMetadata;

struct BlockEditorToolboxContext
{
    bool *tearingDown = nullptr;
    QLineEdit **filterEdit = nullptr;
    QListWidget **toolboxList = nullptr;
    QComboBox **scopeCombo = nullptr;
    QGraphicsScene **canvasScene = nullptr;
    QPlainTextEdit **editor = nullptr;
    const TextEditorCommandMetadata *commandMetadata = nullptr;
    std::function<QString()> documentPath;
    std::function<QString(const QString &)> normalizeCompletionContext;
    std::function<bool(const QString &, const QString &)> isCompatibleChildKindForBlocks;
    std::function<QString(const char *)> translate;
};

class BlockEditorToolboxController final
{
public:
    explicit BlockEditorToolboxController(BlockEditorToolboxContext context);

    void populateBlockToolbox();
    void populateBlockToolboxScopeCombo();
    void updateBlockToolboxScopeLabel();
    QString selectedBlockInsertionContextToken() const;

private:
    BlockEditorToolboxContext context_;

    QLineEdit *filterEdit() const;
    QListWidget *toolboxList() const;
    QComboBox *scopeCombo() const;
    QGraphicsScene *canvasScene() const;
    QPlainTextEdit *editor() const;
    QString normalizeCompletionContext(const QString &contextToken) const;
    bool isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const;
    bool isCommandAllowedForDocument(const QString &commandToken) const;
    QString tr(const char *text) const;

    mutable TherionSourceSnapshotCache sourceSnapshotCache_;
};
}
