#pragma once

#include <QString>

namespace CrashLogger {

void initialize(const QString &defaultDirectory);
void setLogDirectory(const QString &directory);
QString logFilePath();
void log(const QString &message);

} // namespace CrashLogger
