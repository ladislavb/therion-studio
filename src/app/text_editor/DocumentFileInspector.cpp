#include "DocumentFileInspector.h"

#include "InspectorPanel.h"
#include "../LucideIconFactory.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QDir>
#include <QSize>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <utility>

namespace TherionStudio
{

DocumentFileInspector::DocumentFileInspector(DocumentFileInspectorContext context, QWidget *parent)
    : QWidget(parent)
    , context_(std::move(context))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QVBoxLayout *fileSectionLayout = nullptr;
    auto *fileSection = InspectorPanel::createSection(this, tr("File"), &fileSectionLayout, &fileTitleLabel_);

    auto *pathValueWidget = new QWidget(fileSection);
    auto *pathValueLayout = new QHBoxLayout(pathValueWidget);
    pathValueLayout->setContentsMargins(0, 0, 0, 0);
    pathValueLayout->setSpacing(4);

    pathEdit_ = new QLineEdit(pathValueWidget);
    pathEdit_->setReadOnly(true);
    pathEdit_->setMinimumWidth(0);
    pathValueLayout->addWidget(pathEdit_, 1);

    copyPathButton_ = new QPushButton(pathValueWidget);
    copyPathButton_->setAutoDefault(false);
    copyPathButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    copyPathButton_->setMinimumWidth(copyPathButton_->sizeHint().height());
    copyPathButton_->setIconSize(QSize(16, 16));
    copyPathButton_->setToolTip(tr("Copy path"));
    updateCopyPathButtonIcon();
    connect(copyPathButton_, &QPushButton::clicked, this, [this]() {
        const QString currentPath = filePath();
        if (currentPath.isEmpty()) {
            return;
        }
        QApplication::clipboard()->setText(QFileInfo(currentPath).absoluteFilePath());
    });
    pathValueLayout->addWidget(copyPathButton_);
    addMetadataRow(fileSectionLayout, tr("Path"), pathValueWidget);

    sizeValueLabel_ = new QLabel(fileSection);
    sizeValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addMetadataRow(fileSectionLayout, tr("Size"), sizeValueLabel_);

    modifiedValueLabel_ = new QLabel(fileSection);
    modifiedValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addMetadataRow(fileSectionLayout, tr("Last modified"), modifiedValueLabel_);

    encodingValueLabel_ = new QLabel(fileSection);
    encodingValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addMetadataRow(fileSectionLayout, tr("Encoding"), encodingValueLabel_);

    encodingWarningLabel_ = new QLabel(fileSection);
    encodingWarningLabel_->setWordWrap(true);
    encodingWarningLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    encodingWarningLabel_->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
    fileSectionLayout->addWidget(encodingWarningLabel_);

    convertEncodingButton_ = new QPushButton(tr("Convert to UTF-8"), fileSection);
    convertEncodingButton_->setAutoDefault(false);
    convertEncodingButton_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(convertEncodingButton_, &QPushButton::clicked, this, [this]() {
        if (context_.convertToUtf8) {
            context_.convertToUtf8();
            refresh();
        }
    });
    fileSectionLayout->addWidget(convertEncodingButton_);

    layout->addWidget(fileSection);
    layout->addStretch(1);
    refresh();
}

void DocumentFileInspector::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)) {
        updateCopyPathButtonIcon();
    }
}

void DocumentFileInspector::setBackupWidget(QWidget *widget)
{
    if (widget != nullptr) {
        auto *content = qobject_cast<QVBoxLayout *>(layout());
        content->insertWidget(content->count() - 1, widget);
    }
}

void DocumentFileInspector::addMetadataRow(QVBoxLayout *layout, const QString &labelText, QWidget *valueWidget)
{
    if (layout == nullptr || valueWidget == nullptr) {
        return;
    }

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    auto *label = new QLabel(labelText, this);
    label->setMinimumWidth(96);
    row->addWidget(label);
    row->addWidget(valueWidget, 1);
    layout->addLayout(row);
}

void DocumentFileInspector::updateCopyPathButtonIcon()
{
    if (copyPathButton_ == nullptr) {
        return;
    }
    copyPathButton_->setIcon(themedLucideIcon(QStringLiteral("copy"),
                                              copyPathButton_->palette(),
                                              copyPathButton_->iconSize().width(),
                                              copyPathButton_->devicePixelRatioF()));
}

void DocumentFileInspector::updateFileTitle(const QString &currentPath)
{
    if (fileTitleLabel_ == nullptr) {
        return;
    }

    const QString fileName = currentPath.isEmpty() ? QString() : QFileInfo(currentPath).fileName();
    fileTitleLabel_->setText(fileName.isEmpty() ? tr("File") : fileName);
}

void DocumentFileInspector::refresh()
{
    const QString currentPath = filePath();
    const QFileInfo info(currentPath);
    updateFileTitle(currentPath);
    if (pathEdit_ != nullptr) {
        const QString displayedPath = currentPath.isEmpty()
            ? tr("Not saved")
            : QDir::toNativeSeparators(QFileInfo(currentPath).absoluteFilePath());
        pathEdit_->setText(displayedPath);
        pathEdit_->setToolTip(displayedPath);
    }
    if (copyPathButton_ != nullptr) {
        copyPathButton_->setEnabled(!currentPath.isEmpty());
    }

    if (sizeValueLabel_ != nullptr) {
        if (currentPath.isEmpty() || !info.exists()) {
            sizeValueLabel_->setText(tr("Not saved"));
        } else {
            sizeValueLabel_->setText(QLocale().formattedDataSize(info.size()));
        }
    }

    if (modifiedValueLabel_ != nullptr) {
        if (currentPath.isEmpty() || !info.exists()) {
            modifiedValueLabel_->setText(tr("Not saved"));
        } else {
            modifiedValueLabel_->setText(QLocale().toString(info.lastModified(), QLocale::ShortFormat));
        }
    }

    const QString label = encodingLabel().trimmed().isEmpty() ? QStringLiteral("UTF-8") : encodingLabel().trimmed();
    if (encodingValueLabel_ != nullptr) {
        encodingValueLabel_->setText(label);
    }

    const bool canConvert = canConvertToUtf8();
    if (encodingWarningLabel_ != nullptr) {
        encodingWarningLabel_->setVisible(canConvert);
        encodingWarningLabel_->setText(canConvert
            ? tr("This file is opened as %1. Saving keeps this encoding unless you convert it to UTF-8.").arg(label)
            : QString());
    }
    if (convertEncodingButton_ != nullptr) {
        convertEncodingButton_->setVisible(canConvert);
        convertEncodingButton_->setEnabled(canConvert);
    }
}

QString DocumentFileInspector::filePath() const
{
    return context_.filePath ? context_.filePath() : QString();
}

QString DocumentFileInspector::encodingName() const
{
    return context_.encodingName ? context_.encodingName() : QStringLiteral("UTF-8");
}

QString DocumentFileInspector::encodingLabel() const
{
    return context_.encodingLabel ? context_.encodingLabel() : encodingName();
}

bool DocumentFileInspector::canConvertToUtf8() const
{
    return encodingName().compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0;
}

}
