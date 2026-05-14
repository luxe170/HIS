#define _CRT_SECURE_NO_WARNINGS
#include "ward.h"
#include "bed.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 病房只保存总床位和所属科室，真正的占用情况以床位链表为准。
 * 展示病房前会重新统计一次，避免床位变化后数据不同步。
 */

Ward* wardListHead = NULL;

static const int WARD_TABLE_WIDTHS[] = { 8, 10, 10, 8, 8, 8 };
static const char* WARD_TABLE_HEADERS[] = {
    "病房ID", "类型", "科室ID", "总床位", "可用", "占用率"
};

// 初始化病房链表。
void initWardList() {
    wardListHead = NULL;
}

// 将病房节点追加到链表尾部。
void addWard(Ward* p) {
    Ward* current;

    if (p == NULL) return;
    p->next = NULL;
    if (wardListHead == NULL) {
        wardListHead = p;
        return;
    }
    current = wardListHead;
    while (current->next != NULL) current = current->next;
    current->next = p;
}

// 按病房 ID 删除病房。
int deleteWardById(int wardId) {
    Ward* current = wardListHead;
    Ward* previous = NULL;

    while (current != NULL) {
        if (current->wardId == wardId) {
            if (previous == NULL) wardListHead = current->next;
            else previous->next = current->next;
            free(current);
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return wardListHead == NULL ? -1 : 0;
}

// 按病房 ID 查找病房。
Ward* searchWardById(int wardId) {
    Ward* current = wardListHead;

    while (current != NULL) {
        if (current->wardId == wardId) return current;
        current = current->next;
    }
    return NULL;
}

// 修改病房基础信息。
void modifyWard(Ward* ward) {
    Ward* target;

    if (ward == NULL) return;
    target = searchWardById(ward->wardId);
    if (target == NULL) return;
    copyText(target->typeOfWard, WARD_TYPE_LEN, ward->typeOfWard);
    target->relatedDepartmentId = ward->relatedDepartmentId;
    target->totalBedOfWard = ward->totalBedOfWard;
    target->availableBedOfWard = ward->availableBedOfWard;
}

// 根据床位链表重新计算每个病房的可用床位数。
void recalculateWardAvailability() {
    Ward* ward = wardListHead;

    while (ward != NULL) {
        int occupied = countOccupiedBedsByWard(ward->wardId);
        int actualBeds = countBedsByWard(ward->wardId);
        if (actualBeds > ward->totalBedOfWard) ward->totalBedOfWard = actualBeds;
        ward->availableBedOfWard = ward->totalBedOfWard - occupied;
        if (ward->availableBedOfWard < 0) ward->availableBedOfWard = 0;
        ward = ward->next;
    }
}

// 打印单个病房及床位占用率。
void printWard(const Ward* ward) {
    double occupancy = 0.0;
    char wardId[16];
    char departmentId[16];
    char totalBeds[16];
    char availableBeds[16];
    char occupancyText[16];
    const char* cells[6];

    if (ward == NULL) return;
    if (ward->totalBedOfWard > 0) {
        occupancy = (double)(ward->totalBedOfWard - ward->availableBedOfWard) * 100.0 / ward->totalBedOfWard;
    }
    snprintf(wardId, sizeof(wardId), "%d", ward->wardId);
    snprintf(departmentId, sizeof(departmentId), "%d", ward->relatedDepartmentId);
    snprintf(totalBeds, sizeof(totalBeds), "%d", ward->totalBedOfWard);
    snprintf(availableBeds, sizeof(availableBeds), "%d", ward->availableBedOfWard);
    snprintf(occupancyText, sizeof(occupancyText), "%.1f%%", occupancy);
    cells[0] = wardId;
    cells[1] = ward->typeOfWard;
    cells[2] = departmentId;
    cells[3] = totalBeds;
    cells[4] = availableBeds;
    cells[5] = occupancyText;
    printTableRow(cells, WARD_TABLE_WIDTHS, 6);
}

static void printWardHeader() {
    printTableBorder(WARD_TABLE_WIDTHS, 6);
    printTableRow(WARD_TABLE_HEADERS, WARD_TABLE_WIDTHS, 6);
    printTableBorder(WARD_TABLE_WIDTHS, 6);
}

// 打印全部病房列表。
void printAllWards() {
    Ward* current;

    recalculateWardAvailability();
    current = wardListHead;
    if (current == NULL) {
        printf("当前无病房数据。\n");
        return;
    }
    printf("\n================ 病房列表 ================\n");
    printWardHeader();
    while (current != NULL) {
        printWard(current);
        current = current->next;
    }
    printTableBorder(WARD_TABLE_WIDTHS, 6);
}

// 统计病房总数。
int countWards() {
    Ward* current = wardListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 统计病房类型数量。
int countWardTypes() {
    Ward* outer = wardListHead;
    int count = 0;

    while (outer != NULL) {
        Ward* inner = wardListHead;
        int seen = 0;
        while (inner != outer) {
            if (strcmp(inner->typeOfWard, outer->typeOfWard) == 0) {
                seen = 1;
                break;
            }
            inner = inner->next;
        }
        if (!seen) count++;
        outer = outer->next;
    }
    return count;
}

// 保存病房链表到文本文件。
void saveWardsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    Ward* current = wardListHead;

    if (!file) return;
    recalculateWardAvailability();
    while (current != NULL) {
        fprintf(file, "%d|%s|%d|%d|%d\n",
            current->wardId, current->typeOfWard, current->relatedDepartmentId,
            current->totalBedOfWard, current->availableBedOfWard);
        current = current->next;
    }
    fclose(file);
}

// 解析一行病房数据。
static int parseWardLine(char* line, Ward* w) {
    char* fields[5];

    // 病房字段固定，读文件时先尝试当前的 | 分隔格式。
    if (strchr(line, '|') != NULL) {
        if (splitFields(line, fields, 5) != 5) return 0;
        if (!parseIntField(fields[0], &w->wardId)) return 0;
        copyText(w->typeOfWard, WARD_TYPE_LEN, fields[1]);
        return parseIntField(fields[2], &w->relatedDepartmentId)
            && parseIntField(fields[3], &w->totalBedOfWard)
            && parseIntField(fields[4], &w->availableBedOfWard);
    }

    return sscanf(line, "%d %19s %d %d %d",
        &w->wardId, w->typeOfWard, &w->relatedDepartmentId,
        &w->totalBedOfWard, &w->availableBedOfWard) == 5;
}

// 从文件加载病房数据。
void loadWardsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[256];
    int lineNo = 0;

    initWardList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        Ward* w;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        w = (Ward*)malloc(sizeof(Ward));
        if (w == NULL) break;
        memset(w, 0, sizeof(Ward));
        if (!parseWardLine(line, w)) {
            printf("跳过病房数据第 %d 行：格式错误。\n", lineNo);
            free(w);
            continue;
        }
        w->next = NULL;
        addWard(w);
    }
    fclose(file);
}
