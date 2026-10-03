#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QString>

namespace AppConfig {

inline QString projectRoot()
{
    const QString envRoot =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("MINIMINE_ROOT"));

    if (!envRoot.trimmed().isEmpty()) {
        return QDir::cleanPath(envRoot);
    }

    QDir dir(QCoreApplication::applicationDirPath());

    // 开发环境：从构建目录向上查找包含 scripts 的项目根目录。
    QDir candidate = dir;
    while (!candidate.isRoot()) {
        if (candidate.exists(QStringLiteral("scripts"))) {
            return QDir::cleanPath(candidate.absolutePath());
        }
        candidate.cdUp();
    }

    // 部署环境：允许 scripts 与可执行文件位于同一目录。
    return QDir::cleanPath(dir.absolutePath());
}

inline QString runtimeDir()
{
    return QDir(projectRoot()).filePath(QStringLiteral("runtime"));
}

inline QString dbPath()
{
    return QDir(runtimeDir()).filePath(QStringLiteral("minimine.db"));
}

inline QString pythonExe()
{
    const QString envPython =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("MINIMINE_PYTHON"));

    if (!envPython.trimmed().isEmpty()) {
        return QDir::cleanPath(envPython);
    }

#ifdef Q_OS_WIN
    return QStringLiteral("python");
#else
    return QStringLiteral("python3");
#endif
}

inline QString scriptsDir()
{
    return QDir(projectRoot()).filePath(QStringLiteral("scripts"));
}

inline QString logsDir()
{
    return QDir(runtimeDir()).filePath(QStringLiteral("logs"));
}

inline QString mappingsDir()
{
    return QDir(runtimeDir()).filePath(QStringLiteral("mappings"));
}

} // namespace AppConfig

#endif // APPCONFIG_H