#pragma once
#ifndef CHANGE_LOG_H
#define CHANGE_LOG_H

#define CHANGE_LOG_MODULE_LEN 32
#define CHANGE_LOG_TARGET_LEN 64
#define CHANGE_LOG_CONTENT_LEN 200
#define CHANGE_LOG_OPERATOR_LEN 32
#define CHANGE_LOG_TIME_LEN 24

typedef struct ChangeLog {
    int logId;
    char moduleName[CHANGE_LOG_MODULE_LEN];
    char targetName[CHANGE_LOG_TARGET_LEN];
    char changeContent[CHANGE_LOG_CONTENT_LEN];
    char operatorName[CHANGE_LOG_OPERATOR_LEN];
    char createTime[CHANGE_LOG_TIME_LEN];
    struct ChangeLog* next;
} ChangeLog;

void initChangeLogList();
void setChangeLogOperator(const char* operatorName);
const char* getChangeLogOperator();
void setChangeLogEnabled(int enabled);

int addChangeLogEntry(const char* moduleName, const char* targetName, const char* changeContent);
void printAllChangeLogs();
void printChangeLogsByModule(const char* moduleName);
void printChangeLogsByTargetKeyword(const char* keyword);
int countChangeLogs();
void saveChangeLogsToFile(const char* filename);
void loadChangeLogsFromFile(const char* filename);

#endif
