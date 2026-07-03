#include "MainWindowWelcomeWidget.h"

#include "MainWindowRecentFilesService.h"
#include "MainWindowRecentProjectsService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

namespace TherionStudio
{
namespace
{
QLabel *createRecentItemLink(QWidget *parent,
                             const QString &displayText,
                             const QString &toolTip,
                             const QString &targetPath,
                             std::function<void(const QString &)> onActivated)
{
    auto *link = new QLabel(parent);
    link->setText(QStringLiteral("<a href=\"open\">%1</a>").arg(displayText.toHtmlEscaped()));
    link->setTextFormat(Qt::RichText);
    link->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    link->setOpenExternalLinks(false);
    link->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    link->setToolTip(toolTip);
    QObject::connect(link, &QLabel::linkActivated, link, [onActivated, targetPath](const QString &) {
        if (onActivated) {
            onActivated(targetPath);
        }
    });
    return link;
}
}

QWidget *createMainWindowProjectWelcomeWidget(const QString &title,
                                              const QString &body,
                                              const QString &buttonText,
                                              std::function<void()> onButtonClick,
                                              const QString &emptyProjectButtonText,
                                              std::function<void()> onEmptyProjectButtonClick,
                                              const QString &templateButtonText,
                                              std::function<void()> onTemplateButtonClick,
                                              const QString &userManualButtonText,
                                              std::function<void()> onUserManualButtonClick,
                                              const QStringList &recentProjectPaths,
                                              std::function<void(const QString &)> onRecentProjectClick)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *bodyLabel = new QLabel(body);
    bodyLabel->setWordWrap(true);

    layout->addStretch(1);
    layout->addWidget(titleLabel);
    layout->addWidget(bodyLabel);

    auto *buttonRow = new QWidget(widget);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);

    auto *emptyProjectButton = new QPushButton(emptyProjectButtonText, buttonRow);
    emptyProjectButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QObject::connect(emptyProjectButton, &QPushButton::clicked, widget, [onEmptyProjectButtonClick]() {
        if (onEmptyProjectButtonClick) {
            onEmptyProjectButtonClick();
        }
    });
    buttonLayout->addWidget(emptyProjectButton);

    auto *templateButton = new QPushButton(templateButtonText, buttonRow);
    templateButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QObject::connect(templateButton, &QPushButton::clicked, widget, [onTemplateButtonClick]() {
        if (onTemplateButtonClick) {
            onTemplateButtonClick();
        }
    });
    buttonLayout->addWidget(templateButton);

    auto *button = new QPushButton(buttonText, buttonRow);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QObject::connect(button, &QPushButton::clicked, widget, [onButtonClick]() {
        if (onButtonClick) {
            onButtonClick();
        }
    });
    buttonLayout->addWidget(button);

    auto *manualButton = new QPushButton(userManualButtonText, buttonRow);
    manualButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QObject::connect(manualButton, &QPushButton::clicked, widget, [onUserManualButtonClick]() {
        if (onUserManualButtonClick) {
            onUserManualButtonClick();
        }
    });
    buttonLayout->addWidget(manualButton);
    buttonLayout->addStretch(1);
    layout->addWidget(buttonRow);

    const QStringList normalizedRecentProjectPaths =
        MainWindowRecentProjectsService::normalizedRecentProjectPaths(recentProjectPaths);
    if (!normalizedRecentProjectPaths.isEmpty()) {
        auto *recentTitleLabel =
            new QLabel(QCoreApplication::translate("MainWindow", "Recent Projects"));
        QFont recentTitleFont = recentTitleLabel->font();
        recentTitleFont.setBold(true);
        recentTitleLabel->setFont(recentTitleFont);
        layout->addSpacing(16);
        layout->addWidget(recentTitleLabel);

        for (const QString &projectPath : normalizedRecentProjectPaths) {
            layout->addWidget(createRecentItemLink(widget,
                                                   QDir::toNativeSeparators(projectPath),
                                                   QDir::toNativeSeparators(projectPath),
                                                   projectPath,
                                                   onRecentProjectClick));
        }
    }

    layout->addStretch(1);

    return widget;
}

QWidget *createMainWindowActiveProjectWelcomeWidget(const QString &title,
                                                    const QString &projectPath,
                                                    const QString &body,
                                                    const QString &userManualButtonText,
                                                    std::function<void()> onUserManualButtonClick,
                                                    const QStringList &recentFilePaths,
                                                    std::function<void(const QString &)> onRecentFileClick)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    auto *projectLabel =
        new QLabel(QCoreApplication::translate("MainWindow", "Project: %1")
                       .arg(QFileInfo(projectPath).fileName()));
    QFont projectFont = projectLabel->font();
    projectFont.setBold(true);
    projectLabel->setFont(projectFont);

    auto *projectPathLabel = new QLabel(QDir::toNativeSeparators(projectPath));
    projectPathLabel->setWordWrap(true);
    projectPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *bodyLabel = new QLabel(body);
    bodyLabel->setWordWrap(true);

    layout->addStretch(1);
    layout->addWidget(titleLabel);
    layout->addWidget(projectLabel);
    layout->addWidget(projectPathLabel);
    layout->addWidget(bodyLabel);

    auto *manualButton = new QPushButton(userManualButtonText, widget);
    manualButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QObject::connect(manualButton, &QPushButton::clicked, widget, [onUserManualButtonClick]() {
        if (onUserManualButtonClick) {
            onUserManualButtonClick();
        }
    });
    layout->addWidget(manualButton);

    const QStringList normalizedRecentFilePaths =
        MainWindowRecentFilesService::normalizedRecentFilePaths(projectPath, recentFilePaths);
    auto *recentTitleLabel =
        new QLabel(QCoreApplication::translate("MainWindow", "Recent Files"));
    QFont recentTitleFont = recentTitleLabel->font();
    recentTitleFont.setBold(true);
    recentTitleLabel->setFont(recentTitleFont);
    layout->addSpacing(16);
    layout->addWidget(recentTitleLabel);

    if (normalizedRecentFilePaths.isEmpty()) {
        auto *emptyLabel = new QLabel(QCoreApplication::translate("MainWindow", "No recent files yet."));
        layout->addWidget(emptyLabel);
    } else {
        for (const QString &filePath : normalizedRecentFilePaths) {
            layout->addWidget(createRecentItemLink(
                widget,
                MainWindowRecentFilesService::projectRelativeDisplayPath(projectPath, filePath),
                QDir::toNativeSeparators(filePath),
                filePath,
                onRecentFileClick));
        }
    }

    layout->addStretch(1);

    return widget;
}
}
