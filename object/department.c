#define _CRT_SECURE_NO_WARNINGS
#include "department.h"
#include "doctor.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 科室模块保存基础科室表，并提供“每科至少 3 名医生”的检查。
 * 这个规则适合作为答辩里的业务约束示例。
 */

Department* departmentListHead = NULL;

static const int DEPARTMENT_TABLE_WIDTHS[] = { 10, 18, 10, 8 };
static const char* DEPARTMENT_TABLE_HEADERS[] = {
    "科室ID", "科室名称", "医生数", "状态"
};

// 初始化科室链表。
void initDepartmentList() {
    departmentListHead = NULL;
}

// 将科室节点追加到链表尾部。
void addDepartment(Department* p) {
    Department* current;

    if (p == NULL) return;
    p->next = NULL;
    if (departmentListHead == NULL) {
        departmentListHead = p;
        return;
    }
    current = departmentListHead;
    while (current->next != NULL) current = current->next;
    current->next = p;
}

// 按科室 ID 删除科室。
int deleteDepartmentById(int departmentId) {
    Department* current = departmentListHead;
    Department* previous = NULL;

    while (current != NULL) {
        if (current->departmentId == departmentId) {
            if (previous == NULL) departmentListHead = current->next;
            else previous->next = current->next;
            free(current);
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return departmentListHead == NULL ? -1 : 0;
}

// 按科室 ID 查找科室。
Department* searchDepartmentById(int departmentId) {
    Department* current = departmentListHead;

    while (current != NULL) {
        if (current->departmentId == departmentId) return current;
        current = current->next;
    }
    return NULL;
}

// 修改指定科室的基础信息。
void modifyDepartment(Department* department) {
    Department* target;

    if (department == NULL) return;
    target = searchDepartmentById(department->departmentId);
    if (target == NULL) return;
    copyText(target->name, DEPARTMENT_NAME_LEN, department->name);
    target->numberOfDoctors = department->numberOfDoctors;
}

// 根据医生链表重新统计每个科室的人数。
void recalculateDepartmentDoctorCounts() {
    Department* dep = departmentListHead;
    Doctor* doctor;

    while (dep != NULL) {
        dep->numberOfDoctors = 0;
        dep = dep->next;
    }

    doctor = doctorListHead;
    while (doctor != NULL) {
        dep = searchDepartmentById(doctor->departmentId);
        if (dep != NULL) dep->numberOfDoctors++;
        doctor = doctor->next;
    }
}

// 打印单个科室及医生数量状态。
void printDepartment(const Department* dep) {
    const char* status;
    char departmentId[16];
    char doctorCount[16];
    const char* cells[4];

    if (dep == NULL) return;
    status = dep->numberOfDoctors >= 3 ? "达标" : "不足";
    snprintf(departmentId, sizeof(departmentId), "%d", dep->departmentId);
    snprintf(doctorCount, sizeof(doctorCount), "%d", dep->numberOfDoctors);
    cells[0] = departmentId;
    cells[1] = dep->name;
    cells[2] = doctorCount;
    cells[3] = status;
    printTableRow(cells, DEPARTMENT_TABLE_WIDTHS, 4);
}

static void printDepartmentHeader() {
    printTableBorder(DEPARTMENT_TABLE_WIDTHS, 4);
    printTableRow(DEPARTMENT_TABLE_HEADERS, DEPARTMENT_TABLE_WIDTHS, 4);
    printTableBorder(DEPARTMENT_TABLE_WIDTHS, 4);
}

// 打印所有科室列表。
void printAllDepartments() {
    Department* current;

    recalculateDepartmentDoctorCounts();
    current = departmentListHead;
    if (current == NULL) {
        printf("当前无科室数据。\n");
        return;
    }

    printf("\n================ 科室列表 ================\n");
    printDepartmentHeader();
    while (current != NULL) {
        printDepartment(current);
        current = current->next;
    }
    printTableBorder(DEPARTMENT_TABLE_WIDTHS, 4);
}

// 检查各科室是否满足至少三名医生。
void printDepartmentCompliance() {
    Department* current;
    int ok = 1;

    recalculateDepartmentDoctorCounts();
    current = departmentListHead;
    printf("\n========== 科室医生数量检查 ==========\n");
    printDepartmentHeader();
    while (current != NULL) {
        printDepartment(current);
        if (current->numberOfDoctors < 3) ok = 0;
        current = current->next;
    }
    printTableBorder(DEPARTMENT_TABLE_WIDTHS, 4);
    printf("检查结论：%s\n", ok ? "所有科室均满足至少 3 名医生。" : "存在医生数量不足 3 名的科室。");
}

// 统计科室总数。
int countDepartments() {
    Department* current = departmentListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 保存科室数据到文本文件。
void saveDepartmentsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    Department* current = departmentListHead;

    if (!file) return;
    recalculateDepartmentDoctorCounts();
    while (current != NULL) {
        fprintf(file, "%d|%s|%d\n", current->departmentId, current->name, current->numberOfDoctors);
        current = current->next;
    }
    fclose(file);
}

// 解析一行科室数据。
static int parseDepartmentLine(char* line, Department* dep) {
    char* fields[3];

    // 科室数据很短，但仍统一按 | 格式保存，后面兼容旧空格格式。
    if (strchr(line, '|') != NULL) {
        if (splitFields(line, fields, 3) != 3) return 0;
        if (!parseIntField(fields[0], &dep->departmentId)) return 0;
        copyText(dep->name, DEPARTMENT_NAME_LEN, fields[1]);
        if (!parseIntField(fields[2], &dep->numberOfDoctors)) return 0;
        return 1;
    }

    return sscanf(line, "%d %49s %d", &dep->departmentId, dep->name, &dep->numberOfDoctors) == 3;
}

// 从文件加载科室数据。
void loadDepartmentsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[256];
    int lineNo = 0;

    initDepartmentList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        Department* dep;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        dep = (Department*)malloc(sizeof(Department));
        if (dep == NULL) break;
        memset(dep, 0, sizeof(Department));
        if (!parseDepartmentLine(line, dep)) {
            printf("跳过科室数据第 %d 行：格式错误。\n", lineNo);
            free(dep);
            continue;
        }
        dep->next = NULL;
        addDepartment(dep);
    }
    fclose(file);
}
