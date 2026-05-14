#define _CRT_SECURE_NO_WARNINGS
#include "patient.h"
#include "util.h"
#include "change_log.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 患者信息用单链表维护，身份证号作为主要检索键。
 * 住院状态不单独建表，而是记录在患者、病房和床位之间的关联字段里。
 */

Patient* patientListHead = NULL;

static const int PATIENT_TABLE_WIDTHS[] = { 18, 12, 6, 6, 18, 8, 8, 8, 14 };
static const char* PATIENT_TABLE_HEADERS[] = {
    "身份证号", "姓名", "性别", "年龄", "诊断", "类型", "病房", "床位", "电话"
};

static void appendText(char* buffer, int bufferSize, const char* text) {
    size_t len;
    size_t remaining;

    if (buffer == NULL || bufferSize <= 0 || text == NULL) return;
    len = strlen(buffer);
    if (len >= (size_t)bufferSize - 1) return;
    remaining = (size_t)bufferSize - 1 - len;
    strncat(buffer, text, remaining);
}

static void appendFieldChange(char* buffer, int bufferSize, const char* label, const char* oldValue, const char* newValue) {
    char segment[128];

    if (oldValue == NULL) oldValue = "";
    if (newValue == NULL) newValue = "";
    if (strcmp(oldValue, newValue) == 0) return;
    snprintf(segment, sizeof(segment), "%s由%s改为%s；", label, oldValue, newValue);
    appendText(buffer, bufferSize, segment);
}

static void appendIntChange(char* buffer, int bufferSize, const char* label, int oldValue, int newValue) {
    char segment[128];

    if (oldValue == newValue) return;
    snprintf(segment, sizeof(segment), "%s由%d改为%d；", label, oldValue, newValue);
    appendText(buffer, bufferSize, segment);
}

static void buildPatientTargetName(const Patient* patient, char* buffer, int bufferSize) {
    if (buffer == NULL || bufferSize <= 0) return;
    if (patient != NULL && patient->name[0] != '\0') {
        snprintf(buffer, bufferSize, "患者 %s", patient->name);
    } else if (patient != NULL) {
        snprintf(buffer, bufferSize, "患者 %s", patient->patientId);
    } else {
        copyText(buffer, bufferSize, "患者");
    }
}

// 初始化患者链表。
void initPatientList() {
    patientListHead = NULL;
}

// 将患者节点追加到链表尾部。
void addPatient(Patient* p) {
    Patient* current;

    if (p == NULL) return;
    p->next = NULL;
    if (patientListHead == NULL) {
        patientListHead = p;
        return;
    }

    current = patientListHead;
    while (current->next != NULL) current = current->next;
    current->next = p;

    {
        char target[CHANGE_LOG_TARGET_LEN];
        buildPatientTargetName(p, target, sizeof(target));
        addChangeLogEntry("患者管理", target, "新增患者资料");
    }
}

// 按患者身份证号删除患者。
int deletePatientById(const char* patientId) {
    Patient* current = patientListHead;
    Patient* previous = NULL;

    if (patientId == NULL || patientId[0] == '\0') return 0;
    while (current != NULL) {
        if (strcmp(current->patientId, patientId) == 0) {
            Patient snapshot = *current;
            if (previous == NULL) patientListHead = current->next;
            else previous->next = current->next;
            free(current);
            {
                char target[CHANGE_LOG_TARGET_LEN];
                buildPatientTargetName(&snapshot, target, sizeof(target));
                addChangeLogEntry("患者管理", target, "删除患者资料");
            }
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return patientListHead == NULL ? -1 : 0;
}

// 按身份证号查找患者。
Patient* searchPatientById(const char* patientId) {
    Patient* current = patientListHead;

    if (patientId == NULL || patientId[0] == '\0') return NULL;
    while (current != NULL) {
        if (strcmp(current->patientId, patientId) == 0) return current;
        current = current->next;
    }
    return NULL;
}

// 按姓名查找第一个匹配的患者。
Patient* searchPatientByName(char* patientName) {
    Patient* current = patientListHead;

    while (current != NULL) {
        if (strcmp(current->name, patientName) == 0) return current;
        current = current->next;
    }
    return NULL;
}

// 修改患者基础资料和住院关联字段。
void modifyPatient(Patient* patient) {
    Patient* target;

    if (patient == NULL) return;
    target = searchPatientById(patient->patientId);
    if (target == NULL) return;

    {
        Patient before = *target;
        char content[CHANGE_LOG_CONTENT_LEN];
        char targetName[CHANGE_LOG_TARGET_LEN];

        content[0] = '\0';
        appendFieldChange(content, sizeof(content), "姓名", before.name, patient->name);
        appendFieldChange(content, sizeof(content), "性别", before.gender, patient->gender);
        appendIntChange(content, sizeof(content), "年龄", before.age, patient->age);
        appendFieldChange(content, sizeof(content), "诊断", before.diagnosis, patient->diagnosis);
        appendFieldChange(content, sizeof(content), "类型", before.typeOfPatient, patient->typeOfPatient);
        appendIntChange(content, sizeof(content), "病房", before.relatedWardId, patient->relatedWardId);
        appendIntChange(content, sizeof(content), "床位", before.relatedBedId, patient->relatedBedId);
        appendFieldChange(content, sizeof(content), "电话", before.phone, patient->phone);

        if (content[0] == '\0') copyText(content, sizeof(content), "更新患者信息");
        buildPatientTargetName(patient, targetName, sizeof(targetName));

        copyText(target->name, PATIENT_NAME_LEN, patient->name);
        copyText(target->gender, PATIENT_GENDER_LEN, patient->gender);
        target->age = patient->age;
        copyText(target->diagnosis, PATIENT_DIAGNOSIS_LEN, patient->diagnosis);
        copyText(target->typeOfPatient, PATIENT_TYPE_LEN, patient->typeOfPatient);
        target->relatedWardId = patient->relatedWardId;
        target->relatedBedId = patient->relatedBedId;
        copyText(target->phone, PATIENT_PHONE_LEN, patient->phone);

        addChangeLogEntry("患者管理", targetName, content);
        return;
    }
}

// 身份证号：18 位，前 17 位是数字，最后 1 位是数字或大写 X。
int isValidPatientIdCard(const char* patientId) {
    int i;

    if (patientId == NULL || strlen(patientId) != 18) return 0;
    for (i = 0; i < 17; ++i) {
        if (!isdigit((unsigned char)patientId[i])) return 0;
    }
    return isdigit((unsigned char)patientId[17]) || patientId[17] == 'X';
}

// 打印单个患者的表格行。
void printPatient(const Patient* p) {
    char age[16];
    char wardId[16];
    char bedId[16];
    const char* cells[9];

    if (p == NULL) return;
    snprintf(age, sizeof(age), "%d", p->age);
    snprintf(wardId, sizeof(wardId), "%d", p->relatedWardId);
    snprintf(bedId, sizeof(bedId), "%d", p->relatedBedId);
    cells[0] = p->patientId;
    cells[1] = p->name;
    cells[2] = p->gender;
    cells[3] = age;
    cells[4] = p->diagnosis;
    cells[5] = p->typeOfPatient;
    cells[6] = wardId;
    cells[7] = bedId;
    cells[8] = p->phone;
    printTableRow(cells, PATIENT_TABLE_WIDTHS, 9);
}

// 打印患者列表表头。
void printPatientTableHeader() {
    printTableBorder(PATIENT_TABLE_WIDTHS, 9);
    printTableRow(PATIENT_TABLE_HEADERS, PATIENT_TABLE_WIDTHS, 9);
    printTableBorder(PATIENT_TABLE_WIDTHS, 9);
}

void printPatientTableBorder() {
    printTableBorder(PATIENT_TABLE_WIDTHS, 9);
}

// 打印全部患者信息。
void printAllPatients() {
    Patient* current = patientListHead;

    if (current == NULL) {
        printf("当前无患者数据。\n");
        return;
    }

    printf("\n================ 患者列表 ================\n");
    printPatientTableHeader();
    while (current != NULL) {
        printPatient(current);
        current = current->next;
    }
    printPatientTableBorder();
}

// 按姓名打印所有匹配患者。
int printPatientsByName(const char* patientName) {
    Patient* current = patientListHead;
    int found = 0;

    if (patientName == NULL || patientName[0] == '\0') return 0;
    printf("\n================ 患者姓名查询 ================\n");
    printPatientTableHeader();
    while (current != NULL) {
        if (strcmp(current->name, patientName) == 0) {
            printPatient(current);
            found++;
        }
        current = current->next;
    }
    if (!found) printf("(未找到姓名为 %s 的患者)\n", patientName);
    printPatientTableBorder();
    return found;
}

// 统计患者总数。
int countPatients() {
    Patient* current = patientListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 统计指定类型的患者数量。
int countPatientsByType(const char* typeOfPatient) {
    Patient* current = patientListHead;
    int count = 0;

    while (current != NULL) {
        if (strcmp(current->typeOfPatient, typeOfPatient) == 0) count++;
        current = current->next;
    }
    return count;
}

// 保存患者链表到文本文件。
void savePatientsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    Patient* current = patientListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%s|%s|%s|%d|%s|%s|%d|%d|%s\n",
            current->patientId, current->name, current->gender, current->age,
            current->diagnosis, current->typeOfPatient, current->relatedWardId,
            current->relatedBedId, current->phone);
        current = current->next;
    }
    fclose(file);
}

// 解析一行患者数据。
static int parsePatientLine(char* line, Patient* p) {
    char* fields[9];
    int count;

    // 新格式用 | 分隔，下面的 sscanf 保留给旧版空格数据。
    if (strchr(line, '|') != NULL) {
        count = splitFields(line, fields, 9);
        if (count != 9) return 0;
        copyText(p->patientId, PATIENT_ID_LEN, fields[0]);
        copyText(p->name, PATIENT_NAME_LEN, fields[1]);
        copyText(p->gender, PATIENT_GENDER_LEN, fields[2]);
        if (!parseIntField(fields[3], &p->age)) return 0;
        copyText(p->diagnosis, PATIENT_DIAGNOSIS_LEN, fields[4]);
        copyText(p->typeOfPatient, PATIENT_TYPE_LEN, fields[5]);
        if (!parseIntField(fields[6], &p->relatedWardId)) return 0;
        if (!parseIntField(fields[7], &p->relatedBedId)) return 0;
        copyText(p->phone, PATIENT_PHONE_LEN, fields[8]);
        return 1;
    }

    return sscanf(line, "%19s %49s %9s %d %99s %19s %d %d %19s",
        p->patientId, p->name, p->gender, &p->age, p->diagnosis,
        p->typeOfPatient, &p->relatedWardId, &p->relatedBedId, p->phone) == 9;
}

// 从文件加载患者数据。
void loadPatientsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[512];
    int lineNo = 0;

    initPatientList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        Patient* p;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        p = (Patient*)malloc(sizeof(Patient));
        if (p == NULL) break;
        memset(p, 0, sizeof(Patient));
        if (!parsePatientLine(line, p)) {
            printf("跳过患者数据第 %d 行：格式错误。\n", lineNo);
            free(p);
            continue;
        }
        p->next = NULL;
        addPatient(p);
    }
    fclose(file);
}
