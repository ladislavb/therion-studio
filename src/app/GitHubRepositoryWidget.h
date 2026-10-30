#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QTimer;
class QCheckBox;
class QShowEvent;

namespace TherionStudio
{
class GitHubRepositoryWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit GitHubRepositoryWidget(const QString &projectRootPath, QWidget *parent = nullptr,
                                    const QString &executable = {});
    ~GitHubRepositoryWidget() override;

protected:
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    enum class Operation { None, Login, Account, Create, Verify };
    void run(Operation operation, const QStringList &arguments);
    void finished(int exitCode);
    void fail(const QString &message);
    void updateControls();
    void createRepository();
    void loadConfiguration();
    bool saveConfiguration();
    bool readConfiguration(QByteArray &contents, bool &exists);
    bool configurationError(const QString &message);

    QString executable_;
    QString projectRootPath_;
    QString repository_;
    bool projectOpen_ = false;
    bool configurationAvailable_ = false;
    bool configurationDirty_ = false;
    bool configurationExisted_ = false;
    QByteArray configurationContents_;
    bool authenticated_ = false;
    bool autoChecked_ = false;
    bool timedOut_ = false;
    Operation operation_ = Operation::None;
    QProcess *process_;
    QTimer *timeout_;
    QTimer *saveTimer_;
    QLineEdit *owner_;
    QLineEdit *name_;
    QPushButton *login_;
    QPushButton *check_;
    QPushButton *create_;
    QPushButton *verify_;
    QLabel *status_;
    QLabel *configurationStatus_;
    QCheckBox *uploadEnabled_;
    QCheckBox *promptOnClose_;
    QPlainTextEdit *loginOutput_;
};
}
