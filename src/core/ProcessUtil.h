#pragma once
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
// true se il processo con questo PID è ancora in esecuzione
inline bool isProcessRunning(qint64 pid) {
    if (pid <= 0) return false;
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    const DWORD r = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return r == WAIT_TIMEOUT;
}
#else
#include <signal.h>
#include <errno.h>
inline bool isProcessRunning(qint64 pid) {
    if (pid <= 0) return false;
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}
#endif
