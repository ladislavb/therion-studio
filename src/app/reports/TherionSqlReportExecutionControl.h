#pragma once

#include <QtGlobal>

#include <atomic>
#include <memory>
#include <mutex>

struct sqlite3;

namespace TherionStudio
{

class TherionSqlReportExecutionControl final
{
public:
    enum class InterruptionReason
    {
        None,
        Cancelled,
        TimedOut
    };

    void acceptRequest(quint64 requestId);
    bool beginOperation(quint64 requestId, int timeoutMilliseconds);
    InterruptionReason interruptionReason(quint64 requestId);
    void finishOperation(quint64 requestId);
    void requestShutdown();

    void attachConnection(sqlite3 *connection);
    void detachConnection(sqlite3 *connection);
    bool shouldInterruptCurrentOperation();

private:
    static qint64 monotonicMilliseconds();
    void interruptConnection();
    void setCancellationReason(InterruptionReason reason);

    std::atomic<quint64> latestAcceptedRequestId_{0};
    std::atomic<quint64> activeRequestId_{0};
    std::atomic<qint64> deadlineMilliseconds_{0};
    std::atomic<InterruptionReason> interruptionReason_{InterruptionReason::None};
    std::atomic_bool shuttingDown_{false};
    std::mutex connectionMutex_;
    sqlite3 *connection_ = nullptr;
};

using TherionSqlReportExecutionControlPtr = std::shared_ptr<TherionSqlReportExecutionControl>;

} // namespace TherionStudio
