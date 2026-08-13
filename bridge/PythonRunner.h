#ifndef PYTHONRUNNER_H
#define PYTHONRUNNER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

class PythonRunner
{
public:
    static bool runScript(const QString& scriptFileName,
                          const QStringList& args,
                          QJsonObject* result,
                          QString* errorMessage,
                          int timeoutMs = 120000);
};

#endif // PYTHONRUNNER_H
