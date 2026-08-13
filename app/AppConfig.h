#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

namespace AppConfig {

inline QString projectRoot()
{
    return QStringLiteral("D:/Desktop/MiniMineUI");
}

inline QString dbPath()
{
    return projectRoot() + QStringLiteral("/minimine.db");
}

inline QString pythonExe()
{
    return QStringLiteral("D:/Programs/Python/Python311/python.exe");
}

inline QString scriptsDir()
{
    return projectRoot() + QStringLiteral("/scripts");
}

inline QString logsDir()
{
    return projectRoot() + QStringLiteral("/logs");
}

inline QString mappingsDir()
{
    return projectRoot() + QStringLiteral("/mappings");
}

} // namespace AppConfig

#endif // APPCONFIG_H
