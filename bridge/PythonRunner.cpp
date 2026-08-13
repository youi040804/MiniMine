#include "PythonRunner.h"
#include "AppConfig.h"
#include "DatabaseManager.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFileInfo>

namespace {

class ScopedDatabaseRelease
{
public:
    ScopedDatabaseRelease()
    {
        DatabaseManager& db = DatabaseManager::instance();
        m_wasOpen = db.isOpen();
        if (m_wasOpen) {
            db.close();
        }
    }

    ~ScopedDatabaseRelease()
    {
        if (m_wasOpen) {
            DatabaseManager::instance().open(AppConfig::dbPath());
        }
    }

private:
    bool m_wasOpen = false;
};

} // namespace

bool PythonRunner::runScript(const QString& scriptFileName,
                             const QStringList& args,
                             QJsonObject* result,
                             QString* errorMessage,
                             int timeoutMs)
{
    if (result) {
        *result = QJsonObject();
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    const QString scriptPath = AppConfig::scriptsDir() + QStringLiteral("/") + scriptFileName;
    if (!QFileInfo::exists(scriptPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("找不到 Python 脚本: %1").arg(scriptPath);
        }
        return false;
    }

    if (!QFileInfo::exists(AppConfig::pythonExe())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("找不到 Python 解释器: %1").arg(AppConfig::pythonExe());
        }
        return false;
    }

    ScopedDatabaseRelease releaseDbForPython;

    QProcess process;
    process.setWorkingDirectory(AppConfig::projectRoot());

    QStringList fullArgs;
    fullArgs << scriptPath;
    fullArgs << args;

    process.start(AppConfig::pythonExe(), fullArgs);
    if (!process.waitForStarted(10000)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法启动 Python 进程");
        }
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Python 脚本执行超时");
        }
        return false;
    }

    const QByteArray stdOut = process.readAllStandardOutput().trimmed();
    const QByteArray stdErr = process.readAllStandardError().trimmed();

    if (!stdOut.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(stdOut, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            if (result) {
                *result = obj;
            }
            if (obj.contains(QStringLiteral("status"))) {
                const QString status = obj.value(QStringLiteral("status")).toString();
                if (status == QStringLiteral("success")) {
                    return true;
                }
                if (errorMessage) {
                    *errorMessage = obj.value(QStringLiteral("message")).toString(
                        QString::fromUtf8(stdErr));
                }
                return false;
            }
            if (obj.contains(QStringLiteral("success"))) {
                return obj.value(QStringLiteral("success")).toBool();
            }
        }
    }

    if (process.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = stdErr.isEmpty()
                ? QStringLiteral("Python 脚本执行失败，退出码: %1").arg(process.exitCode())
                : QString::fromUtf8(stdErr);
        }
        return false;
    }

    if (stdOut.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Python 脚本未返回结果");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(stdOut, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("解析 Python 返回 JSON 失败: %1\n输出: %2")
                                .arg(parseError.errorString(), QString::fromUtf8(stdOut));
        }
        return false;
    }

    if (result) {
        *result = doc.object();
    }

    return true;
}
