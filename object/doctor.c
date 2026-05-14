#define _CRT_SECURE_NO_WARNINGS
#include "doctor.h"
#include "department.h"
#include "util.h"
#include "change_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 医生模块负责医生基础资料和科室关联。
 * 科室人数不是手填的，医生增删改后统一重新统计，展示时更可靠。
 */

Doctor* doctorListHead = NULL;

static const int DOCTOR_TABLE_WIDTHS[] = { 8, 12, 6, 6, 13, 12, 16, 8, 14, 6 };
static const char* DOCTOR_TABLE_HEADERS[] = {
    "医生ID", "姓名", "性别", "年龄", "电话", "职称", "专业", "科室ID", "科室", "状态"
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

static const char* onDutyText(int onDuty) {
    return onDuty ? "在岗" : "停诊";
}

static void buildDoctorTargetName(const Doctor* doctor, char* buffer, int bufferSize) {
    if (buffer == NULL || bufferSize <= 0) return;
    if (doctor != NULL && doctor->name[0] != '\0') {
        snprintf(buffer, bufferSize, "医生 %s", doctor->name);
    } else if (doctor != NULL) {
        snprintf(buffer, bufferSize, "医生ID %d", doctor->doctorId);
    } else {
        copyText(buffer, bufferSize, "医生");
    }
}

// 初始化医生链表。
void initDoctorList() {
    doctorListHead = NULL;
}

// 根据科室 ID 补齐医生结构里的科室名称。
static void fillDepartmentName(Doctor* doctor) {
    Department* dep;

    if (doctor == NULL) return;
    dep = searchDepartmentById(doctor->departmentId);
    if (dep != NULL) copyText(doctor->departmentName, DOCTOR_DEPARTMENT_LEN, dep->name);
    else copyText(doctor->departmentName, DOCTOR_DEPARTMENT_LEN, "未知科室");
}

// 新增医生并同步刷新科室人数。
void addDoctor(Doctor* p) {
    Doctor* current;

    if (p == NULL) return;
    fillDepartmentName(p);
    p->next = NULL;
    if (doctorListHead == NULL) {
        doctorListHead = p;
    } else {
        current = doctorListHead;
        while (current->next != NULL) current = current->next;
        current->next = p;
    }
    recalculateDepartmentDoctorCounts();

    {
        char target[CHANGE_LOG_TARGET_LEN];
        buildDoctorTargetName(p, target, sizeof(target));
        addChangeLogEntry("医生管理", target, "新增医生资料");
    }
}

// 按医生 ID 删除医生并刷新科室人数。
int deleteDoctorById(int doctorId) {
    Doctor* current = doctorListHead;
    Doctor* previous = NULL;

    while (current != NULL) {
        if (current->doctorId == doctorId) {
            Doctor snapshot = *current;
            if (previous == NULL) doctorListHead = current->next;
            else previous->next = current->next;
            free(current);
            recalculateDepartmentDoctorCounts();
            {
                char target[CHANGE_LOG_TARGET_LEN];
                buildDoctorTargetName(&snapshot, target, sizeof(target));
                addChangeLogEntry("医生管理", target, "删除医生资料");
            }
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return doctorListHead == NULL ? -1 : 0;
}

// 按医生 ID 查找医生。
Doctor* searchDoctorById(int doctorId) {
    Doctor* current = doctorListHead;

    while (current != NULL) {
        if (current->doctorId == doctorId) return current;
        current = current->next;
    }
    return NULL;
}

// 按身份证号查找医生，用于医生账号注册。
Doctor* searchDoctorByIdCard(const char* idCard) {
    Doctor* current = doctorListHead;

    if (idCard == NULL || idCard[0] == '\0') return NULL;
    while (current != NULL) {
        if (strcmp(current->idCard, idCard) == 0) return current;
        current = current->next;
    }
    return NULL;
}

// 按姓名查找医生。
Doctor* searchDoctorByName(char* doctorName) {
    Doctor* current = doctorListHead;

    while (current != NULL) {
        if (strcmp(current->name, doctorName) == 0) return current;
        current = current->next;
    }
    return NULL;
}

// 修改医生资料并重新同步科室信息。
void modifyDoctor(Doctor* doctor) {
    Doctor* target;

    if (doctor == NULL) return;
    target = searchDoctorById(doctor->doctorId);
    if (target == NULL) return;

    {
        Doctor before = *target;
        char content[CHANGE_LOG_CONTENT_LEN];
        char targetName[CHANGE_LOG_TARGET_LEN];

        content[0] = '\0';
        appendFieldChange(content, sizeof(content), "身份证", before.idCard, doctor->idCard);
        appendFieldChange(content, sizeof(content), "姓名", before.name, doctor->name);
        appendFieldChange(content, sizeof(content), "职称", before.title, doctor->title);
        appendFieldChange(content, sizeof(content), "专业", before.specialty, doctor->specialty);
        appendFieldChange(content, sizeof(content), "电话", before.phone, doctor->phone);
        appendFieldChange(content, sizeof(content), "性别", before.gender, doctor->gender);
        appendIntChange(content, sizeof(content), "年龄", before.age, doctor->age);
        appendIntChange(content, sizeof(content), "科室ID", before.departmentId, doctor->departmentId);
        appendFieldChange(content, sizeof(content), "状态", onDutyText(before.onDuty), onDutyText(doctor->onDuty));

        if (content[0] == '\0') copyText(content, sizeof(content), "更新医生信息");
        buildDoctorTargetName(doctor, targetName, sizeof(targetName));

        copyText(target->idCard, DOCTOR_ID_CARD_LEN, doctor->idCard);
        copyText(target->name, DOCTOR_NAME_LEN, doctor->name);
        copyText(target->title, DOCTOR_TITLE_LEN, doctor->title);
        copyText(target->specialty, DOCTOR_SPECIALTY_LEN, doctor->specialty);
        target->onDuty = doctor->onDuty;
        target->departmentId = doctor->departmentId;
        copyText(target->phone, DOCTOR_PHONE_LEN, doctor->phone);
        copyText(target->gender, sizeof(target->gender), doctor->gender);
        target->age = doctor->age;
        copyText(target->type, DOCTOR_SPECIALTY_LEN, doctor->type);
        target->bedId = doctor->bedId;
        fillDepartmentName(target);
        recalculateDepartmentDoctorCounts();

        addChangeLogEntry("医生管理", targetName, content);
        return;
    }
}

// 打印单个医生的表格行。
void printDoctor(const Doctor* doctor) {
    char doctorId[16];
    char age[16];
    char departmentId[16];
    const char* cells[10];

    if (doctor == NULL) return;
    snprintf(doctorId, sizeof(doctorId), "%d", doctor->doctorId);
    snprintf(age, sizeof(age), "%d", doctor->age);
    snprintf(departmentId, sizeof(departmentId), "%d", doctor->departmentId);
    cells[0] = doctorId;
    cells[1] = doctor->name;
    cells[2] = doctor->gender;
    cells[3] = age;
    cells[4] = doctor->phone;
    cells[5] = doctor->title;
    cells[6] = doctor->specialty;
    cells[7] = departmentId;
    cells[8] = doctor->departmentName;
    cells[9] = doctor->onDuty ? "在岗" : "停诊";
    printTableRow(cells, DOCTOR_TABLE_WIDTHS, 10);
}

// 打印医生列表表头。
static void printDoctorHeader() {
    printTableBorder(DOCTOR_TABLE_WIDTHS, 10);
    printTableRow(DOCTOR_TABLE_HEADERS, DOCTOR_TABLE_WIDTHS, 10);
    printTableBorder(DOCTOR_TABLE_WIDTHS, 10);
}

// 打印全部医生信息。
void printAllDoctors() {
    Doctor* current = doctorListHead;

    if (current == NULL) {
        printf("当前无医生数据。\n");
        return;
    }

    printf("\n================ 医生列表 ================\n");
    printDoctorHeader();
    while (current != NULL) {
        fillDepartmentName(current);
        printDoctor(current);
        current = current->next;
    }
    printTableBorder(DOCTOR_TABLE_WIDTHS, 10);
}

// 打印指定科室下的医生。
void printDoctorsByDepartment(int departmentId) {
    Doctor* current = doctorListHead;
    int found = 0;

    printf("\n========== 科室 %d 医生列表 ==========\n", departmentId);
    printDoctorHeader();
    while (current != NULL) {
        if (current->departmentId == departmentId) {
            fillDepartmentName(current);
            printDoctor(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(该科室暂无医生)\n");
    printTableBorder(DOCTOR_TABLE_WIDTHS, 10);
}

// 统计医生总数。
int countDoctors() {
    Doctor* current = doctorListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 统计某个科室下的医生数量。
int countDoctorsByDepartment(int departmentId) {
    Doctor* current = doctorListHead;
    int count = 0;

    while (current != NULL) {
        if (current->departmentId == departmentId) count++;
        current = current->next;
    }
    return count;
}

// 保存医生链表到文本文件。
void saveDoctorsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    Doctor* current = doctorListHead;

    if (!file) return;
    while (current != NULL) {
        fillDepartmentName(current);
        fprintf(file, "%d|%s|%s|%s|%s|%d|%d|%s|%s|%s|%d|%s|%d\n",
            current->doctorId, current->idCard, current->name, current->title,
            current->specialty, current->onDuty, current->departmentId,
            current->departmentName, current->phone, current->gender,
            current->age, current->type, current->bedId);
        current = current->next;
    }
    fclose(file);
}

// 解析一行医生数据。
static int parseDoctorLine(char* line, Doctor* d) {
    char* fields[13];

    // 先读完整的新格式；旧格式缺少的字段用默认值补齐。
    if (strchr(line, '|') != NULL) {
        if (splitFields(line, fields, 13) != 13) return 0;
        if (!parseIntField(fields[0], &d->doctorId)) return 0;
        copyText(d->idCard, DOCTOR_ID_CARD_LEN, fields[1]);
        copyText(d->name, DOCTOR_NAME_LEN, fields[2]);
        copyText(d->title, DOCTOR_TITLE_LEN, fields[3]);
        copyText(d->specialty, DOCTOR_SPECIALTY_LEN, fields[4]);
        if (!parseIntField(fields[5], &d->onDuty)) return 0;
        if (!parseIntField(fields[6], &d->departmentId)) return 0;
        copyText(d->departmentName, DOCTOR_DEPARTMENT_LEN, fields[7]);
        copyText(d->phone, DOCTOR_PHONE_LEN, fields[8]);
        copyText(d->gender, sizeof(d->gender), fields[9]);
        if (!parseIntField(fields[10], &d->age)) return 0;
        copyText(d->type, DOCTOR_SPECIALTY_LEN, fields[11]);
        if (!parseIntField(fields[12], &d->bedId)) return 0;
        return 1;
    }

    if (sscanf(line, "%d %49s %9s %d %19s %49s %29s %d %d",
        &d->doctorId, d->name, d->gender, &d->age, d->phone,
        d->type, d->title, &d->bedId, &d->departmentId) == 9) {
        copyText(d->idCard, DOCTOR_ID_CARD_LEN, "none");
        copyText(d->specialty, DOCTOR_SPECIALTY_LEN, d->type);
        d->onDuty = 1;
        fillDepartmentName(d);
        return 1;
    }
    return 0;
}

// 从文件加载医生数据。
void loadDoctorsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[512];
    int lineNo = 0;

    initDoctorList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        Doctor* d;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        d = (Doctor*)malloc(sizeof(Doctor));
        if (d == NULL) break;
        memset(d, 0, sizeof(Doctor));
        if (!parseDoctorLine(line, d)) {
            printf("跳过医生数据第 %d 行：格式错误。\n", lineNo);
            free(d);
            continue;
        }
        d->next = NULL;
        addDoctor(d);
    }
    fclose(file);
    recalculateDepartmentDoctorCounts();
}
