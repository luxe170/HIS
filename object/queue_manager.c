#define _CRT_SECURE_NO_WARNINGS
#include "queue_manager.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 候诊队列不单独落文件，启动时从“待叫号”的病历记录重建。
 * 这样保存一份病历就够了，避免队列文件和病历文件互相打架。
 */

typedef struct QueueEntry {
    // 队列里只放排序需要的字段，完整信息回病历链表查。
    int recordId;
    int doctorId;
    int regType;
    char appointmentDate[MEDICAL_RECORD_DATE];
    int appointmentSlot;
    char registrationTime[MEDICAL_RECORD_TIME];
    int queueNumber;
} QueueEntry;

typedef struct QueueNode {
    QueueEntry data;
    struct QueueNode* next;
} QueueNode;

static QueueNode* gQueueHead = NULL;

static const int WAITING_QUEUE_WIDTHS[] = { 8, 8, 8, 12, 8 };
static const char* WAITING_QUEUE_HEADERS[] = {
    "记录ID", "排队号", "类型", "日期", "时段"
};
static const int CALLED_RECORD_WIDTHS[] = { 8, 18, 8, 19 };
static const char* CALLED_RECORD_HEADERS[] = {
    "医生ID", "患者ID", "记录ID", "时间"
};

// 把时段编号转成显示文字。
static const char* slotToText(int slot) {
    if (slot == SLOT_MORNING) return "上午";
    if (slot == SLOT_AFTERNOON) return "下午";
    if (slot == SLOT_EVENING) return "晚上";
    return "未知";
}

// 判断队列项 a 是否应该排在 b 前面。
static int shouldComeBefore(const QueueEntry* a, const QueueEntry* b) {
    int timeCompare = strcmp(a->registrationTime, b->registrationTime);

    // 排序规则：先按医生分组，再按挂号记录时间、排队号。
    if (a->doctorId != b->doctorId) return a->doctorId < b->doctorId;
    if (timeCompare != 0) return timeCompare < 0;
    return a->queueNumber < b->queueNumber;
}

// 释放候诊队列中的所有节点。
static void clearQueue() {
    QueueNode* current = gQueueHead;

    while (current != NULL) {
        QueueNode* next = current->next;
        free(current);
        current = next;
    }
    gQueueHead = NULL;
}

// 初始化候诊队列。
void initQueueManager() {
    clearQueue();
}

// 把待叫号病历插入候诊队列。
int enqueueMedicalRecord(const MedicalRecord* record) {
    QueueNode* node;
    QueueNode* current;
    QueueNode* previous = NULL;

    if (record == NULL) return 0;
    if (record->status != RECORD_STATUS_WAITING) return 1;

    node = (QueueNode*)malloc(sizeof(QueueNode));
    if (node == NULL) return 0;
    node->data.recordId = record->medicalRecordId;
    node->data.doctorId = record->relatedDoctorId;
    node->data.regType = record->regType;
    copyText(node->data.appointmentDate, MEDICAL_RECORD_DATE, record->appointmentDate);
    node->data.appointmentSlot = record->appointmentSlot;
    copyText(node->data.registrationTime, MEDICAL_RECORD_TIME, record->time);
    node->data.queueNumber = record->queueNumber;
    node->next = NULL;

    // 按挂号记录时间插到医生自己的候诊队列里。
    current = gQueueHead;
    while (current != NULL && shouldComeBefore(&current->data, &node->data)) {
        previous = current;
        current = current->next;
    }
    if (previous == NULL) {
        node->next = gQueueHead;
        gQueueHead = node;
    }
    else {
        node->next = current;
        previous->next = node;
    }
    return 1;
}

// 从病历链表重新构建候诊队列。
void rebuildQueueFromMedicalRecords() {
    MedicalRecord* current;

    // 删除、修改或重新读文件后，直接从病历状态恢复队列。
    initQueueManager();
    current = medicalRecordListHead;
    while (current != NULL) {
        enqueueMedicalRecord(current);
        current = current->next;
    }
}

// 取出某医生下一位候诊患者的病历 ID。
int dequeueNextRecordIdByDoctor(int doctorId) {
    QueueNode* current = gQueueHead;
    QueueNode* previous = NULL;
    int recordId;

    while (current != NULL) {
        if (current->data.doctorId == doctorId) {
            recordId = current->data.recordId;
            if (previous == NULL) gQueueHead = current->next;
            else previous->next = current->next;
            free(current);
            return recordId;
        }
        previous = current;
        current = current->next;
    }
    return -1;
}

// 打印某医生的候诊队列。
void printWaitingQueueByDoctor(int doctorId) {
    QueueNode* current = gQueueHead;
    int found = 0;

    printf("\n========== 医生 %d 候诊队列 ==========\n", doctorId);
    printTableBorder(WAITING_QUEUE_WIDTHS, 5);
    printTableRow(WAITING_QUEUE_HEADERS, WAITING_QUEUE_WIDTHS, 5);
    printTableBorder(WAITING_QUEUE_WIDTHS, 5);
    while (current != NULL) {
        if (current->data.doctorId == doctorId) {
            char recordId[16];
            char queueNumber[16];
            const char* cells[5];

            snprintf(recordId, sizeof(recordId), "%d", current->data.recordId);
            snprintf(queueNumber, sizeof(queueNumber), "%d", current->data.queueNumber);
            cells[0] = recordId;
            cells[1] = queueNumber;
            cells[2] = current->data.regType == REG_TYPE_APPOINTMENT ? "预约" : "现场";
            cells[3] = current->data.appointmentDate;
            cells[4] = slotToText(current->data.appointmentSlot);
            printTableRow(cells, WAITING_QUEUE_WIDTHS, 5);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(当前无候诊患者)\n");
    printTableBorder(WAITING_QUEUE_WIDTHS, 5);
}

// 打印当前已叫号未就诊的患者。
void printCurrentCalledPatients() {
    MedicalRecord* current = medicalRecordListHead;
    int found = 0;

    printf("\n========== 当前叫号信息 ==========\n");
    printTableBorder(CALLED_RECORD_WIDTHS, 4);
    printTableRow(CALLED_RECORD_HEADERS, CALLED_RECORD_WIDTHS, 4);
    printTableBorder(CALLED_RECORD_WIDTHS, 4);
    while (current != NULL) {
        if (current->status == RECORD_STATUS_CALLED) {
            char doctorId[16];
            char recordId[16];
            const char* cells[4];

            snprintf(doctorId, sizeof(doctorId), "%d", current->relatedDoctorId);
            snprintf(recordId, sizeof(recordId), "%d", current->medicalRecordId);
            cells[0] = doctorId;
            cells[1] = current->patientId;
            cells[2] = recordId;
            cells[3] = current->time;
            printTableRow(cells, CALLED_RECORD_WIDTHS, 4);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(当前没有已叫号未就诊记录)\n");
    printTableBorder(CALLED_RECORD_WIDTHS, 4);
}

// 统计某医生当前候诊人数。
int countWaitingByDoctor(int doctorId) {
    QueueNode* current = gQueueHead;
    int count = 0;

    while (current != NULL) {
        if (current->data.doctorId == doctorId) count++;
        current = current->next;
    }
    return count;
}

// 统计某天某类挂号数量。
int countRegistrationsByDateAndType(const char* date, int regType) {
    MedicalRecord* current = medicalRecordListHead;
    int count = 0;

    if (date == NULL) return 0;
    while (current != NULL) {
        if (current->regType == regType &&
            strcmp(current->appointmentDate, date) == 0 &&
            (strcmp(current->typeOfMedicalRecord, "预约挂号") == 0 ||
                strcmp(current->typeOfMedicalRecord, "现场挂号") == 0 ||
                strcmp(current->typeOfMedicalRecord, "已叫号") == 0 ||
                strcmp(current->typeOfMedicalRecord, "已就诊") == 0)) {
            count++;
        }
        current = current->next;
    }
    return count;
}
