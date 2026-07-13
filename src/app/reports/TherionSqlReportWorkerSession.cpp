#include "TherionSqlReportWorkerSession.h"

#include "TherionSqlReportWorker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QThread>

#include <utility>

namespace TherionStudio
{

TherionSqlReportWorkerSession::TherionSqlReportWorkerSession(QObject *parent)
    : TherionSqlReportWorkerSession({}, parent)
{
}

TherionSqlReportWorkerSession::TherionSqlReportWorkerSession(
    TherionSqlReportDatabase::ConnectionLifecycleObserver lifecycleObserver,
    QObject *parent)
    : TherionSqlReportSession(parent)
    , workerThread_(new QThread)
    , worker_(new TherionSqlReportWorker(std::move(lifecycleObserver)))
{
    worker_->moveToThread(workerThread_);
    connect(worker_, &TherionSqlReportWorker::importFinished,
            this, &TherionSqlReportSession::importFinished);
    connect(worker_, &TherionSqlReportWorker::queryFinished,
            this, &TherionSqlReportSession::queryFinished);
    connect(worker_, &QObject::destroyed, workerThread_, &QThread::quit, Qt::DirectConnection);
    connect(workerThread_, &QThread::finished, workerThread_, &QObject::deleteLater);
    workerThread_->start();
}

TherionSqlReportWorkerSession::~TherionSqlReportWorkerSession()
{
    shutdown();
}

bool TherionSqlReportWorkerSession::requestImport(const TherionSqlReportImportRequest &request,
                                                   QString *errorMessage)
{
    if (shuttingDown_ || worker_ == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "TherionStudio::TherionSqlReportWorkerSession",
                "The SQL report session is closing.");
        }
        return false;
    }

    const QFileInfo inputInfo(request.filePath);
    QFile inputFile(request.filePath);
    if (!inputInfo.isFile() || !inputFile.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                                "TherionStudio::TherionSqlReportWorkerSession",
                                "Could not read SQL export %1.")
                                .arg(QDir::toNativeSeparators(request.filePath));
        }
        return false;
    }
    inputFile.close();

    QPointer<TherionSqlReportWorker> worker(worker_);
    const bool accepted = QMetaObject::invokeMethod(worker_,
                                                     [worker, request]() {
                                                         if (worker != nullptr) {
                                                             worker->importDatabase(request);
                                                         }
                                                     },
                                                     Qt::QueuedConnection);
    if (!accepted && errorMessage != nullptr) {
        *errorMessage = QCoreApplication::translate(
            "TherionStudio::TherionSqlReportWorkerSession",
            "The SQL report session is closing.");
    }
    return accepted;
}

void TherionSqlReportWorkerSession::requestQuery(const TherionSqlReportQueryRequest &request)
{
    if (shuttingDown_ || worker_ == nullptr) {
        return;
    }
    QPointer<TherionSqlReportWorker> worker(worker_);
    QMetaObject::invokeMethod(worker_,
                              [worker, request]() {
                                  if (worker != nullptr) {
                                      worker->executeQuery(request);
                                  }
                              },
                              Qt::QueuedConnection);
}

void TherionSqlReportWorkerSession::shutdown()
{
    if (shuttingDown_) {
        return;
    }
    shuttingDown_ = true;
    disconnect(worker_, nullptr, this, nullptr);

    QPointer<TherionSqlReportWorker> worker(worker_);
    if (worker_ != nullptr) {
        QMetaObject::invokeMethod(worker_,
                                  [worker]() {
                                      if (worker != nullptr) {
                                          worker->shutdown();
                                          worker->deleteLater();
                                      }
                                  },
                                  Qt::QueuedConnection);
    } else if (workerThread_ != nullptr) {
        workerThread_->quit();
    }
    worker_ = nullptr;
    workerThread_ = nullptr;
}

} // namespace TherionStudio
