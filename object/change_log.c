#define _CRT_SECURE_NO_WARNINGS
#include "change_log.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ChangeLog* changeLogListHead = NULL;
static char gOperator[CHANGE_LOG_OPERATOR_LEN] = "system";
static int gChangeLogEnabled = 1;

static const int LOG_TABLE_WIDTHS[] = { 19, 12, 24, 50, 16 };
static const char* LOG_TABLE_HEADERS[] = {
    "时间", "模块", "对象", "变更内容", "操作人"
};

static void sanitizeField(char* dest, int destSize, const char* src) {
    int i;

    if (dest == NULL || destSize <= 0) return;
    copyText(dest, destSize, src == NULL ? "" : src);
    for (i = 0; dest[i] != '\0'; ++i) {
        if (dest[i] == '|' || dest[i] == '\n' || dest[i] == '\r') dest[i] = ' ';
    }
}

static void addChangeLog(ChangeLog* log) {
    ChangeLog* current;

    if (log == NULL) return;
    log->next = NULL;
    if (changeLogListHead == NULL) {
        changeLogListHead = log;
        return;
    }
    current = changeLogListHead;
    while (current->next != NULL) current = current->next;
    current->next = log;
}

static int generateChangeLogId() {
    int maxId = 90000000;
    ChangeLog* current = changeLogListHead;

    while (current != NULL) {
        if (current->logId > maxId) maxId = current->logId;
        current = current->next;
    }
    return maxId + 1;
}

static int isMatch(const ChangeLog* log, const char* moduleName, const char* keyword) {
    if (log == NULL) return 0;
    if (moduleName != NULL && moduleName[0] != '\0') {
        if (strcmp(log->moduleName, moduleName) != 0) return 0;
    }
    if (keyword != NULL && keyword[0] != '\0') {
        if (strstr(log->targetName, keyword) == NULL) return 0;
    }
    return 1;
}

static int countMatches(const char* moduleName, const char* keyword) {
    ChangeLog* current = changeLogListHead;
    int count = 0;

    while (current != NULL) {
        if (isMatch(current, moduleName, keyword)) count++;
        current = current->next;
    }
    return count;
}

static void printLogHeader() {
    printTableBorder(LOG_TABLE_WIDTHS, 5);
    printTableRow(LOG_TABLE_HEADERS, LOG_TABLE_WIDTHS, 5);
    printTableBorder(LOG_TABLE_WIDTHS, 5);
}

static void printLog(const ChangeLog* log) {
    const char* cells[5];

    if (log == NULL) return;
    cells[0] = log->createTime;
    cells[1] = log->moduleName;
    cells[2] = log->targetName;
    cells[3] = log->changeContent;
    cells[4] = log->operatorName;
    printTableRow(cells, LOG_TABLE_WIDTHS, 5);
}

static void printLogsWithFilter(const char* title, const char* moduleName, const char* keyword) {
    int total = countMatches(moduleName, keyword);
    ChangeLog** items = NULL;
    ChangeLog* current;
    int index = 0;
    int i;

    printf("\n================ %s ================\n", title == NULL ? "变更日志" : title);
    if (total <= 0) {
        printf("(暂无日志)\n");
        return;
    }

    items = (ChangeLog**)malloc(sizeof(ChangeLog*) * (size_t)total);
    if (items == NULL) {
        printLogHeader();
        current = changeLogListHead;
        while (current != NULL) {
            if (isMatch(current, moduleName, keyword)) printLog(current);
            current = current->next;
        }
        printTableBorder(LOG_TABLE_WIDTHS, 5);
        return;
    }

    current = changeLogListHead;
    while (current != NULL) {
        if (isMatch(current, moduleName, keyword)) items[index++] = current;
        current = current->next;
    }

    printLogHeader();
    for (i = total - 1; i >= 0; --i) {
        printLog(items[i]);
    }
    printTableBorder(LOG_TABLE_WIDTHS, 5);
    free(items);
}

void initChangeLogList() {
    ChangeLog* current = changeLogListHead;

    while (current != NULL) {
        ChangeLog* next = current->next;
        free(current);
        current = next;
    }
    changeLogListHead = NULL;
}

void setChangeLogOperator(const char* operatorName) {
    char sanitized[CHANGE_LOG_OPERATOR_LEN];

    sanitizeField(sanitized, sizeof(sanitized), operatorName);
    if (sanitized[0] == '\0') {
        copyText(gOperator, sizeof(gOperator), "system");
        return;
    }
    copyText(gOperator, sizeof(gOperator), sanitized);
}

const char* getChangeLogOperator() {
    return gOperator;
}

void setChangeLogEnabled(int enabled) {
    gChangeLogEnabled = enabled ? 1 : 0;
}

int addChangeLogEntry(const char* moduleName, const char* targetName, const char* changeContent) {
    ChangeLog* log;

    if (!gChangeLogEnabled) return 0;
    if (moduleName == NULL || moduleName[0] == '\0') return 0;
    if (targetName == NULL || targetName[0] == '\0') return 0;

    log = (ChangeLog*)malloc(sizeof(ChangeLog));
    if (log == NULL) return 0;
    memset(log, 0, sizeof(ChangeLog));

    log->logId = generateChangeLogId();
    sanitizeField(log->moduleName, CHANGE_LOG_MODULE_LEN, moduleName);
    sanitizeField(log->targetName, CHANGE_LOG_TARGET_LEN, targetName);
    sanitizeField(log->changeContent, CHANGE_LOG_CONTENT_LEN, changeContent == NULL ? "" : changeContent);
    sanitizeField(log->operatorName, CHANGE_LOG_OPERATOR_LEN, gOperator);
    getCurrentTime(log->createTime, CHANGE_LOG_TIME_LEN);
    log->next = NULL;
    addChangeLog(log);
    return log->logId;
}

void printAllChangeLogs() {
    printLogsWithFilter("变更日志", NULL, NULL);
}

void printChangeLogsByModule(const char* moduleName) {
    printLogsWithFilter("变更日志(模块筛选)", moduleName, NULL);
}

void printChangeLogsByTargetKeyword(const char* keyword) {
    printLogsWithFilter("变更日志(对象筛选)", NULL, keyword);
}

int countChangeLogs() {
    ChangeLog* current = changeLogListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

void saveChangeLogsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    ChangeLog* current = changeLogListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%d|%s|%s|%s|%s|%s\n",
            current->logId, current->moduleName, current->targetName,
            current->changeContent, current->operatorName, current->createTime);
        current = current->next;
    }
    fclose(file);
}

static int parseChangeLogLine(char* line, ChangeLog* log) {
    char* fields[6];

    if (splitFields(line, fields, 6) != 6) return 0;
    if (!parseIntField(fields[0], &log->logId)) return 0;
    copyText(log->moduleName, CHANGE_LOG_MODULE_LEN, fields[1]);
    copyText(log->targetName, CHANGE_LOG_TARGET_LEN, fields[2]);
    copyText(log->changeContent, CHANGE_LOG_CONTENT_LEN, fields[3]);
    copyText(log->operatorName, CHANGE_LOG_OPERATOR_LEN, fields[4]);
    copyText(log->createTime, CHANGE_LOG_TIME_LEN, fields[5]);
    return 1;
}

void loadChangeLogsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[512];
    int lineNo = 0;

    initChangeLogList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        ChangeLog* log;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        log = (ChangeLog*)malloc(sizeof(ChangeLog));
        if (log == NULL) break;
        memset(log, 0, sizeof(ChangeLog));
        if (!parseChangeLogLine(line, log)) {
            printf("跳过变更日志第 %d 行：格式错误。\n", lineNo);
            free(log);
            continue;
        }
        log->next = NULL;
        addChangeLog(log);
    }
    fclose(file);
}
