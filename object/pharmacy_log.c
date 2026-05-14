#define _CRT_SECURE_NO_WARNINGS
#include "pharmacy_log.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 药品流水只记录库存变化的事实：入库、发药、关联患者和医生。
 * 它不直接决定库存，库存增减由 medicine.c 先做校验。
 */

PharmacyLog* pharmacyLogListHead = NULL;

static const int LOG_TABLE_WIDTHS[] = { 8, 8, 8, 6, 18, 8, 19, 28 };
static const char* LOG_TABLE_HEADERS[] = {
    "流水ID", "药品ID", "类型", "数量", "患者ID", "医生ID", "时间", "备注"
};

// 初始化药品流水链表。
void initPharmacyLogList() {
    pharmacyLogListHead = NULL;
}

// 根据当前最大流水号生成下一条 ID。
static int generateLogId() {
    int maxId = 80000000;
    PharmacyLog* current = pharmacyLogListHead;

    while (current != NULL) {
        if (current->logId > maxId) maxId = current->logId;
        current = current->next;
    }
    return maxId + 1;
}

// 将药品流水节点追加到链表尾部。
void addPharmacyLog(PharmacyLog* log) {
    PharmacyLog* current;

    if (log == NULL) return;
    log->next = NULL;
    if (pharmacyLogListHead == NULL) {
        pharmacyLogListHead = log;
        return;
    }
    current = pharmacyLogListHead;
    while (current->next != NULL) current = current->next;
    current->next = log;
}

// 生成一条入库或发药流水。
int addPharmacyLogEntry(int medicineId, const char* type, int quantity, const char* patientId, int doctorId, const char* remark) {
    PharmacyLog* log;

    if (type == NULL || type[0] == '\0') return -1;
    if (quantity <= 0) return -2;

    log = (PharmacyLog*)malloc(sizeof(PharmacyLog));
    if (log == NULL) return -3;
    memset(log, 0, sizeof(PharmacyLog));

    log->logId = generateLogId();
    log->medicineId = medicineId;
    copyText(log->type, PHARMACY_LOG_TYPE_LEN, type);
    log->quantity = quantity;
    copyText(log->patientId, PATIENT_ID_LEN, patientId == NULL ? PATIENT_ID_NONE : patientId);
    log->doctorId = doctorId;
    getCurrentTime(log->time, PHARMACY_LOG_TIME_LEN);
    copyText(log->remark, PHARMACY_LOG_REMARK_LEN, remark == NULL ? "" : remark);
    log->next = NULL;
    addPharmacyLog(log);
    return log->logId;
}

// 打印药品流水表头。
static void printLogHeader() {
    printTableBorder(LOG_TABLE_WIDTHS, 8);
    printTableRow(LOG_TABLE_HEADERS, LOG_TABLE_WIDTHS, 8);
    printTableBorder(LOG_TABLE_WIDTHS, 8);
}

// 打印单条药品流水。
static void printLog(const PharmacyLog* log) {
    char logId[16];
    char medicineId[16];
    char quantity[16];
    char doctorId[16];
    const char* cells[8];

    if (log == NULL) return;
    snprintf(logId, sizeof(logId), "%d", log->logId);
    snprintf(medicineId, sizeof(medicineId), "%d", log->medicineId);
    snprintf(quantity, sizeof(quantity), "%d", log->quantity);
    snprintf(doctorId, sizeof(doctorId), "%d", log->doctorId);
    cells[0] = logId;
    cells[1] = medicineId;
    cells[2] = log->type;
    cells[3] = quantity;
    cells[4] = log->patientId;
    cells[5] = doctorId;
    cells[6] = log->time;
    cells[7] = log->remark;
    printTableRow(cells, LOG_TABLE_WIDTHS, 8);
}

// 打印全部药品流水。
void printAllPharmacyLogs() {
    PharmacyLog* current = pharmacyLogListHead;

    printf("\n================ 药品流水 ================\n");
    printLogHeader();
    while (current != NULL) {
        printLog(current);
        current = current->next;
    }
    printTableBorder(LOG_TABLE_WIDTHS, 8);
}

// 按药品 ID 打印流水。
void printPharmacyLogsByMedicineId(int medicineId) {
    PharmacyLog* current = pharmacyLogListHead;
    int found = 0;

    printf("\n========== 药品 %d 流水 ==========\n", medicineId);
    printLogHeader();
    while (current != NULL) {
        if (current->medicineId == medicineId) {
            printLog(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(暂无相关流水)\n");
    printTableBorder(LOG_TABLE_WIDTHS, 8);
}

// 按患者 ID 打印流水。
void printPharmacyLogsByPatientId(const char* patientId) {
    PharmacyLog* current = pharmacyLogListHead;
    int found = 0;

    if (patientId == NULL || patientId[0] == '\0') return;
    printf("\n========== 患者 %s 药品流水 ==========\n", patientId);
    printLogHeader();
    while (current != NULL) {
        if (strcmp(current->patientId, patientId) == 0) {
            printLog(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(暂无相关流水)\n");
    printTableBorder(LOG_TABLE_WIDTHS, 8);
}

// 按医生 ID 打印流水。
void printPharmacyLogsByDoctorId(int doctorId) {
    PharmacyLog* current = pharmacyLogListHead;
    int found = 0;

    printf("\n========== 医生 %d 药品流水 ==========\n", doctorId);
    printLogHeader();
    while (current != NULL) {
        if (current->doctorId == doctorId) {
            printLog(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(暂无相关流水)\n");
    printTableBorder(LOG_TABLE_WIDTHS, 8);
}

// 保存药品流水到文本文件。
void savePharmacyLogsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    PharmacyLog* current = pharmacyLogListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%d|%d|%s|%d|%s|%d|%s|%s\n",
            current->logId, current->medicineId, current->type,
            current->quantity, current->patientId, current->doctorId,
            current->time, current->remark);
        current = current->next;
    }
    fclose(file);
}

// 解析一行药品流水数据。
static int parseLogLine(char* line, PharmacyLog* log) {
    char* fields[8];

    // 流水备注里可能有中文说明，用 | 分隔比空格读取稳。
    if (strchr(line, '|') != NULL) {
        if (splitFields(line, fields, 8) != 8) return 0;
        if (!parseIntField(fields[0], &log->logId)) return 0;
        if (!parseIntField(fields[1], &log->medicineId)) return 0;
        copyText(log->type, PHARMACY_LOG_TYPE_LEN, fields[2]);
        if (!parseIntField(fields[3], &log->quantity)) return 0;
        copyText(log->patientId, PATIENT_ID_LEN, fields[4]);
        if (!parseIntField(fields[5], &log->doctorId)) return 0;
        copyText(log->time, PHARMACY_LOG_TIME_LEN, fields[6]);
        copyText(log->remark, PHARMACY_LOG_REMARK_LEN, fields[7]);
        return 1;
    }

    return sscanf(line, "%d %d %19s %d %19s %d %49s %119s",
        &log->logId, &log->medicineId, log->type, &log->quantity,
        log->patientId, &log->doctorId, log->time, log->remark) == 8;
}

// 从文件加载药品流水数据。
void loadPharmacyLogsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[512];
    int lineNo = 0;

    initPharmacyLogList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        PharmacyLog* log;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        log = (PharmacyLog*)malloc(sizeof(PharmacyLog));
        if (log == NULL) break;
        memset(log, 0, sizeof(PharmacyLog));
        if (!parseLogLine(line, log)) {
            printf("跳过药品流水第 %d 行：格式错误。\n", lineNo);
            free(log);
            continue;
        }
        log->next = NULL;
        addPharmacyLog(log);
    }
    fclose(file);
}
