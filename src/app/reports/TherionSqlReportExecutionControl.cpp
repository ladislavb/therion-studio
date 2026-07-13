#include "TherionSqlReportExecutionControl.h"

#ifdef Q_OS_WIN
#include <winsqlite/winsqlite3.h>
#else
#include <sqlite3.h>
#endif

#include <chrono>

namespace TherionStudio
{

void TherionSqlReportExecutionControl::acceptRequest(quint64 requestId)
{
    quint64 acceptedRequestId = latestAcceptedRequestId_.load(std::memory_order_acquire);
    while (requestId > acceptedRequestId) {
        if (latestAcceptedRequestId_.compare_exchange_weak(acceptedRequestId,
                                                           requestId,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
            if (activeRequestId_.load(std::memory_order_acquire) != 0) {
                setCancellationReason(InterruptionReason::Cancelled);
                interruptConnection();
            }
            return;
        }
    }
}

bool TherionSqlReportExecutionControl::beginOperation(quint64 requestId, int timeoutMilliseconds)
{
    acceptRequest(requestId);
    if (shuttingDown_.load(std::memory_order_acquire)
        || latestAcceptedRequestId_.load(std::memory_order_acquire) != requestId) {
        return false;
    }

    interruptionReason_.store(InterruptionReason::None, std::memory_order_release);
    deadlineMilliseconds_.store(timeoutMilliseconds > 0
                                    ? monotonicMilliseconds() + timeoutMilliseconds
                                    : 0,
                                std::memory_order_release);
    activeRequestId_.store(requestId, std::memory_order_release);

    if (shuttingDown_.load(std::memory_order_acquire)
        || latestAcceptedRequestId_.load(std::memory_order_acquire) != requestId) {
        setCancellationReason(InterruptionReason::Cancelled);
        activeRequestId_.store(0, std::memory_order_release);
        deadlineMilliseconds_.store(0, std::memory_order_release);
        return false;
    }
    return true;
}

TherionSqlReportExecutionControl::InterruptionReason
TherionSqlReportExecutionControl::interruptionReason(quint64 requestId)
{
    if (activeRequestId_.load(std::memory_order_acquire) != requestId) {
        return InterruptionReason::Cancelled;
    }
    shouldInterruptCurrentOperation();
    return interruptionReason_.load(std::memory_order_acquire);
}

void TherionSqlReportExecutionControl::finishOperation(quint64 requestId)
{
    quint64 expectedRequestId = requestId;
    activeRequestId_.compare_exchange_strong(expectedRequestId,
                                             0,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
    deadlineMilliseconds_.store(0, std::memory_order_release);
}

void TherionSqlReportExecutionControl::requestShutdown()
{
    shuttingDown_.store(true, std::memory_order_release);
    setCancellationReason(InterruptionReason::Cancelled);
    interruptConnection();
}

void TherionSqlReportExecutionControl::attachConnection(sqlite3 *connection)
{
    const std::lock_guard lock(connectionMutex_);
    connection_ = connection;
}

void TherionSqlReportExecutionControl::detachConnection(sqlite3 *connection)
{
    const std::lock_guard lock(connectionMutex_);
    if (connection_ == connection) {
        connection_ = nullptr;
    }
}

bool TherionSqlReportExecutionControl::shouldInterruptCurrentOperation()
{
    const quint64 activeRequestId = activeRequestId_.load(std::memory_order_acquire);
    if (activeRequestId == 0) {
        return false;
    }
    if (shuttingDown_.load(std::memory_order_acquire)
        || latestAcceptedRequestId_.load(std::memory_order_acquire) != activeRequestId) {
        setCancellationReason(InterruptionReason::Cancelled);
        return true;
    }

    const qint64 deadline = deadlineMilliseconds_.load(std::memory_order_acquire);
    if (deadline > 0 && monotonicMilliseconds() >= deadline) {
        setCancellationReason(InterruptionReason::TimedOut);
        return true;
    }
    return interruptionReason_.load(std::memory_order_acquire) != InterruptionReason::None;
}

qint64 TherionSqlReportExecutionControl::monotonicMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void TherionSqlReportExecutionControl::interruptConnection()
{
    const std::lock_guard lock(connectionMutex_);
    if (connection_ != nullptr) {
        sqlite3_interrupt(connection_);
    }
}

void TherionSqlReportExecutionControl::setCancellationReason(InterruptionReason reason)
{
    InterruptionReason expectedReason = InterruptionReason::None;
    interruptionReason_.compare_exchange_strong(expectedReason,
                                                reason,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
}

} // namespace TherionStudio
