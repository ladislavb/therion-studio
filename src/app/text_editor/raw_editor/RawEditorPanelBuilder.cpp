#include "../TextEditorTab.h"

#include "RawEditorCompletionController.h"
#include "RawEditorFindController.h"
#include "RawEditorTextEdit.h"
#include "../TextEditorSurfaceStyler.h"

#include "../../../editor/TherionSyntaxHighlighter.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCompleter>
#include <QFontMetricsF>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStringListModel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{
constexpr int kPanelPadding = 12;
constexpr int kPanelSpacing = 8;
constexpr int kBlocksSidePaneMinWidth = 320;
constexpr int kBlocksSidePaneMaxWidth = 460;

void applyThinSplitterStyle(QSplitter *splitter, const QString &objectName)
{
    if (splitter == nullptr) {
        return;
    }

    splitter->setObjectName(objectName);
    splitter->setHandleWidth(10);
    splitter->setStyleSheet(QString());
}
}

namespace TherionStudio
{
void TextEditorTab::buildRawEditorPanel()
{
    searchBar_ = new QFrame(this);
    searchBar_->setFrameShape(QFrame::StyledPanel);
    searchBar_->setVisible(false);

    auto *searchLayout = new QVBoxLayout(searchBar_);
    searchLayout->setContentsMargins(kPanelPadding, kPanelPadding, kPanelPadding, kPanelPadding);
    searchLayout->setSpacing(kPanelSpacing);

    auto *findRow = new QHBoxLayout;
    findRow->setSpacing(kPanelSpacing);

    findEdit_ = new QLineEdit(searchBar_);
    findEdit_->setPlaceholderText(tr("Find"));

    findNextButton_ = new QPushButton(tr("Next"), searchBar_);
    findPreviousButton_ = new QPushButton(tr("Previous"), searchBar_);

    closeSearchButton_ = new QPushButton(tr("Close"), searchBar_);
    closeSearchButton_->setAutoDefault(false);

    searchStatusLabel_ = new QLabel(searchBar_);
    searchStatusLabel_->setMinimumWidth(140);

    findRow->addWidget(findEdit_, 1);
    findRow->addWidget(findPreviousButton_);
    findRow->addWidget(findNextButton_);
    findRow->addWidget(closeSearchButton_);
    findRow->addWidget(searchStatusLabel_);

    auto *optionRow = new QHBoxLayout;
    optionRow->setSpacing(kPanelSpacing);

    wholeWordCheck_ = new QCheckBox(tr("Whole word"), searchBar_);
    caseSensitiveCheck_ = new QCheckBox(tr("Case sensitive"), searchBar_);

    optionRow->addWidget(wholeWordCheck_);
    optionRow->addWidget(caseSensitiveCheck_);
    optionRow->addStretch(1);

    replaceRow_ = new QWidget(searchBar_);
    auto *replaceLayout = new QHBoxLayout(replaceRow_);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(kPanelSpacing);

    replaceEdit_ = new QLineEdit(replaceRow_);
    replaceEdit_->setPlaceholderText(tr("Replace with"));
    replaceButton_ = new QPushButton(tr("Replace"), replaceRow_);
    replaceAllButton_ = new QPushButton(tr("Replace All"), replaceRow_);

    replaceLayout->addWidget(replaceEdit_, 1);
    replaceLayout->addWidget(replaceButton_);
    replaceLayout->addWidget(replaceAllButton_);

    searchLayout->addLayout(findRow);
    searchLayout->addLayout(optionRow);
    searchLayout->addWidget(replaceRow_);

    connect(findEdit_, &QLineEdit::textEdited, this, &TextEditorTab::handleFindTextEdited);
    connect(replaceEdit_, &QLineEdit::textEdited, this, &TextEditorTab::handleReplaceTextEdited);
    connect(wholeWordCheck_, &QCheckBox::toggled, this, &TextEditorTab::handleSearchOptionsChanged);
    connect(caseSensitiveCheck_, &QCheckBox::toggled, this, &TextEditorTab::handleSearchOptionsChanged);
    connect(findNextButton_, &QPushButton::clicked, this, &TextEditorTab::handleFindNextTriggered);
    connect(findPreviousButton_, &QPushButton::clicked, this, &TextEditorTab::handleFindPreviousTriggered);
    connect(replaceButton_, &QPushButton::clicked, this, &TextEditorTab::handleReplaceTriggered);
    connect(replaceAllButton_, &QPushButton::clicked, this, &TextEditorTab::handleReplaceAllTriggered);
    connect(closeSearchButton_, &QPushButton::clicked, this, &TextEditorTab::handleCloseSearchTriggered);

    closeFindBarShortcut_ = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    closeFindBarShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    closeFindBarShortcut_->setEnabled(false);
    connect(closeFindBarShortcut_, &QShortcut::activated, this, &TextEditorTab::hideFindBar);

    editor_ = new RawEditorTextEdit(this);
    editor_->setFrameShape(QFrame::NoFrame);
    editor_->setObjectName(QStringLiteral("rawTextEditorCanvas"));
    editor_->setStyleSheet(QStringLiteral(
        "QPlainTextEdit#rawTextEditorCanvas {"
        " border-left: none;"
        " border-right: 1px solid palette(mid);"
        " border-top: none;"
        " border-bottom: none;"
        "}"));
    editor_->setTabChangesFocus(false);
    editor_->setTabStopDistance(QFontMetricsF(editor_->font()).horizontalAdvance(QLatin1Char(' ')) * 4.0);
    editor_->setPlaceholderText(tr("Open a Therion text file to begin editing."));
    highlighter_ = new TherionSyntaxHighlighter(catalogStore_, editor_->document());

    completionModel_ = new QStringListModel(this);
    completionCompleter_ = new QCompleter(completionModel_, this);
    completionCompleter_->setWidget(editor_);
    completionCompleter_->setCaseSensitivity(Qt::CaseInsensitive);
    completionCompleter_->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completionCompleter_->setCompletionMode(QCompleter::PopupCompletion);
    connect(completionCompleter_,
            QOverload<const QString &>::of(&QCompleter::activated),
            this,
            &TextEditorTab::insertCompletionToken);
    editor_->installEventFilter(this);
    editor_->viewport()->installEventFilter(this);
    if (completionCompleter_->popup() != nullptr) {
        completionCompleter_->popup()->setFocusPolicy(Qt::NoFocus);
        completionCompleter_->popup()->installEventFilter(this);
    }

    RawEditorCompletionControllerContext completionContext;
    completionContext.eventContext = this;
    completionContext.editor = editor_;
    completionContext.completionCompleter = completionCompleter_;
    completionContext.completionModel = completionModel_;
    completionContext.metadata = &commandMetadata_;
    completionContext.projectRootPath = [this]() { return projectRootPath_; };
    completionContext.filePath = [this]() { return filePath_; };
    completionContext.normalizedDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    completionContext.openingDirectiveForClosingToken = [this](const QString &directive) {
        return openingDirectiveForClosingToken(directive);
    };
    completionContext.closingDirectiveForOpeningToken = [this](const QString &directive) {
        return closingDirectiveForOpeningToken(directive);
    };
    completionContext.isContainerDirectiveInstance = [this](const QString &directive, const TherionParsedLine &parsedLine) {
        return isContainerDirectiveInstanceForParsedLine(directive, parsedLine);
    };
    rawEditorCompletionController_ = std::make_unique<RawEditorCompletionController>(std::move(completionContext));

    loadHelpMetadata();

    statusRow_ = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusRow_);
    statusLayout->setContentsMargins(kPanelPadding, 0, kPanelPadding, kPanelPadding);
    statusLayout->setSpacing(kPanelSpacing);

    encodingNoteLabel_ = new QLabel(statusRow_);
    encodingNoteLabel_->setWordWrap(true);
    encodingNoteLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    encodingNoteLabel_->setVisible(false);
    convertEncodingButton_ = new QPushButton(tr("Convert to UTF-8"), statusRow_);
    convertEncodingButton_->setVisible(false);
    convertEncodingButton_->setEnabled(false);
    convertEncodingButton_->setAutoDefault(false);
    connect(convertEncodingButton_, &QPushButton::clicked, this, &TextEditorTab::handleConvertToUtf8Triggered);

    statusLayout->addWidget(encodingNoteLabel_, 1);
    statusLayout->addWidget(convertEncodingButton_);

    editorHelpSplitter_ = new QSplitter(Qt::Horizontal, this);
    editorHelpSplitter_->setChildrenCollapsible(false);
    applyThinSplitterStyle(editorHelpSplitter_, QStringLiteral("textEditorHelpSplitter"));
    editorHelpSplitter_->addWidget(editor_);

    buildHelpPanel();
    helpPanel_->setMinimumWidth(kBlocksSidePaneMinWidth);
    helpPanel_->setMaximumWidth(kBlocksSidePaneMaxWidth);
    editorHelpSplitter_->addWidget(helpPanel_);
    editorHelpSplitter_->setStretchFactor(0, 1);
    editorHelpSplitter_->setStretchFactor(1, 0);
    editorHelpSplitter_->setCollapsible(1, true);
    editorHelpSplitter_->setSizes({980, 380});

    rawEditorPanel_ = new QWidget(this);
    auto *rawEditorLayout = new QHBoxLayout(rawEditorPanel_);
    rawEditorLayout->setContentsMargins(0, 0, 0, 0);
    rawEditorLayout->setSpacing(kPanelSpacing);
    rawEditorLayout->addWidget(editorHelpSplitter_, 1);

    RawEditorFindContext findContext;
    findContext.editor = editor_;
    findContext.searchBar = searchBar_;
    findContext.replaceRow = replaceRow_;
    findContext.findEdit = findEdit_;
    findContext.replaceEdit = replaceEdit_;
    findContext.searchStatusLabel = searchStatusLabel_;
    findContext.wholeWordCheck = wholeWordCheck_;
    findContext.caseSensitiveCheck = caseSensitiveCheck_;
    findContext.replaceButton = replaceButton_;
    findContext.replaceAllButton = replaceAllButton_;
    findContext.blocksModeActive = &blocksModeActive_;
    findContext.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    rawEditorFindController_ = std::make_unique<RawEditorFindController>(std::move(findContext));
}
}
