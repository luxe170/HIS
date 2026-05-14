#define _CRT_SECURE_NO_WARNINGS
#include "bed.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 床位是住院流程里最小的资源单位。
 * 患者办理住院时占用空床，办理出院时释放床位并清掉患者绑定。
 */

Bed* bedListHead = NULL;

static const int BED_TABLE_WIDTHS[] = { 8, 10, 8, 18 };
static const char* BED_TABLE_HEADERS[] = {
    "床位ID", "病房ID", "状态", "患者ID"
};

// 初始化床位链表。
void initBedList() {
    bedListHead = NULL;
}

// 将床位节点追加到链表尾部。
void addBed(Bed* p) {
    Bed* current;

    if (p == NULL) return;
    p->next = NULL;
    if (bedListHead == NULL) {
        bedListHead = p;
        return;
    }
    current = bedListHead;
    while (current->next != NULL) current = current->next;
    current->next = p;
}

// 按床位 ID 删除床位节点。
int deleteBedById(int bedId) {
    Bed* current = bedListHead;
    Bed* previous = NULL;

    while (current != NULL) {
        if (current->bedId == bedId) {
            if (previous == NULL) bedListHead = current->next;
            else previous->next = current->next;
            free(current);
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return bedListHead == NULL ? -1 : 0;
}

// 按床位 ID 查找床位。
Bed* searchBedById(int bedId) {
    Bed* current = bedListHead;

    while (current != NULL) {
        if (current->bedId == bedId) return current;
        current = current->next;
    }
    return NULL;
}

// 用传入的床位对象覆盖链表中的同 ID 床位。
void modifyBed(Bed* bed) {
    Bed* target;

    if (bed == NULL) return;
    target = searchBedById(bed->bedId);
    if (target == NULL) return;
    target->relatedWardId = bed->relatedWardId;
    target->status = bed->status;
    copyText(target->relatedPatientId, PATIENT_ID_LEN, bed->relatedPatientId);
}

// 打印单张床位的简要信息。
void printBed(const Bed* bed) {
    char bedId[16];
    char wardId[16];
    const char* cells[4];

    if (bed == NULL) return;
    snprintf(bedId, sizeof(bedId), "%d", bed->bedId);
    snprintf(wardId, sizeof(wardId), "%d", bed->relatedWardId);
    cells[0] = bedId;
    cells[1] = wardId;
    cells[2] = bed->status == 0 ? "空闲" : "占用";
    cells[3] = bed->relatedPatientId;
    printTableRow(cells, BED_TABLE_WIDTHS, 4);
}

static void printBedHeader() {
    printTableBorder(BED_TABLE_WIDTHS, 4);
    printTableRow(BED_TABLE_HEADERS, BED_TABLE_WIDTHS, 4);
    printTableBorder(BED_TABLE_WIDTHS, 4);
}

// 打印全部床位列表。
void printAllBeds() {
    Bed* current = bedListHead;

    if (current == NULL) {
        printf("当前无床位数据。\n");
        return;
    }
    printf("\n================ 床位列表 ================\n");
    printBedHeader();
    while (current != NULL) {
        printBed(current);
        current = current->next;
    }
    printTableBorder(BED_TABLE_WIDTHS, 4);
}

// 统计床位总数。
int countBeds() {
    Bed* current = bedListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 统计某个病房下的床位数。
int countBedsByWard(int wardId) {
    Bed* current = bedListHead;
    int count = 0;

    while (current != NULL) {
        if (current->relatedWardId == wardId) count++;
        current = current->next;
    }
    return count;
}

// 统计某个病房已经占用的床位数。
int countOccupiedBedsByWard(int wardId) {
    Bed* current = bedListHead;
    int count = 0;

    while (current != NULL) {
        if (current->relatedWardId == wardId && current->status == 1) count++;
        current = current->next;
    }
    return count;
}

// 根据现有最大床位号生成下一个床位 ID。
int generateNextBedId() {
    Bed* current = bedListHead;
    int maxId = 30000000;

    while (current != NULL) {
        if (current->bedId > maxId) maxId = current->bedId;
        current = current->next;
    }
    return maxId + 1;
}

// 保存床位链表到文本文件。
void saveBedsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    Bed* current = bedListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%d|%d|%d|%s\n",
            current->bedId, current->relatedWardId, current->status, current->relatedPatientId);
        current = current->next;
    }
    fclose(file);
}

// 解析一行床位数据。
static int parseBedLine(char* line, Bed* b) {
    char* fields[4];

    // 床位数据没有复杂文本，仍保持和其他表一致的 | 分隔。
    if (strchr(line, '|') != NULL) {
        if (splitFields(line, fields, 4) != 4) return 0;
        if (!parseIntField(fields[0], &b->bedId)) return 0;
        if (!parseIntField(fields[1], &b->relatedWardId)) return 0;
        if (!parseIntField(fields[2], &b->status)) return 0;
        copyText(b->relatedPatientId, PATIENT_ID_LEN, fields[3]);
        return 1;
    }

    return sscanf(line, "%d %d %d %19s",
        &b->bedId, &b->relatedWardId, &b->status, b->relatedPatientId) == 4;
}

// 从文件加载床位数据。
void loadBedsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[256];
    int lineNo = 0;

    initBedList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        Bed* b;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        b = (Bed*)malloc(sizeof(Bed));
        if (b == NULL) break;
        memset(b, 0, sizeof(Bed));
        if (!parseBedLine(line, b)) {
            printf("跳过床位数据第 %d 行：格式错误。\n", lineNo);
            free(b);
            continue;
        }
        b->next = NULL;
        addBed(b);
    }
    fclose(file);
}
