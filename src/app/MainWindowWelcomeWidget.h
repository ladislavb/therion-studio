#pragma once

#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

namespace TherionStudio
{
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
                                              std::function<void(const QString &)> onRecentProjectClick);

QWidget *createMainWindowActiveProjectWelcomeWidget(const QString &title,
                                                    const QString &projectPath,
                                                    const QString &body,
                                                    const QString &userManualButtonText,
                                                    std::function<void()> onUserManualButtonClick,
                                                    const QStringList &recentFilePaths,
                                                    std::function<void(const QString &)> onRecentFileClick);
}
