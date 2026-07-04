#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "../core/TherionFileTypes.h"

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QPainter>
#include <QPen>
#include <QPalette>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace
{

QIcon therionBadgedFileIcon(const QIcon &baseIcon)
{
    QIcon composed = baseIcon;
    const QList<int> iconSizes = {16, 18, 20, 22, 24, 32};
    for (const int size : iconSizes) {
        QPixmap pixmap = baseIcon.pixmap(size, size);
        if (pixmap.isNull()) {
            pixmap = QPixmap(size, size);
            pixmap.fill(Qt::transparent);
        }

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const int badgeWidth = qMax(9, (size / 2) + 2);
        const int badgeHeight = qMax(8, (size / 2) + 1);
        const QRect badgeRect(size - badgeWidth, size - badgeHeight, badgeWidth, badgeHeight);

        painter.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
        painter.setBrush(QColor(QStringLiteral("#0b7ac8")));
        painter.drawRoundedRect(badgeRect.adjusted(0, 0, -1, -1), 3.0, 3.0);

        QFont badgeFont = painter.font();
        badgeFont.setBold(true);
        badgeFont.setPointSizeF(qMax(5.0, badgeHeight * 0.52));
        painter.setFont(badgeFont);
        painter.setPen(Qt::white);
        painter.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("TH"));
        painter.end();

        composed.addPixmap(pixmap, QIcon::Normal, QIcon::Off);
    }

    return composed;
}

class ProjectTreeItemDelegate final : public QStyledItemDelegate
{
public:
    explicit ProjectTreeItemDelegate(const QFileSystemModel *model, QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , model_(model)
    {
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (model_ == nullptr || option == nullptr || index.column() != 0) {
            return;
        }

        if (model_->isDir(index)) {
            return;
        }

        const QString filePath = model_->filePath(index);
        if (filePath.isEmpty()) {
            return;
        }

        if (!QFileInfo(filePath).isFile() || !(TherionStudio::isTherionProjectFilePath(filePath)
                                               || TherionStudio::isThreeDViewerArtifactFilePath(filePath))) {
            return;
        }

        ensureTherionIcon(option->widget);
        option->icon = therionFileIcon_;
    }

private:
    void ensureTherionIcon(const QWidget *widget) const
    {
        if (therionIconInitialized_) {
            return;
        }

        const QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
        const QIcon baseFileIcon = style != nullptr ? style->standardIcon(QStyle::SP_FileIcon) : QIcon();
        therionFileIcon_ = therionBadgedFileIcon(baseFileIcon);
        therionIconInitialized_ = true;
    }

    const QFileSystemModel *model_ = nullptr;
    mutable bool therionIconInitialized_ = false;
    mutable QIcon therionFileIcon_;
};

QString duplicateFilePath(const QString &sourcePath)
{
    const QFileInfo sourceInfo(sourcePath);
    const QString directoryPath = sourceInfo.dir().absolutePath();
    const QString baseName = sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix();
    const QString extension = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;

    for (int counter = 1; counter < 1000; ++counter) {
        const QString candidateName = counter == 1
            ? QStringLiteral("%1 copy%2").arg(baseName, extension)
            : QStringLiteral("%1 copy %2%3").arg(baseName).arg(counter).arg(extension);
        const QString candidatePath = QDir(directoryPath).absoluteFilePath(candidateName);
        if (!QFileInfo::exists(candidatePath)) {
            return candidatePath;
        }
    }

    return QString();
}

QString ensureSuffix(const QString &name, const QString &suffix)
{
    QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QString dotSuffix = QStringLiteral(".") + suffix.toLower();
    if (!normalized.toLower().endsWith(dotSuffix)) {
        normalized += dotSuffix;
    }

    return normalized;
}

QString defaultFileBaseName(const QString &defaultName, const QString &suffix)
{
    if (suffix.isEmpty()) {
        return defaultName.trimmed();
    }

    const QString extension = QStringLiteral(".") + suffix;
    if (defaultName.endsWith(extension, Qt::CaseInsensitive)) {
        return defaultName.left(defaultName.size() - extension.size()).trimmed();
    }

    return defaultName.trimmed();
}

QString requestProjectFileBaseName(QWidget *parent,
                                   const QString &title,
                                   const QString &prompt,
                                   const QString &defaultName,
                                   const QString &suffix,
                                   bool *accepted)
{
    if (accepted != nullptr) {
        *accepted = false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel(prompt, &dialog);
    layout->addWidget(label);

    auto *row = new QWidget(&dialog);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    auto *nameEdit = new QLineEdit(row);
    nameEdit->setText(defaultFileBaseName(defaultName, suffix));
    nameEdit->selectAll();
    rowLayout->addWidget(nameEdit, 1);

    if (!suffix.isEmpty()) {
        auto *suffixLabel = new QLabel(QStringLiteral(".") + suffix, row);
        suffixLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        rowLayout->addWidget(suffixLabel, 0);
    }

    layout->addWidget(row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    nameEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }

    if (accepted != nullptr) {
        *accepted = true;
    }
    return nameEdit->text().trimmed();
}

} // namespace

void MainWindow::buildProjectBrowser()
{
    if (sidebarPages_ == nullptr || projectModel_ == nullptr) {
        return;
    }

    auto *projectPage = new QWidget;
    projectFilesPage_ = projectPage;
    projectPage->setMinimumWidth(0);
    projectPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *projectLayout = new QVBoxLayout(projectPage);
    projectLayout->setContentsMargins(0, 0, 0, 0);
    projectLayout->setSpacing(8);

    projectFilesEmptyState_ = new QWidget(projectPage);
    auto *emptyLayout = new QVBoxLayout(projectFilesEmptyState_);
    emptyLayout->setContentsMargins(0, 24, 0, 0);
    emptyLayout->setSpacing(8);

    auto *emptyTitle = new QLabel(tr("No project open."), projectFilesEmptyState_);
    QFont emptyTitleFont = emptyTitle->font();
    emptyTitleFont.setBold(true);
    emptyTitle->setFont(emptyTitleFont);
    emptyTitle->setWordWrap(true);
    emptyLayout->addWidget(emptyTitle);

    auto *emptyMessage = new QLabel(tr("Open a project to browse its files."), projectFilesEmptyState_);
    emptyMessage->setWordWrap(true);
    emptyLayout->addWidget(emptyMessage);

    projectFilesOpenProjectButton_ = new QPushButton(tr("Open Project..."), projectFilesEmptyState_);
    projectFilesOpenProjectButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(projectFilesOpenProjectButton_, &QPushButton::clicked, this, &MainWindow::openProject);
    emptyLayout->addWidget(projectFilesOpenProjectButton_);
    emptyLayout->addStretch(1);
    projectLayout->addWidget(projectFilesEmptyState_, 1);

    projectTree_ = new QTreeView(projectPage);
    projectTree_->setMinimumWidth(0);
    projectTree_->setModel(projectModel_);
    projectTree_->setRootIsDecorated(true);
    projectTree_->setAnimated(true);
    projectTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    projectTree_->setSelectionMode(QAbstractItemView::SingleSelection);
    projectTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    projectTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    projectTree_->setAlternatingRowColors(true);
    projectTree_->hideColumn(1);
    projectTree_->hideColumn(2);
    projectTree_->hideColumn(3);
    projectTree_->header()->hide();
    projectTree_->setItemDelegateForColumn(0, new ProjectTreeItemDelegate(projectModel_, projectTree_));
    connect(projectTree_, &QTreeView::activated, this, &MainWindow::handleProjectTreeActivated);
    connect(projectTree_, &QTreeView::customContextMenuRequested, this, &MainWindow::handleProjectTreeContextMenuRequested);

    projectLayout->addWidget(projectTree_, 1);

    auto *fileBrowserPlaceholder = new QWidget(sidebarPages_);
    fileBrowserPlaceholder->setObjectName(QStringLiteral("fileBrowserSidebarPlaceholder"));
    sidebarPages_->addWidget(fileBrowserPlaceholder);

    resetProjectBrowser();
}

void MainWindow::refreshProjectBrowserView(const QString &focusPath, bool forceReload)
{
    if (projectModel_ == nullptr || projectTree_ == nullptr) {
        return;
    }

    const bool hasOpenProject = !projectRootPath_.trimmed().isEmpty() && QDir(projectRootPath_).exists();
    if (projectFilesEmptyState_ != nullptr) {
        projectFilesEmptyState_->setVisible(!hasOpenProject);
    }
    projectTree_->setVisible(hasOpenProject);

    if (!hasOpenProject) {
        projectTree_->setRootIndex(QModelIndex());
        return;
    }

    const QString rootPath = projectRootPath_;
    if (forceReload) {
        auto *previousModel = projectModel_;
        QAbstractItemDelegate *previousNameDelegate = projectTree_->itemDelegateForColumn(0);
        projectModel_ = new QFileSystemModel(this);
        projectTree_->setModel(projectModel_);
        projectTree_->hideColumn(1);
        projectTree_->hideColumn(2);
        projectTree_->hideColumn(3);
        projectTree_->setItemDelegateForColumn(0, new ProjectTreeItemDelegate(projectModel_, projectTree_));
        if (previousNameDelegate != nullptr && previousNameDelegate->parent() == projectTree_) {
            previousNameDelegate->deleteLater();
        }
        previousModel->deleteLater();
    }
    projectModel_->setRootPath(rootPath);
    projectTree_->setRootIndex(projectModel_->index(rootPath));

    const QString normalizedFocusPath = focusPath.trimmed();
    if (normalizedFocusPath.isEmpty()) {
        return;
    }

    const QString absoluteFocusPath = QFileInfo(normalizedFocusPath).absoluteFilePath();
    QTimer::singleShot(0, this, [this, absoluteFocusPath]() {
        if (projectModel_ == nullptr || projectTree_ == nullptr) {
            return;
        }

        QModelIndex targetIndex = projectModel_->index(absoluteFocusPath);
        if (!targetIndex.isValid()) {
            return;
        }

        QModelIndex parentIndex = targetIndex.parent();
        while (parentIndex.isValid() && parentIndex != projectTree_->rootIndex()) {
            projectTree_->expand(parentIndex);
            parentIndex = parentIndex.parent();
        }

        projectTree_->scrollTo(targetIndex);
        projectTree_->setCurrentIndex(targetIndex);
    });
}

void MainWindow::handleProjectTreeContextMenuRequested(const QPoint &position)
{
    if (projectTree_ == nullptr || projectModel_ == nullptr) {
        return;
    }

    const QModelIndex index = projectTree_->indexAt(position);
    const QString itemPath = index.isValid() ? projectModel_->filePath(index) : QString();
    const QFileInfo itemInfo(itemPath);
    const bool hasItem = index.isValid() && !itemPath.isEmpty() && itemInfo.exists();

    QString creationDirectory = projectRootPath_;
    if (creationDirectory.isEmpty() && hasItem) {
        creationDirectory = itemInfo.isDir() ? itemInfo.absoluteFilePath() : itemInfo.absolutePath();
    } else if (hasItem) {
        creationDirectory = itemInfo.isDir() ? itemInfo.absoluteFilePath() : itemInfo.absolutePath();
    }
    if (creationDirectory.isEmpty() || !QDir(creationDirectory).exists()) {
        creationDirectory = projectRootPath_;
    }

    QMenu menu(projectTree_);

    auto openTabsForPathPrefix = [this](const QString &pathPrefix) {
        QVector<int> indices;
        if (pathPrefix.isEmpty()) {
            return indices;
        }

        const QString canonicalPrefix = QFileInfo(pathPrefix).canonicalFilePath().isEmpty()
            ? QFileInfo(pathPrefix).absoluteFilePath()
            : QFileInfo(pathPrefix).canonicalFilePath();
        const QString normalizedPrefix = canonicalPrefix.endsWith(QLatin1Char('/'))
            ? canonicalPrefix
            : canonicalPrefix + QLatin1Char('/');

        for (int tabIndex = 0; tabIndex < editorTabs_->count(); ++tabIndex) {
            QWidget *tabWidget = editorTabs_->widget(tabIndex);
            const QString documentPath = documentPathForWidget(tabWidget);
            if (documentPath.isEmpty()) {
                continue;
            }

            const QString canonicalDocumentPath = QFileInfo(documentPath).canonicalFilePath().isEmpty()
                ? QFileInfo(documentPath).absoluteFilePath()
                : QFileInfo(documentPath).canonicalFilePath();
            if (canonicalDocumentPath == canonicalPrefix || canonicalDocumentPath.startsWith(normalizedPrefix)) {
                indices.append(tabIndex);
            }
        }

        return indices;
    };

    auto warnOpenTabs = [this, &openTabsForPathPrefix](const QString &path) {
        const QVector<int> openTabs = openTabsForPathPrefix(path);
        if (openTabs.isEmpty()) {
            return false;
        }

        QMessageBox::information(this,
                                 tr("Project Browser"),
                                 tr("Close open tabs for the selected path before renaming it or deleting a folder."));
        return true;
    };

    auto refreshAfterProjectFileMutation = [this](const QString &focusPath, const QString &previousPath = QString()) {
        refreshProjectBrowserView(focusPath, true);
        handleProjectFileSystemMutation(focusPath, previousPath);
    };

    auto createFileAction = [this, creationDirectory, &menu, &refreshAfterProjectFileMutation](const QString &label,
                                                                                               const QString &prompt,
                                                                                               const QString &defaultName,
                                                                                               const QString &suffix) {
        if (creationDirectory.isEmpty()) {
            return;
        }

        menu.addAction(label, this, [this, creationDirectory, prompt, defaultName, suffix, &refreshAfterProjectFileMutation]() {
            bool ok = false;
            QString enteredName = requestProjectFileBaseName(this,
                                                             tr("Create File"),
                                                             prompt,
                                                             defaultName,
                                                             suffix,
                                                             &ok);
            if (!ok) {
                return;
            }
            if (enteredName.isEmpty()) {
                QMessageBox::warning(this, tr("Create File"), tr("File name cannot be empty."));
                return;
            }

            if (!suffix.isEmpty()) {
                enteredName = ensureSuffix(enteredName, suffix);
            }

            const QString filePath = QDir(creationDirectory).absoluteFilePath(enteredName);
            if (QFileInfo::exists(filePath)) {
                QMessageBox::warning(this, tr("Create File"), tr("File already exists: %1").arg(QDir::toNativeSeparators(filePath)));
                return;
            }

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QMessageBox::warning(this,
                                     tr("Create File"),
                                     tr("Failed to create file: %1").arg(QDir::toNativeSeparators(filePath)));
                return;
            }
            const QByteArray initialContents = TherionStudio::initialTherionProjectFileContents(filePath);
            if (!initialContents.isEmpty()) {
                if (file.write(initialContents) != initialContents.size()) {
                    file.close();
                    QMessageBox::warning(this,
                                         tr("Create File"),
                                         tr("Failed to initialize file: %1").arg(QDir::toNativeSeparators(filePath)));
                    QFile::remove(filePath);
                    return;
                }
            }
            file.close();

            refreshAfterProjectFileMutation(filePath);
            statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(filePath)), 3000);
            appendConsoleLine(tr("Created %1").arg(filePath));
        });
    };

    if (hasItem && itemInfo.isFile()) {
        menu.addAction(tr("Open"), this, [this, index]() {
            handleProjectTreeActivated(index);
        });

        if (itemInfo.suffix().compare(QStringLiteral("th2"), Qt::CaseInsensitive) == 0) {
            menu.addAction(tr("Open in Map Editor"), this, [this, itemPath]() {
                openMapEditorTab(itemPath);
            });
        }

        if (!itemInfo.isFile()
            || !(TherionStudio::isTherionProjectFilePath(itemPath)
                 || TherionStudio::isThreeDViewerArtifactFilePath(itemPath))) {
            menu.addAction(tr("Open Externally"), this, [this, itemPath]() {
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(itemPath))) {
                    QMessageBox::warning(this,
                                         tr("Open Externally"),
                                         tr("Failed to open %1 with the system default application.")
                                             .arg(QDir::toNativeSeparators(itemPath)));
                }
            });
        }

        menu.addSeparator();
        menu.addAction(tr("Duplicate"), this, [this, itemPath, &refreshAfterProjectFileMutation]() {
            const QString targetPath = duplicateFilePath(itemPath);
            if (targetPath.isEmpty()) {
                QMessageBox::warning(this, tr("Duplicate"), tr("Could not resolve a duplicate file name."));
                return;
            }

            if (!QFile::copy(itemPath, targetPath)) {
                QMessageBox::warning(this,
                                     tr("Duplicate"),
                                     tr("Failed to duplicate file to %1.").arg(QDir::toNativeSeparators(targetPath)));
                return;
            }

            statusBar()->showMessage(tr("Duplicated %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Duplicated %1 -> %2").arg(itemPath, targetPath));
            refreshAfterProjectFileMutation(targetPath);
        });

        menu.addAction(tr("Rename"), this, [this, itemPath, &warnOpenTabs, &refreshAfterProjectFileMutation]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const QFileInfo currentInfo(itemPath);
            bool ok = false;
            const QString newName = QInputDialog::getText(this,
                                                          tr("Rename"),
                                                          tr("New name:"),
                                                          QLineEdit::Normal,
                                                          currentInfo.fileName(),
                                                          &ok).trimmed();
            if (!ok) {
                return;
            }
            if (newName.isEmpty()) {
                QMessageBox::warning(this, tr("Rename"), tr("Name cannot be empty."));
                return;
            }
            if (newName == currentInfo.fileName()) {
                return;
            }

            const QString renamedPath = currentInfo.dir().absoluteFilePath(newName);
            if (QFileInfo::exists(renamedPath)) {
                QMessageBox::warning(this, tr("Rename"), tr("Target already exists: %1").arg(QDir::toNativeSeparators(renamedPath)));
                return;
            }

            if (!QFile::rename(itemPath, renamedPath)) {
                QMessageBox::warning(this, tr("Rename"), tr("Failed to rename file."));
                return;
            }

            statusBar()->showMessage(tr("Renamed to %1").arg(QDir::toNativeSeparators(renamedPath)), 3000);
            appendConsoleLine(tr("Renamed %1 -> %2").arg(itemPath, renamedPath));
            refreshAfterProjectFileMutation(renamedPath, itemPath);
        });

        menu.addAction(tr("Delete"), this, [this, itemPath, &refreshAfterProjectFileMutation]() {
            const auto answer = QMessageBox::question(this,
                                                      tr("Delete File"),
                                                      tr("Delete %1?\n\nIf the file is open, it will be closed first.")
                                                          .arg(QDir::toNativeSeparators(itemPath)),
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }

            if (!closeOpenDocumentForFilePath(itemPath)) {
                return;
            }

            if (!QFile::remove(itemPath)) {
                QMessageBox::warning(this, tr("Delete File"), tr("Failed to delete file."));
                return;
            }

            statusBar()->showMessage(tr("Deleted %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Deleted %1").arg(itemPath));
            refreshAfterProjectFileMutation(QFileInfo(itemPath).absolutePath(), itemPath);
        });
    } else if (hasItem && itemInfo.isDir()) {
        menu.addAction(tr("Rename Folder"), this, [this, itemPath, &warnOpenTabs, &refreshAfterProjectFileMutation]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const QFileInfo currentInfo(itemPath);
            bool ok = false;
            const QString newName = QInputDialog::getText(this,
                                                          tr("Rename Folder"),
                                                          tr("New folder name:"),
                                                          QLineEdit::Normal,
                                                          currentInfo.fileName(),
                                                          &ok).trimmed();
            if (!ok) {
                return;
            }
            if (newName.isEmpty()) {
                QMessageBox::warning(this, tr("Rename Folder"), tr("Folder name cannot be empty."));
                return;
            }
            if (newName == currentInfo.fileName()) {
                return;
            }

            const QString renamedPath = currentInfo.dir().absoluteFilePath(newName);
            if (QFileInfo::exists(renamedPath)) {
                QMessageBox::warning(this, tr("Rename Folder"), tr("Target already exists: %1").arg(QDir::toNativeSeparators(renamedPath)));
                return;
            }

            if (!QDir().rename(itemPath, renamedPath)) {
                QMessageBox::warning(this, tr("Rename Folder"), tr("Failed to rename folder."));
                return;
            }

            statusBar()->showMessage(tr("Renamed folder to %1").arg(QDir::toNativeSeparators(renamedPath)), 3000);
            appendConsoleLine(tr("Renamed folder %1 -> %2").arg(itemPath, renamedPath));
            refreshAfterProjectFileMutation(renamedPath, itemPath);
        });

        menu.addAction(tr("Delete Folder"), this, [this, itemPath, &warnOpenTabs, &refreshAfterProjectFileMutation]() {
            if (warnOpenTabs(itemPath)) {
                return;
            }

            const auto answer = QMessageBox::question(this,
                                                      tr("Delete Folder"),
                                                      tr("Delete folder %1 and all its contents?")
                                                          .arg(QDir::toNativeSeparators(itemPath)),
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);
            if (answer != QMessageBox::Yes) {
                return;
            }

            QDir directory(itemPath);
            if (!directory.removeRecursively()) {
                QMessageBox::warning(this, tr("Delete Folder"), tr("Failed to delete folder."));
                return;
            }

            statusBar()->showMessage(tr("Deleted folder %1").arg(QDir::toNativeSeparators(itemPath)), 3000);
            appendConsoleLine(tr("Deleted folder %1").arg(itemPath));
            refreshAfterProjectFileMutation(QFileInfo(itemPath).absolutePath(), itemPath);
        });
    }

    if (!creationDirectory.isEmpty() && QDir(creationDirectory).exists()) {
        if (!menu.isEmpty()) {
            menu.addSeparator();
        }

        menu.addAction(tr("New Folder"), this, [this, creationDirectory, &refreshAfterProjectFileMutation]() {
            bool ok = false;
            const QString folderName = QInputDialog::getText(this,
                                                             tr("Create Folder"),
                                                             tr("Folder name:"),
                                                             QLineEdit::Normal,
                                                             tr("New Folder"),
                                                             &ok).trimmed();
            if (!ok) {
                return;
            }
            if (folderName.isEmpty()) {
                QMessageBox::warning(this, tr("Create Folder"), tr("Folder name cannot be empty."));
                return;
            }

            const QString folderPath = QDir(creationDirectory).absoluteFilePath(folderName);
            if (QFileInfo::exists(folderPath)) {
                QMessageBox::warning(this, tr("Create Folder"), tr("Folder already exists: %1").arg(QDir::toNativeSeparators(folderPath)));
                return;
            }

            if (!QDir().mkpath(folderPath)) {
                QMessageBox::warning(this, tr("Create Folder"), tr("Failed to create folder."));
                return;
            }

            statusBar()->showMessage(tr("Created folder %1").arg(QDir::toNativeSeparators(folderPath)), 3000);
            appendConsoleLine(tr("Created folder %1").arg(folderPath));
            refreshAfterProjectFileMutation(folderPath);
        });

        createFileAction(tr("New .th File"), tr("File name:"), tr("new-file.th"), QStringLiteral("th"));
        createFileAction(tr("New .th2 File"), tr("File name:"), tr("new-map.th2"), QStringLiteral("th2"));
        createFileAction(tr("New .thconfig File"), tr("File name:"), tr("new-config.thconfig"), QStringLiteral("thconfig"));
    }

    if (menu.isEmpty()) {
        return;
    }

    menu.exec(projectTree_->viewport()->mapToGlobal(position));
}
