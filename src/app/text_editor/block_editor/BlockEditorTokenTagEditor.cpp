#include "BlockEditorTokenTagEditor.h"

#include "../../../core/TherionSourceDocument.h"

#include <QCompleter>
#include <QKeyEvent>
#include <QLayout>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QAbstractItemView>

#include <algorithm>

namespace TherionStudio
{
namespace
{
class TokenFlowLayout final : public QLayout
{
public:
    explicit TokenFlowLayout(QWidget *parent)
        : QLayout(parent)
    {
        setContentsMargins(0, 0, 0, 0);
        setSpacing(4);
    }

    ~TokenFlowLayout() override
    {
        while (QLayoutItem *item = takeAt(0)) {
            delete item;
        }
    }

    void addItem(QLayoutItem *item) override
    {
        items_.append(item);
    }

    int count() const override
    {
        return items_.size();
    }

    QLayoutItem *itemAt(int index) const override
    {
        return index >= 0 && index < items_.size() ? items_.at(index) : nullptr;
    }

    QLayoutItem *takeAt(int index) override
    {
        if (index < 0 || index >= items_.size()) {
            return nullptr;
        }
        return items_.takeAt(index);
    }

    Qt::Orientations expandingDirections() const override
    {
        return {};
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(int width) const override
    {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize minimumSize() const override
    {
        QSize size;
        for (const QLayoutItem *item : items_) {
            size = size.expandedTo(item->minimumSize());
        }
        QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    QSize sizeHint() const override
    {
        return minimumSize();
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        QMargins margins = contentsMargins();
        const QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;
        const int hSpace = spacing();
        const int vSpace = spacing();

        for (QLayoutItem *item : items_) {
            if (item == nullptr || item->isEmpty()) {
                continue;
            }
            const QSize itemSize = item->sizeHint();
            const int nextX = x + itemSize.width() + hSpace;
            if (nextX - hSpace > effectiveRect.right() && lineHeight > 0) {
                x = effectiveRect.x();
                y += lineHeight + vSpace;
                lineHeight = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), itemSize));
            }
            x += itemSize.width() + hSpace;
            lineHeight = std::max(lineHeight, itemSize.height());
        }
        return y + lineHeight - rect.y() + margins.bottom();
    }

    QList<QLayoutItem *> items_;
};
}

BlockEditorTokenTagEditor::BlockEditorTokenTagEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    chipsContainer_ = new QWidget(this);
    chipsContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    chipsLayout_ = new TokenFlowLayout(chipsContainer_);
    chipsContainer_->setVisible(false);
    layout->addWidget(chipsContainer_);

    input_ = new QLineEdit(this);
    input_->setObjectName(QStringLiteral("tokenTagEditorInput"));
    input_->setMinimumWidth(96);
    input_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    input_->installEventFilter(this);
    layout->addWidget(input_);

    connect(input_, &QLineEdit::returnPressed, this, [this]() {
        commitInputTokens();
    });
    connect(input_, &QLineEdit::selectionChanged, this, [this]() {
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    });
    connect(input_, &QLineEdit::cursorPositionChanged, this, [this](int, int) {
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    });
}

void BlockEditorTokenTagEditor::setPlaceholderText(const QString &text)
{
    placeholderText_ = text;
    refreshInputPlaceholder();
}

void BlockEditorTokenTagEditor::setSuggestions(const QStringList &values)
{
    suggestions_.clear();
    for (const QString &value : values) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty()) {
            suggestions_.append(normalized);
        }
    }
    suggestions_.removeDuplicates();
    std::sort(suggestions_.begin(), suggestions_.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    rebuildCompleter();
}

void BlockEditorTokenTagEditor::setTokens(const QStringList &tokens)
{
    QStringList normalized;
    for (const QString &token : tokens) {
        const QString trimmed = token.trimmed();
        if (!trimmed.isEmpty() && !containsTokenCaseInsensitive(normalized, trimmed)) {
            normalized.append(trimmed);
        }
    }
    tokens_ = normalized;
    rebuildChips();
    refreshInputPlaceholder();
    updateGeometry();
}

QStringList BlockEditorTokenTagEditor::tokens() const
{
    return tokens_;
}

bool BlockEditorTokenTagEditor::hasEditorFocus() const
{
    if (input_ != nullptr && input_->hasFocus()) {
        return true;
    }
    for (QToolButton *chip : chipButtons_) {
        if (chip != nullptr && chip->hasFocus()) {
            return true;
        }
    }
    return false;
}

bool BlockEditorTokenTagEditor::containsTokenCaseInsensitive(const QStringList &tokens, const QString &token)
{
    for (const QString &existing : tokens) {
        if (QString::compare(existing.trimmed(), token.trimmed(), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void BlockEditorTokenTagEditor::rebuildCompleter()
{
    if (input_ == nullptr) {
        return;
    }
    if (suggestions_.isEmpty()) {
        input_->setCompleter(nullptr);
        return;
    }
    auto *completer = new QCompleter(suggestions_, input_);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    input_->setCompleter(completer);
    connect(completer,
            QOverload<const QString &>::of(&QCompleter::activated),
            input_,
            [this](const QString &choice) {
                const QString normalized = choice.trimmed();
                if (normalized.isEmpty()) {
                    return;
                }
                appendToken(normalized);
                // Clear after Qt finishes completer insertion to avoid stale token text.
                QTimer::singleShot(0, this, [this]() {
                    if (input_ != nullptr) {
                        input_->clear();
                    }
                });
            });
}

void BlockEditorTokenTagEditor::refreshInputPlaceholder()
{
    if (input_ == nullptr) {
        return;
    }
    input_->setPlaceholderText(tokens_.isEmpty() ? placeholderText_ : QString());
}

bool BlockEditorTokenTagEditor::appendToken(const QString &token)
{
    const QString normalized = token.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    if (containsTokenCaseInsensitive(tokens_, normalized)) {
        return false;
    }
    tokens_.append(normalized);
    rebuildChips();
    refreshInputPlaceholder();
    if (onTokensChanged) {
        onTokensChanged();
    }
    return true;
}

bool BlockEditorTokenTagEditor::commitInputTokens()
{
    if (input_ == nullptr) {
        return false;
    }
    const QString rawInput = input_->text().trimmed();
    if (rawInput.isEmpty()) {
        return false;
    }
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(rawInput);
    const TherionSourceDocumentLine *sourceLine = sourceDocument.lineAtLineNumber(1);
    QStringList parsedTokens = sourceLine != nullptr ? sourceLine->sourceLine.parsed.tokens : QStringList{};
    if (parsedTokens.isEmpty()) {
        parsedTokens = rawInput.split(QRegularExpression(QStringLiteral(R"(\s+)")), Qt::SkipEmptyParts);
    }
    if (parsedTokens.isEmpty()) {
        return false;
    }
    for (const QString &token : parsedTokens) {
        const QString normalized = token.trimmed();
        if (!normalized.isEmpty()) {
            appendToken(normalized);
        }
    }
    input_->clear();
    return true;
}

void BlockEditorTokenTagEditor::rebuildChips()
{
    if (chipsLayout_ == nullptr || input_ == nullptr) {
        return;
    }
    for (QToolButton *chip : chipButtons_) {
        if (chip == nullptr) {
            continue;
        }
        chipsLayout_->removeWidget(chip);
        chip->deleteLater();
    }
    chipButtons_.clear();

    for (int index = 0; index < tokens_.size(); ++index) {
        const QString token = tokens_.at(index);
        auto *chip = new QToolButton(this);
        chip->setObjectName(QStringLiteral("tokenTagEditorChip"));
        chip->setText(QStringLiteral("%1  ×").arg(token));
        chip->setProperty("token_index", index);
        chip->setAutoRaise(false);
        chip->setFocusPolicy(Qt::StrongFocus);
        chip->setStyleSheet(QStringLiteral(
            "QToolButton#tokenTagEditorChip {"
            " border: 1px solid palette(mid);"
            " border-radius: 10px;"
            " padding: 1px 8px;"
            " background: palette(button);"
            "}"
            "QToolButton#tokenTagEditorChip:hover {"
            " background: palette(light);"
            "}"
        ));
        connect(chip, &QToolButton::clicked, this, [this, chip]() {
            const int tokenIndex = chip->property("token_index").toInt();
            if (tokenIndex < 0 || tokenIndex >= tokens_.size()) {
                return;
            }
            tokens_.removeAt(tokenIndex);
            rebuildChips();
            refreshInputPlaceholder();
            if (onTokensChanged) {
                onTokensChanged();
            }
        });
        connect(chip, &QToolButton::pressed, this, [this]() {
            if (onFocusContextChanged) {
                onFocusContextChanged();
            }
        });
        chipsLayout_->addWidget(chip);
        chipButtons_.append(chip);
    }
    if (chipsContainer_ != nullptr) {
        chipsContainer_->setVisible(!chipButtons_.isEmpty());
        chipsContainer_->updateGeometry();
    }
    updateGeometry();
}

bool BlockEditorTokenTagEditor::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != input_ || event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent == nullptr) {
            return QWidget::eventFilter(watched, event);
        }
        if (keyEvent->key() == Qt::Key_Space
            || keyEvent->key() == Qt::Key_Comma
            || keyEvent->key() == Qt::Key_Return
            || keyEvent->key() == Qt::Key_Enter
            || keyEvent->key() == Qt::Key_Tab) {
            const bool isCompletionAcceptKey = keyEvent->key() == Qt::Key_Return
                || keyEvent->key() == Qt::Key_Enter
                || keyEvent->key() == Qt::Key_Tab;
            if (isCompletionAcceptKey) {
                if (QCompleter *completer = input_->completer();
                    completer != nullptr && completer->popup() != nullptr && completer->popup()->isVisible()) {
                    return QWidget::eventFilter(watched, event);
                }
            }
            if (commitInputTokens()) {
                return true;
            }
        }
    } else if (event->type() == QEvent::FocusIn) {
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    } else if (event->type() == QEvent::FocusOut) {
        commitInputTokens();
        if (onFocusContextChanged) {
            onFocusContextChanged();
        }
    }
    return QWidget::eventFilter(watched, event);
}
}
