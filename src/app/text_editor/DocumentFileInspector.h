#pragma once

#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QEvent;
class QPushButton;
class QVBoxLayout;

namespace TherionStudio
{

struct DocumentFileInspectorContext
{
    std::function<QString()> filePath;
    std::function<QString()> encodingName;
    std::function<QString()> encodingLabel;
    std::function<void()> convertToUtf8;
};

class DocumentFileInspector final : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentFileInspector(DocumentFileInspectorContext context, QWidget *parent = nullptr);

    void refresh();
    void setBackupWidget(QWidget *widget);

protected:
    void changeEvent(QEvent *event) override;

private:
    void addMetadataRow(QVBoxLayout *layout, const QString &labelText, QWidget *valueWidget);
    void updateCopyPathButtonIcon();
    void updateFileTitle(const QString &currentPath);
    QString filePath() const;
    QString encodingName() const;
    QString encodingLabel() const;
    bool canConvertToUtf8() const;

    DocumentFileInspectorContext context_;
    QLabel *fileTitleLabel_ = nullptr;
    QLineEdit *pathEdit_ = nullptr;
    QPushButton *copyPathButton_ = nullptr;
    QLabel *sizeValueLabel_ = nullptr;
    QLabel *modifiedValueLabel_ = nullptr;
    QLabel *encodingValueLabel_ = nullptr;
    QLabel *encodingWarningLabel_ = nullptr;
    QPushButton *convertEncodingButton_ = nullptr;
};

}
