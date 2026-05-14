#define _CRT_SECURE_NO_WARNINGS
#include "medicine.h"
#include "util.h"
#include "change_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 药品模块维护库存本身；谁给谁发药、何时发药由药品流水模块补充记录。
 * 这样库存计算和审计记录分开，答辩时也容易说明职责边界。
 */

Medicine* medicineListHead = NULL;

static const int MEDICINE_TABLE_WIDTHS[] = { 8, 14, 14, 14, 12, 6, 8, 8, 10 };
static const char* MEDICINE_TABLE_HEADERS[] = {
    "药品ID", "通用名", "商品名", "别名", "类别", "单位", "库存", "单价", "科室ID"
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

static void buildMedicineTargetName(const Medicine* medicine, char* buffer, int bufferSize) {
    if (buffer == NULL || bufferSize <= 0) return;
    if (medicine != NULL && medicine->genericName[0] != '\0') {
        snprintf(buffer, bufferSize, "药品 %s", medicine->genericName);
    } else if (medicine != NULL) {
        snprintf(buffer, bufferSize, "药品ID %d", medicine->medicineId);
    } else {
        copyText(buffer, bufferSize, "药品");
    }
}

// 初始化药品链表。
void initMedicineList() {
    medicineListHead = NULL;
}

// 将药品节点追加到链表尾部。
void addMedicine(Medicine* p) {
    Medicine* current;

    if (p == NULL) return;
    p->next = NULL;
    if (medicineListHead == NULL) {
        medicineListHead = p;
        return;
    }
    current = medicineListHead;
    while (current->next != NULL) current = current->next;
    current->next = p;

    {
        char target[CHANGE_LOG_TARGET_LEN];
        buildMedicineTargetName(p, target, sizeof(target));
        addChangeLogEntry("药品管理", target, "新增药品资料");
    }
}

// 按药品 ID 删除药品。
int deleteMedicineById(int medicineId) {
    Medicine* current = medicineListHead;
    Medicine* previous = NULL;

    while (current != NULL) {
        if (current->medicineId == medicineId) {
            Medicine snapshot = *current;
            if (previous == NULL) medicineListHead = current->next;
            else previous->next = current->next;
            free(current);
            {
                char target[CHANGE_LOG_TARGET_LEN];
                buildMedicineTargetName(&snapshot, target, sizeof(target));
                addChangeLogEntry("药品管理", target, "删除药品资料");
            }
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return medicineListHead == NULL ? -1 : 0;
}

// 按药品 ID 查找药品。
Medicine* searchMedicineById(int medicineId) {
    Medicine* current = medicineListHead;

    while (current != NULL) {
        if (current->medicineId == medicineId) return current;
        current = current->next;
    }
    return NULL;
}

// 判断关键字是否命中药品的通用名、商品名或别名。
static int nameMatches(const Medicine* medicine, const char* keyword) {
    if (medicine == NULL || keyword == NULL || keyword[0] == '\0') return 0;
    return strstr(medicine->genericName, keyword) != NULL ||
        strstr(medicine->brandName, keyword) != NULL ||
        strstr(medicine->aliasName, keyword) != NULL;
}

// 按名称关键字查找第一个匹配药品。
Medicine* searchMedicineByName(char* medicineName) {
    Medicine* current = medicineListHead;

    while (current != NULL) {
        if (nameMatches(current, medicineName)) return current;
        current = current->next;
    }
    return NULL;
}

// 修改药品基础资料和库存信息。
void modifyMedicine(Medicine* medicine) {
    Medicine* target;

    if (medicine == NULL) return;
    target = searchMedicineById(medicine->medicineId);
    if (target == NULL) return;

    {
        Medicine before = *target;
        char content[CHANGE_LOG_CONTENT_LEN];
        char targetName[CHANGE_LOG_TARGET_LEN];

        content[0] = '\0';
        appendFieldChange(content, sizeof(content), "通用名", before.genericName, medicine->genericName);
        appendFieldChange(content, sizeof(content), "商品名", before.brandName, medicine->brandName);
        appendFieldChange(content, sizeof(content), "别名", before.aliasName, medicine->aliasName);
        appendFieldChange(content, sizeof(content), "类别", before.category, medicine->category);
        appendFieldChange(content, sizeof(content), "单位", before.unit, medicine->unit);
        appendIntChange(content, sizeof(content), "库存", before.inventory, medicine->inventory);
        appendIntChange(content, sizeof(content), "单价", before.price, medicine->price);
        appendIntChange(content, sizeof(content), "科室ID", before.relatedDepartmentId, medicine->relatedDepartmentId);

        if (content[0] == '\0') copyText(content, sizeof(content), "更新药品信息");
        buildMedicineTargetName(medicine, targetName, sizeof(targetName));

        copyText(target->genericName, MEDICINE_NAME_LEN, medicine->genericName);
        copyText(target->brandName, MEDICINE_NAME_LEN, medicine->brandName);
        copyText(target->aliasName, MEDICINE_NAME_LEN, medicine->aliasName);
        copyText(target->category, MEDICINE_CATEGORY_LEN, medicine->category);
        copyText(target->unit, MEDICINE_UNIT_LEN, medicine->unit);
        target->inventory = medicine->inventory;
        target->price = medicine->price;
        target->relatedDepartmentId = medicine->relatedDepartmentId;

        addChangeLogEntry("药品管理", targetName, content);
        return;
    }
}

// 打印单个药品的表格行。
void printMedicine(const Medicine* medicine) {
    char medicineId[16];
    char inventory[16];
    char price[16];
    char departmentId[16];
    const char* cells[9];

    if (medicine == NULL) return;
    snprintf(medicineId, sizeof(medicineId), "%d", medicine->medicineId);
    snprintf(inventory, sizeof(inventory), "%d", medicine->inventory);
    snprintf(price, sizeof(price), "%d", medicine->price);
    snprintf(departmentId, sizeof(departmentId), "%d", medicine->relatedDepartmentId);
    cells[0] = medicineId;
    cells[1] = medicine->genericName;
    cells[2] = medicine->brandName;
    cells[3] = medicine->aliasName;
    cells[4] = medicine->category;
    cells[5] = medicine->unit;
    cells[6] = inventory;
    cells[7] = price;
    cells[8] = departmentId;
    printTableRow(cells, MEDICINE_TABLE_WIDTHS, 9);
}

// 打印药品列表表头。
void printMedicineTableHeader() {
    printTableBorder(MEDICINE_TABLE_WIDTHS, 9);
    printTableRow(MEDICINE_TABLE_HEADERS, MEDICINE_TABLE_WIDTHS, 9);
    printTableBorder(MEDICINE_TABLE_WIDTHS, 9);
}

void printMedicineTableBorder() {
    printTableBorder(MEDICINE_TABLE_WIDTHS, 9);
}

// 打印全部药品信息。
void printAllMedicines() {
    Medicine* current = medicineListHead;

    if (current == NULL) {
        printf("当前无药品数据。\n");
        return;
    }
    printf("\n================ 药品列表 ================\n");
    printMedicineTableHeader();
    while (current != NULL) {
        printMedicine(current);
        current = current->next;
    }
    printMedicineTableBorder();
}

// 按名称关键字打印匹配药品。
int printMedicinesByName(const char* medicineName) {
    Medicine* current = medicineListHead;
    int found = 0;

    if (medicineName == NULL || medicineName[0] == '\0') return 0;
    printf("\n================ 药品名称查询 ================\n");
    printMedicineTableHeader();
    while (current != NULL) {
        if (nameMatches(current, medicineName)) {
            printMedicine(current);
            found++;
        }
        current = current->next;
    }
    if (!found) printf("(未找到名称包含 %s 的药品)\n", medicineName);
    printMedicineTableBorder();
    return found;
}

// 统计药品种类数量。
int countMedicines() {
    Medicine* current = medicineListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 给指定药品增加库存。
int addMedicineStock(int medicineId, int quantity) {
    Medicine* medicine = searchMedicineById(medicineId);
    int before;

    if (medicine == NULL) return -1;
    if (quantity <= 0) return -2;
    before = medicine->inventory;
    medicine->inventory += quantity;
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildMedicineTargetName(medicine, target, sizeof(target));
        snprintf(content, sizeof(content), "库存由 %d 改为 %d（入库 +%d）", before, medicine->inventory, quantity);
        addChangeLogEntry("药品管理", target, content);
    }
    return 1;
}

// 从指定药品库存中扣减数量。
int reduceMedicineStock(int medicineId, int quantity) {
    Medicine* medicine = searchMedicineById(medicineId);
    int before;

    if (medicine == NULL) return -1;
    if (quantity <= 0) return -2;
    if (medicine->inventory < quantity) return -3;
    // 库存先扣掉，流水在调用处补上患者和医生信息。
    before = medicine->inventory;
    medicine->inventory -= quantity;
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildMedicineTargetName(medicine, target, sizeof(target));
        snprintf(content, sizeof(content), "库存由 %d 改为 %d（出库 -%d）", before, medicine->inventory, quantity);
        addChangeLogEntry("药品管理", target, content);
    }
    return 1;
}

// 保存药品链表到文本文件。
void saveMedicinesToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    Medicine* current = medicineListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%d|%s|%s|%s|%s|%s|%d|%d|%d\n",
            current->medicineId, current->genericName, current->brandName,
            current->aliasName, current->category, current->unit,
            current->inventory, current->price, current->relatedDepartmentId);
        current = current->next;
    }
    fclose(file);
}

// 解析一行药品数据。
static int parseMedicineLine(char* line, Medicine* m) {
    char* fields[9];
    int count;

    // 支持 9 字段新格式，也兼容早期只有一个药名的 7 字段格式。
    if (strchr(line, '|') != NULL) {
        count = splitFields(line, fields, 9);
        if (count == 9) {
            if (!parseIntField(fields[0], &m->medicineId)) return 0;
            copyText(m->genericName, MEDICINE_NAME_LEN, fields[1]);
            copyText(m->brandName, MEDICINE_NAME_LEN, fields[2]);
            copyText(m->aliasName, MEDICINE_NAME_LEN, fields[3]);
            copyText(m->category, MEDICINE_CATEGORY_LEN, fields[4]);
            copyText(m->unit, MEDICINE_UNIT_LEN, fields[5]);
            return parseIntField(fields[6], &m->inventory)
                && parseIntField(fields[7], &m->price)
                && parseIntField(fields[8], &m->relatedDepartmentId);
        }
        if (count == 7) {
            if (!parseIntField(fields[0], &m->medicineId)) return 0;
            copyText(m->genericName, MEDICINE_NAME_LEN, fields[1]);
            copyText(m->brandName, MEDICINE_NAME_LEN, fields[1]);
            copyText(m->aliasName, MEDICINE_NAME_LEN, fields[1]);
            copyText(m->category, MEDICINE_CATEGORY_LEN, fields[2]);
            copyText(m->unit, MEDICINE_UNIT_LEN, fields[3]);
            return parseIntField(fields[4], &m->inventory)
                && parseIntField(fields[5], &m->price)
                && parseIntField(fields[6], &m->relatedDepartmentId);
        }
        return 0;
    }

    if (sscanf(line, "%d %49s %49s %49s %19s %9s %d %d %d",
        &m->medicineId, m->genericName, m->brandName, m->aliasName,
        m->category, m->unit, &m->inventory, &m->price,
        &m->relatedDepartmentId) == 9) {
        return 1;
    }

    if (sscanf(line, "%d %49s %19s %9s %d %d %d",
        &m->medicineId, m->genericName, m->category, m->unit,
        &m->inventory, &m->price, &m->relatedDepartmentId) == 7) {
        copyText(m->brandName, MEDICINE_NAME_LEN, m->genericName);
        copyText(m->aliasName, MEDICINE_NAME_LEN, m->genericName);
        return 1;
    }
    return 0;
}

// 从文件加载药品数据。
void loadMedicinesFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[512];
    int lineNo = 0;

    initMedicineList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        Medicine* m;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        m = (Medicine*)malloc(sizeof(Medicine));
        if (m == NULL) break;
        memset(m, 0, sizeof(Medicine));
        if (!parseMedicineLine(line, m)) {
            printf("跳过药品数据第 %d 行：格式错误。\n", lineNo);
            free(m);
            continue;
        }
        m->next = NULL;
        addMedicine(m);
    }
    fclose(file);
}
