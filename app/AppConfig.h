#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QCoreApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QString>

namespace AppConfig {

inline QString projectRoot()
{
    QDir dir(QCoreApplication::applicationDirPath());

    // 开发环境下，可执行文件通常位于构建目录中。
    // 允许通过环境变量显式指定 MiniMine 项目根目录。
    const QString envRoot =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("MINIMINE_ROOT"));

    if (!envRoot.trimmed().isEmpty()) {
        return QDir::cleanPath(envRoot);
    }

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