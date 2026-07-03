#include "MainWindowHelpDialog.h"

#include "MainWindowHelpDocument.h"
#include "../platform/ApplicationLanguageOverride.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QShortcut>
#include <QSettings>
#include <QSplitter>
#include <QSysInfo>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

#ifndef THERION_STUDIO_VERSION_STRING
#define THERION_STUDIO_VERSION_STRING "unknown"
#endif

#ifndef THERION_STUDIO_PACKAGE_LABEL_STRING
#define THERION_STUDIO_PACKAGE_LABEL_STRING THERION_STUDIO_VERSION_STRING
#endif

namespace TherionStudio
{
namespace
{
constexpr auto kApplicationLanguageKey = "settings/applicationLanguage";

QString buildString(const char *value)
{
    return QString::fromUtf8(value);
}

QString aboutMarkdown()
{
    const QString version = buildString(THERION_STUDIO_VERSION_STRING);
    const QString packageLabel = buildString(THERION_STUDIO_PACKAGE_LABEL_STRING);
    const QString qtVersion = QString::fromLatin1(QT_VERSION_STR);
    const QString platform = QStringLiteral("%1 (%2)")
                                 .arg(QSysInfo::prettyProductName(),
                                      QSysInfo::currentCpuArchitecture());

    return QCoreApplication::translate(
               "MainWindowHelpDialog",
               "# Therion Studio\n"
               "\n"
               "Cross-platform Qt desktop editor for Therion cave surveying software.\n"
               "\n"
               "- **Version:** `%1`\n"
               "- **Build:** `%2`\n"
               "- **Qt:** `%3`\n"
               "- **Platform:** `%4`\n"
               "- **License:** GNU General Public License v3.0 or later (`GPL-3.0-or-later`)\n"
               "- **Maintainer:** Ladislav Blazek\n"
               "\n"
               "Repository: <https://github.com/ladislavb/therion-studio>\n"
               "\n"
               "Third-party notices: <https://github.com/ladislavb/therion-studio/blob/main/docs/THIRD_PARTY_NOTICES.md>\n"
               "\n"
               "Therion Studio does not bundle the external Therion compiler. Install Therion separately and configure it in Settings.\n")
        .arg(version, packageLabel, qtVersion, platform);
}

void appendUnique(QStringList &values, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty() && !values.contains(trimmed)) {
        values.append(trimmed);
    }
}

QStringList userManualLocaleTags()
{
    QStringList tags;
    QString applicationLanguage = TherionStudio::Platform::applicationLanguageOverride();
    if (applicationLanguage == QStringLiteral("system")) {
        applicationLanguage = QSettings()
            .value(QString::fromLatin1(kApplicationLanguageKey), QStringLiteral("system"))
            .toString();
    }
    applicationLanguage = TherionStudio::Platform::normalizeApplicationLanguageSetting(applicationLanguage);
    if (applicationLanguage == QStringLiteral("en")) {
        appendUnique(tags, QStringLiteral("en"));
        return tags;
    }
    if (applicationLanguage == QStringLiteral("cs") || applicationLanguage == QStringLiteral("sk")) {
        appendUnique(tags, applicationLanguage);
        return tags;
    }

    const QLocale locale;
    for (const QString &uiLanguage : locale.uiLanguages()) {
        const QString normalized = uiLanguage.trimmed();
        appendUnique(tags, normalized);
        appendUnique(tags, normalized.toLower());
        appendUnique(tags, QString(normalized).replace(QLatin1Char('-'), QLatin1Char('_')));
        appendUnique(tags, QString(normalized).toLower().replace(QLatin1Char('-'), QLatin1Char('_')));

        const int separatorIndex = normalized.indexOf(QLatin1Char('-'));
        if (separatorIndex > 0) {
            appendUnique(tags, normalized.left(separatorIndex).toLower());
        }
    }

    const QString localeName = locale.name();
    if (localeName != QStringLiteral("C")) {
        appendUnique(tags, localeName);
        const int separatorIndex = localeName.indexOf(QLatin1Char('_'));
        if (separatorIndex > 0) {
            appendUnique(tags, localeName.left(separatorIndex).toLower());
        }
    }
    return tags;
}

QStringList userManualFileNames()
{
    QStringList fileNames;
    for (const QString &localeTag : userManualLocaleTags()) {
        appendUnique(fileNames, QStringLiteral("USER_MANUAL.%1.md").arg(localeTag));
    }
    appendUnique(fileNames, QStringLiteral("USER_MANUAL.md"));
    return fileNames;
}

QStringList userManualRootCandidates()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    return {
        QDir(cwd).absoluteFilePath(QStringLiteral("docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/docs")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/doc/therion-studio")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../share/doc/therion-studio")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../share/doc/therion-studio")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../../share/doc/therion-studio"))};
}

QStringList userManualPathCandidates()
{
    QStringList candidates;
    const QStringList fileNames = userManualFileNames();
    const QStringList roots = userManualRootCandidates();
    for (const QString &fileName : fileNames) {
        for (const QString &root : roots) {
            appendUnique(candidates, QDir(root).absoluteFilePath(fileName));
        }
    }
    return candidates;
}

QString resolveUserManualPath()
{
    const QStringList candidates = userManualPathCandidates();
    for (const QString &candidatePath : candidates) {
        const QFileInfo info(candidatePath);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

QString missingUserManualMarkdown()
{
    return QCoreApplication::translate(
        "MainWindowHelpDialog",
        "# User Manual\n"
        "\n"
        "The user manual file was not found.\n"
        "\n"
        "Expected files are `docs/USER_MANUAL.<language>.md` or `docs/USER_MANUAL.md`.\n");
}

QString loadUtf8TextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QString markdownStyleSheet(const QPalette &palette)
{
    const QString textColor = palette.color(QPalette::Text).name(QColor::HexRgb);
    const QString linkColor = palette.color(QPalette::Link).name(QColor::HexRgb);
    const QString borderColor = palette.color(QPalette::Mid).name(QColor::HexRgb);
    const QString codeBackground = palette.color(QPalette::AlternateBase).name(QColor::HexRgb);

    return QStringLiteral(
               "body { color: %1; font-size: 14px; line-height: 1.45; }"
               "h1 { font-size: 26px; margin-top: 0; margin-bottom: 18px; }"
               "h2 { font-size: 22px; margin-top: 28px; margin-bottom: 12px; }"
               "h3 { font-size: 18px; margin-top: 20px; margin-bottom: 8px; }"
               "h4 { font-size: 15px; margin-top: 16px; margin-bottom: 6px; }"
               "p { margin-top: 0; margin-bottom: 12px; }"
               "ul, ol { margin-top: 6px; margin-bottom: 14px; margin-left: 22px; }"
               "li { margin-bottom: 6px; }"
               "table { border-collapse: collapse; margin-top: 10px; margin-bottom: 18px; }"
               "th, td { border: 1px solid %3; padding: 6px 8px; }"
               "th { font-weight: bold; }"
               "code { background-color: %4; padding: 1px 3px; }"
               "pre { background-color: %4; margin-top: 10px; margin-bottom: 16px; padding: 10px; }"
               "a { color: %2; text-decoration: none; }")
        .arg(textColor, linkColor, borderColor, codeBackground);
}

void openHelpLink(QTextBrowser *browser, const QUrl &url)
{
    const QString fragment = url.fragment(QUrl::FullyDecoded);
    if (!fragment.isEmpty() && (url.isRelative() || url.scheme().isEmpty())) {
        browser->scrollToAnchor(fragment);
        return;
    }

    QDesktopServices::openUrl(url);
}

bool shouldShowSectionInToc(const MainWindowHelpSection &section)
{
    return section.level > 1
           && section.anchor != QStringLiteral("contents")
           && section.anchor != QStringLiteral("obsah");
}

void populateTableOfContents(QTreeWidget *toc, const QVector<MainWindowHelpSection> &sections)
{
    toc->clear();

    QVector<QTreeWidgetItem *> parents(7, nullptr);
    for (const MainWindowHelpSection &section : sections) {
        if (!shouldShowSectionInToc(section)) {
            continue;
        }

        auto *item = new QTreeWidgetItem(QStringList(section.title));
        item->setData(0, Qt::UserRole, section.anchor);

        QTreeWidgetItem *parentItem = nullptr;
        const int sectionLevel = qBound(2, section.level, 6);
        for (int parentLevel = sectionLevel - 1; parentLevel >= 2; --parentLevel) {
            if (parents.at(parentLevel) != nullptr) {
                parentItem = parents.at(parentLevel);
                break;
            }
        }

        if (parentItem != nullptr) {
            parentItem->addChild(item);
        } else {
            toc->addTopLevelItem(item);
        }

        parents[sectionLevel] = item;
        for (int lowerLevel = sectionLevel + 1; lowerLevel < parents.size(); ++lowerLevel) {
            parents[lowerLevel] = nullptr;
        }
    }

    toc->expandToDepth(1);
}

void configureManualSearch(QDialog *dialog,
                           QLineEdit *searchField,
                           QPushButton *previousButton,
                           QPushButton *nextButton,
                           QLabel *searchStatus,
                           QTextBrowser *browser)
{
    const auto updateSearchControls = [searchField, previousButton, nextButton, searchStatus]() {
        const bool hasSearchText = !searchField->text().trimmed().isEmpty();
        previousButton->setEnabled(hasSearchText);
        nextButton->setEnabled(hasSearchText);
        if (!hasSearchText) {
            searchStatus->clear();
        }
    };

    const auto findText = [browser, searchField, searchStatus](bool backward) {
        const QString searchText = searchField->text().trimmed();
        if (searchText.isEmpty()) {
            searchStatus->clear();
            return;
        }

        QTextDocument::FindFlags flags;
        if (backward) {
            flags |= QTextDocument::FindBackward;
        }

        if (browser->find(searchText, flags)) {
            searchStatus->clear();
            return;
        }

        const QTextCursor originalCursor = browser->textCursor();
        QTextCursor wrapCursor(browser->document());
        wrapCursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        browser->setTextCursor(wrapCursor);
        if (browser->find(searchText, flags)) {
            searchStatus->setText(QCoreApplication::translate("MainWindowHelpDialog", "Wrapped"));
            return;
        }

        browser->setTextCursor(originalCursor);
        searchStatus->setText(QCoreApplication::translate("MainWindowHelpDialog", "No matches"));
    };

    QObject::connect(searchField, &QLineEdit::textChanged, dialog, updateSearchControls);
    QObject::connect(searchField, &QLineEdit::returnPressed, dialog, [findText]() {
        findText(false);
    });
    QObject::connect(previousButton, &QPushButton::clicked, dialog, [findText]() {
        findText(true);
    });
    QObject::connect(nextButton, &QPushButton::clicked, dialog, [findText]() {
        findText(false);
    });

    auto *findShortcut = new QShortcut(QKeySequence::Find, dialog);
    QObject::connect(findShortcut, &QShortcut::activated, dialog, [searchField]() {
        searchField->setFocus(Qt::ShortcutFocusReason);
        searchField->selectAll();
    });

    auto *findNextShortcut = new QShortcut(QKeySequence::FindNext, dialog);
    QObject::connect(findNextShortcut, &QShortcut::activated, dialog, [findText]() {
        findText(false);
    });

    auto *findPreviousShortcut = new QShortcut(QKeySequence::FindPrevious, dialog);
    QObject::connect(findPreviousShortcut, &QShortcut::activated, dialog, [findText]() {
        findText(true);
    });

    updateSearchControls();
}

void showMarkdownDialog(QWidget *parent,
                        const QString &title,
                        const QString &markdown,
                        const QString &sourceLabel = QString())
{
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(title);
    dialog->resize(860, 680);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    if (!sourceLabel.trimmed().isEmpty()) {
        auto *sourceInfo = new QLabel(sourceLabel, dialog);
        sourceInfo->setWordWrap(true);
        sourceInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(sourceInfo);
    }

    auto *browser = new QTextBrowser(dialog);
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);
    browser->setReadOnly(true);
    browser->document()->setDocumentMargin(18.0);
    browser->document()->setDefaultStyleSheet(markdownStyleSheet(browser->palette()));
    browser->setHtml(markdownToHtmlWithHeadingAnchors(markdown));
    QObject::connect(browser, &QTextBrowser::anchorClicked, browser, [browser](const QUrl &url) {
        openHelpLink(browser, url);
    });
    layout->addWidget(browser, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::close);
    layout->addWidget(buttons);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void showUserManualMarkdownDialog(QWidget *parent,
                                  const QString &title,
                                  const QString &markdown)
{
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(title);
    dialog->resize(1080, 740);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Horizontal, dialog);

    auto *toc = new QTreeWidget(splitter);
    toc->setObjectName(QStringLiteral("userManualTableOfContents"));
    toc->setHeaderHidden(true);
    toc->setMinimumWidth(220);
    toc->setUniformRowHeights(true);
    populateTableOfContents(toc, parseMarkdownHelpSections(markdown));

    auto *contentPane = new QWidget(splitter);
    auto *contentLayout = new QVBoxLayout(contentPane);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);

    auto *searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(6);

    auto *searchField = new QLineEdit(contentPane);
    searchField->setObjectName(QStringLiteral("userManualSearchField"));
    searchField->setPlaceholderText(QCoreApplication::translate("MainWindowHelpDialog", "Search manual"));
    auto *previousButton = new QPushButton(QCoreApplication::translate("MainWindowHelpDialog", "Previous"), contentPane);
    auto *nextButton = new QPushButton(QCoreApplication::translate("MainWindowHelpDialog", "Next"), contentPane);
    auto *searchStatus = new QLabel(contentPane);
    searchStatus->setMinimumWidth(90);

    searchRow->addWidget(searchField, 1);
    searchRow->addWidget(previousButton);
    searchRow->addWidget(nextButton);
    searchRow->addWidget(searchStatus);
    contentLayout->addLayout(searchRow);

    auto *browser = new QTextBrowser(contentPane);
    browser->setObjectName(QStringLiteral("userManualBrowser"));
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);
    browser->setReadOnly(true);
    browser->document()->setDocumentMargin(18.0);
    browser->document()->setDefaultStyleSheet(markdownStyleSheet(browser->palette()));
    browser->setHtml(markdownToHtmlWithHeadingAnchors(markdown));
    QObject::connect(browser, &QTextBrowser::anchorClicked, browser, [browser](const QUrl &url) {
        openHelpLink(browser, url);
    });
    contentLayout->addWidget(browser, 1);

    QObject::connect(toc, &QTreeWidget::itemActivated, browser, [browser](QTreeWidgetItem *item, int) {
        if (item != nullptr) {
            browser->scrollToAnchor(item->data(0, Qt::UserRole).toString());
        }
    });
    QObject::connect(toc, &QTreeWidget::itemClicked, browser, [browser](QTreeWidgetItem *item, int) {
        if (item != nullptr) {
            browser->scrollToAnchor(item->data(0, Qt::UserRole).toString());
        }
    });

    configureManualSearch(dialog, searchField, previousButton, nextButton, searchStatus, browser);

    splitter->addWidget(toc);
    splitter->addWidget(contentPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 820});
    layout->addWidget(splitter, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::close);
    layout->addWidget(buttons);

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    searchField->setFocus(Qt::ShortcutFocusReason);
    searchField->selectAll();
}
}

void showAboutDialog(QWidget *parent)
{
    showMarkdownDialog(parent,
                       QCoreApplication::translate("MainWindow", "About Therion Studio"),
                       aboutMarkdown());
}

void showUserManualDialog(QWidget *parent)
{
    const QString manualPath = resolveUserManualPath();
    if (manualPath.isEmpty()) {
        showMarkdownDialog(
            parent,
            QCoreApplication::translate("MainWindow", "User Manual"),
            missingUserManualMarkdown(),
            QCoreApplication::translate("MainWindow", "User manual file was not found in expected locations."));
        return;
    }

    const QString manualText = loadUtf8TextFile(manualPath);
    if (manualText.trimmed().isEmpty()) {
        showMarkdownDialog(
            parent,
            QCoreApplication::translate("MainWindow", "User Manual"),
            missingUserManualMarkdown(),
            QCoreApplication::translate("MainWindow", "Failed to load `%1`.")
                .arg(QDir::toNativeSeparators(manualPath)));
        return;
    }

    showUserManualMarkdownDialog(parent,
                                 QCoreApplication::translate("MainWindow", "User Manual"),
                                 manualText);
}
} // namespace TherionStudio
