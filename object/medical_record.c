#define _CRT_SECURE_NO_WARNINGS
#include "medical_record.h"
#include "util.h"
#include "ward.h"
#include "bed.h"
#include "patient.h"
#include "doctor.h"
#include "medicine.h"
#include "queue_manager.h"
#include "pharmacy_log.h"
#include "change_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * 病历记录是系统的流程主线：挂号、叫号、看诊、检查、住院、出院、发药都会落一条记录。
 * 其他模块多是“基础资料”，这里负责把一次医疗行为串起来。
 */

MedicalRecord* medicalRecordListHead = NULL;

static const int MEDICAL_RECORD_TABLE_WIDTHS[] = { 12, 64 };

static const char* regTypeToText(int regType);
static const char* slotToText(int slot);
static const char* statusToText(int status);

static void buildPatientTargetName(const Patient* patient, const char* patientId, char* buffer, int bufferSize) {
    if (buffer == NULL || bufferSize <= 0) return;
    if (patient != NULL && patient->name[0] != '\0') {
        snprintf(buffer, bufferSize, "患者 %s", patient->name);
    } else if (patientId != NULL && patientId[0] != '\0') {
        snprintf(buffer, bufferSize, "患者 %s", patientId);
    } else {
        copyText(buffer, bufferSize, "患者");
    }
}

// 初始化病历链表。
void initMedicalRecordList() {
    medicalRecordListHead = NULL;
}

// 将一条病历追加到链表尾部。
void addMedicalRecord(MedicalRecord* p) {
    MedicalRecord* current;

    if (p == NULL) return;
    p->next = NULL;
    if (medicalRecordListHead == NULL) {
        medicalRecordListHead = p;
        return;
    }
    current = medicalRecordListHead;
    while (current->next != NULL) current = current->next;
    current->next = p;
}

// 按病历 ID 删除记录，并同步重建候诊队列。
int deleteMedicalRecordById(int medicalRecordId) {
    MedicalRecord* current = medicalRecordListHead;
    MedicalRecord* previous = NULL;

    while (current != NULL) {
        if (current->medicalRecordId == medicalRecordId) {
            if (previous == NULL) medicalRecordListHead = current->next;
            else previous->next = current->next;
            free(current);
            rebuildQueueFromMedicalRecords();
            return 1;
        }
        previous = current;
        current = current->next;
    }
    return medicalRecordListHead == NULL ? -1 : 0;
}

// 按病历 ID 查找记录。
MedicalRecord* searchMedicalRecordById(int medicalRecordId) {
    MedicalRecord* current = medicalRecordListHead;

    while (current != NULL) {
        if (current->medicalRecordId == medicalRecordId) return current;
        current = current->next;
    }
    return NULL;
}

// 修改病历内容，并让候诊队列跟着最新状态刷新。
void modifyMedicalRecord(MedicalRecord* medicalRecord) {
    MedicalRecord* target;

    if (medicalRecord == NULL) return;
    target = searchMedicalRecordById(medicalRecord->medicalRecordId);
    if (target == NULL) return;

    copyText(target->patientId, PATIENT_ID_LEN, medicalRecord->patientId);
    target->relatedDoctorId = medicalRecord->relatedDoctorId;
    target->regType = medicalRecord->regType;
    copyText(target->appointmentDate, MEDICAL_RECORD_DATE, medicalRecord->appointmentDate);
    target->appointmentSlot = medicalRecord->appointmentSlot;
    target->queueNumber = medicalRecord->queueNumber;
    copyText(target->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, medicalRecord->typeOfMedicalRecord);
    copyText(target->time, MEDICAL_RECORD_TIME, medicalRecord->time);
    copyText(target->detail, MEDICAL_RECORD_DETAIL_LEN, medicalRecord->detail);
    target->status = medicalRecord->status;
    rebuildQueueFromMedicalRecords();
}

// 把挂号类型编号转成显示文字。
static const char* regTypeToText(int regType) {
    return regType == REG_TYPE_APPOINTMENT ? "预约" : "现场";
}

// 把时段编号转成显示文字。
static const char* slotToText(int slot) {
    if (slot == SLOT_MORNING) return "上午";
    if (slot == SLOT_AFTERNOON) return "下午";
    if (slot == SLOT_EVENING) return "晚上";
    return "未指定";
}

// 把病历状态编号转成显示文字。
static const char* statusToText(int status) {
    if (status == RECORD_STATUS_WAITING) return "待叫号";
    if (status == RECORD_STATUS_CALLED) return "已叫号";
    if (status == RECORD_STATUS_CONSULTED) return "已就诊";
    if (status == RECORD_STATUS_ADMITTED) return "已住院";
    if (status == RECORD_STATUS_DISCHARGED) return "已出院";
    if (status == RECORD_STATUS_PRESCRIPTION) return "已发药";
    return "未知";
}

// 打印一条完整病历。
void printMedicalRecord(const MedicalRecord* current) {
    char recordId[16];
    char doctorId[16];
    char queueNumber[16];
    const char* row[2];

    if (current == NULL) return;
    snprintf(recordId, sizeof(recordId), "%d", current->medicalRecordId);
    snprintf(doctorId, sizeof(doctorId), "%d", current->relatedDoctorId);
    snprintf(queueNumber, sizeof(queueNumber), "%d", current->queueNumber);

    printTableBorder(MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "记录ID"; row[1] = recordId; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "患者ID"; row[1] = current->patientId; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "医生ID"; row[1] = doctorId; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "挂号类型"; row[1] = regTypeToText(current->regType); printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "预约日期"; row[1] = current->appointmentDate; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "预约时段"; row[1] = slotToText(current->appointmentSlot); printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "排队号"; row[1] = queueNumber; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "类型"; row[1] = current->typeOfMedicalRecord; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "状态"; row[1] = statusToText(current->status); printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "时间"; row[1] = current->time; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    row[0] = "详情"; row[1] = current->detail; printTableRow(row, MEDICAL_RECORD_TABLE_WIDTHS, 2);
    printTableBorder(MEDICAL_RECORD_TABLE_WIDTHS, 2);
}

// 打印全部病历。
void printAllMedicalRecord() {
    MedicalRecord* current = medicalRecordListHead;

    if (current == NULL) {
        printf("当前无病历数据。\n");
        return;
    }
    printf("\n================ 病历列表 ================\n");
    while (current != NULL) {
        printMedicalRecord(current);
        current = current->next;
    }
}

// 按患者 ID 打印该患者的诊疗流程。
void printMedicalRecordsByPatientId(const char* patientId) {
    MedicalRecord* current = medicalRecordListHead;
    int found = 0;

    if (patientId == NULL || patientId[0] == '\0') return;
    printf("\n========== 患者 %s 诊疗流程 ==========\n", patientId);
    while (current != NULL) {
        if (strcmp(current->patientId, patientId) == 0) {
            printMedicalRecord(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(该患者暂无医疗记录)\n");
}

// 按医生 ID 打印该医生相关病历。
void printMedicalRecordsByDoctorId(int doctorId) {
    MedicalRecord* current = medicalRecordListHead;
    int found = 0;

    printf("\n========== 医生 %d 相关病历 ==========\n", doctorId);
    while (current != NULL) {
        if (current->relatedDoctorId == doctorId) {
            printMedicalRecord(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(该医生暂无医疗记录)\n");
}

// 打印某医生已叫号但还没看诊的记录。
void printCalledRecordsByDoctorId(int doctorId) {
    MedicalRecord* current = medicalRecordListHead;
    int found = 0;

    printf("\n========== 医生 %d 已叫号未就诊 ==========\n", doctorId);
    while (current != NULL) {
        if (current->relatedDoctorId == doctorId && current->status == RECORD_STATUS_CALLED) {
            printMedicalRecord(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(当前没有已叫号未就诊记录)\n");
}

// 根据当前最大病历 ID 生成下一条 ID。
static int generateMedicalRecordId() {
    int maxId = 70000000;
    MedicalRecord* current = medicalRecordListHead;

    // 用当前最大值加一生成 ID，文本文件重启后也能接着编号。
    while (current != NULL) {
        if (current->medicalRecordId > maxId) maxId = current->medicalRecordId;
        current = current->next;
    }
    return maxId + 1;
}

// 生成新的排队号。
static int generateQueueNumber() {
    int maxQueueNumber = 0;
    MedicalRecord* current = medicalRecordListHead;

    while (current != NULL) {
        if (current->queueNumber > maxQueueNumber) maxQueueNumber = current->queueNumber;
        current = current->next;
    }
    return maxQueueNumber + 1;
}

// 按当前时间判断现场挂号所在时段。
static int getCurrentSlot() {
    time_t nowtime;
    struct tm local;

    time(&nowtime);
    localtime_s(&local, &nowtime);
    if (local.tm_hour < 12) return SLOT_MORNING;
    if (local.tm_hour < 18) return SLOT_AFTERNOON;
    return SLOT_EVENING;
}

// 取得当天日期，供现场挂号和流程记录使用。
static void getCurrentDate(char* buffer, int bufferSize) {
    time_t nowtime;
    struct tm local;

    time(&nowtime);
    localtime_s(&local, &nowtime);
    snprintf(buffer, bufferSize, "%04d-%02d-%02d",
        local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

// 检查同一患者是否已有同医生同日同时段的未完成挂号。
static int hasActiveRegistration(const char* patientId, int doctorId, const char* appointmentDate, int appointmentSlot) {
    MedicalRecord* current = medicalRecordListHead;

    // 同一患者、同一医生、同一天同一时段，不重复挂号。
    while (current != NULL) {
        if (strcmp(current->patientId, patientId) == 0 &&
            current->relatedDoctorId == doctorId &&
            strcmp(current->appointmentDate, appointmentDate) == 0 &&
            current->appointmentSlot == appointmentSlot &&
            (current->status == RECORD_STATUS_WAITING || current->status == RECORD_STATUS_CALLED)) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// 判断年份是否为闰年。
static int isLeapYear(int year) {
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    return year % 4 == 0;
}

// 校验预约日期格式和真实日期。
static int isValidDateText(const char* date) {
    int year;
    int month;
    int day;
    int maxDay;
    char tail;
    static const int monthDays[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    // 这里不只检查格式，还会挡掉 2026-02-31 这类不存在的日期。
    if (date == NULL || strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    if (sscanf(date, "%4d-%2d-%2d%c", &year, &month, &day, &tail) != 3) return 0;
    if (year < 1900 || month < 1 || month > 12 || day < 1) return 0;
    maxDay = monthDays[month - 1];
    if (month == 2 && isLeapYear(year)) maxDay = 29;
    return day <= maxDay;
}

// 挂号的公共实现，现场和预约都从这里生成病历。
static int registerPatientInternal(const char* patientId, int doctorId, int regType, const char* appointmentDate, int appointmentSlot) {
    Patient* patient = searchPatientById(patientId);
    Doctor* doctor = searchDoctorById(doctorId);
    MedicalRecord* record;
    int enqueueResult;

    // 正数返回病历 ID，负数留给菜单层翻译成更友好的提示。
    if (patient == NULL) return -1;
    if (doctor == NULL) return -2;
    if (appointmentSlot < SLOT_MORNING || appointmentSlot > SLOT_EVENING) return -3;
    if (appointmentDate == NULL || appointmentDate[0] == '\0') return -7;
    if (!isValidDateText(appointmentDate)) return -8;
    if (hasActiveRegistration(patientId, doctorId, appointmentDate, appointmentSlot)) return -6;

    record = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (record == NULL) return -4;

    record->medicalRecordId = generateMedicalRecordId();
    copyText(record->patientId, PATIENT_ID_LEN, patientId);
    record->relatedDoctorId = doctorId;
    record->regType = regType;
    copyText(record->appointmentDate, MEDICAL_RECORD_DATE, appointmentDate);
    record->appointmentSlot = appointmentSlot;
    record->queueNumber = generateQueueNumber();
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, regType == REG_TYPE_APPOINTMENT ? "预约挂号" : "现场挂号");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    copyText(record->detail, MEDICAL_RECORD_DETAIL_LEN, "待叫号");
    record->status = RECORD_STATUS_WAITING;
    record->next = NULL;

    // 病历先落链表，再把“待叫号”的记录放进候诊队列。
    addMedicalRecord(record);
    enqueueResult = enqueueMedicalRecord(record);
    if (!enqueueResult) return -5;
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildPatientTargetName(patient, patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "挂号类型:%s 医生ID:%d 日期:%s 时段:%s 状态:%s",
            regTypeToText(regType), doctorId, appointmentDate, slotToText(appointmentSlot), statusToText(RECORD_STATUS_WAITING));
        addChangeLogEntry("挂号管理", target, content);
    }
    return record->medicalRecordId;
}

// 兼容旧接口，默认按现场挂号处理。
int registerPatient(const char* patientId, int doctorId) {
    return registerPatientOnsite(patientId, doctorId);
}

// 给患者办理现场挂号。
int registerPatientOnsite(const char* patientId, int doctorId) {
    char currentDate[MEDICAL_RECORD_DATE];

    getCurrentDate(currentDate, MEDICAL_RECORD_DATE);
    return registerPatientInternal(patientId, doctorId, REG_TYPE_ONSITE, currentDate, getCurrentSlot());
}

// 给患者办理预约挂号。
int registerPatientAppointment(const char* patientId, int doctorId, const char* appointmentDate, int appointmentSlot) {
    return registerPatientInternal(patientId, doctorId, REG_TYPE_APPOINTMENT, appointmentDate, appointmentSlot);
}

// 医生从候诊队列中叫下一位患者。
int callNextPatientByDoctor(int doctorId) {
    int recordId;
    MedicalRecord* record;
    int oldStatus;

    if (searchDoctorById(doctorId) == NULL) return -1;
    recordId = dequeueNextRecordIdByDoctor(doctorId);
    if (recordId < 0) return -2;

    record = searchMedicalRecordById(recordId);
    if (record == NULL) return -3;

    // 叫号后从队列移除，但病历仍在，状态改成“已叫号”。
    oldStatus = record->status;
    record->status = RECORD_STATUS_CALLED;
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, "已叫号");
    copyText(record->detail, MEDICAL_RECORD_DETAIL_LEN, "已叫号");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        Patient* patient = searchPatientById(record->patientId);
        buildPatientTargetName(patient, record->patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "挂号状态由 %s 改为 %s，记录ID:%d",
            statusToText(oldStatus), statusToText(record->status), record->medicalRecordId);
        addChangeLogEntry("挂号管理", target, content);
    }
    return recordId;
}

// 完成看诊并写入诊断结果。
int consultPatient(int registrationId, const char* diagnosis) {
    MedicalRecord* record = searchMedicalRecordById(registrationId);
    Patient* patient;
    char oldDiagnosis[PATIENT_DIAGNOSIS_LEN];
    int oldStatus;

    if (record == NULL) return -1;
    if (diagnosis == NULL || diagnosis[0] == '\0') return -2;
    if (record->status != RECORD_STATUS_CALLED) return -3;

    patient = searchPatientById(record->patientId);
    if (patient == NULL) return -4;

    copyText(oldDiagnosis, sizeof(oldDiagnosis), patient->diagnosis);
    if (oldDiagnosis[0] == '\0') copyText(oldDiagnosis, sizeof(oldDiagnosis), "无");
    oldStatus = record->status;

    // 看诊完成后同时更新病历详情和患者当前诊断。
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, "已就诊");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    copyText(record->detail, MEDICAL_RECORD_DETAIL_LEN, diagnosis);
    record->status = RECORD_STATUS_CONSULTED;
    copyText(patient->diagnosis, PATIENT_DIAGNOSIS_LEN, diagnosis);
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildPatientTargetName(patient, record->patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "挂号状态由 %s 改为 %s；诊断由 %s 改为 %s",
            statusToText(oldStatus), statusToText(record->status), oldDiagnosis, diagnosis);
        addChangeLogEntry("挂号管理", target, content);
    }
    return 1;
}

// 为患者增加一条检查记录。
int addExamination(const char* patientId, int doctorId, const char* examItem) {
    Patient* patient = searchPatientById(patientId);
    Doctor* doctor = searchDoctorById(doctorId);
    MedicalRecord* record;

    if (patient == NULL) return -1;
    if (doctor == NULL) return -2;
    if (examItem == NULL || examItem[0] == '\0') return -3;

    record = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (record == NULL) return -4;

    record->medicalRecordId = generateMedicalRecordId();
    copyText(record->patientId, PATIENT_ID_LEN, patientId);
    record->relatedDoctorId = doctorId;
    record->regType = REG_TYPE_ONSITE;
    getCurrentDate(record->appointmentDate, MEDICAL_RECORD_DATE);
    record->appointmentSlot = SLOT_UNKNOWN;
    record->queueNumber = 0;
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, "检查");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    copyText(record->detail, MEDICAL_RECORD_DETAIL_LEN, examItem);
    record->status = RECORD_STATUS_CONSULTED;
    record->next = NULL;
    addMedicalRecord(record);
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildPatientTargetName(patient, patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "新增检查:%s 医生ID:%d", examItem, doctorId);
        addChangeLogEntry("挂号管理", target, content);
    }
    return record->medicalRecordId;
}

// 给患者办理住院并分配空床。
int admitPatient(const char* patientId, int wardId) {
    Patient* patient = searchPatientById(patientId);
    Ward* ward = searchWardById(wardId);
    Bed* bed = bedListHead;
    MedicalRecord* record;
    int assignedBedId;

    if (patient == NULL) return -1;
    if (ward == NULL) return -2;
    if (strcmp(patient->typeOfPatient, "住院") == 0) return -4;
    recalculateWardAvailability();
    if (ward->availableBedOfWard <= 0) return -3;

    while (bed != NULL) {
        if (bed->relatedWardId == wardId && bed->status == 0) break;
        bed = bed->next;
    }
    if (bed == NULL) return -3;
    assignedBedId = bed->bedId;

    record = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (record == NULL) return -5;

    // 找到空床后再改患者和床位，避免中途失败留下半截数据。
    bed->status = 1;
    copyText(bed->relatedPatientId, PATIENT_ID_LEN, patientId);
    copyText(patient->typeOfPatient, PATIENT_TYPE_LEN, "住院");
    patient->relatedWardId = wardId;
    patient->relatedBedId = bed->bedId;
    recalculateWardAvailability();

    record->medicalRecordId = generateMedicalRecordId();
    copyText(record->patientId, PATIENT_ID_LEN, patientId);
    record->relatedDoctorId = -1;
    record->regType = REG_TYPE_ONSITE;
    getCurrentDate(record->appointmentDate, MEDICAL_RECORD_DATE);
    record->appointmentSlot = SLOT_UNKNOWN;
    record->queueNumber = 0;
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, "住院");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    snprintf(record->detail, MEDICAL_RECORD_DETAIL_LEN, "病房%d 床位%d", wardId, bed->bedId);
    record->status = RECORD_STATUS_ADMITTED;
    record->next = NULL;
    addMedicalRecord(record);
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildPatientTargetName(patient, patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "办理住院: 病房%d 床位%d", wardId, assignedBedId);
        addChangeLogEntry("挂号管理", target, content);
    }
    return record->medicalRecordId;
}

// 给住院患者办理出院并释放床位。
int dischargePatient(const char* patientId) {
    Patient* patient = searchPatientById(patientId);
    Ward* ward;
    Bed* bed;
    MedicalRecord* record;
    int wardId;
    int bedId;

    if (patient == NULL) return -1;
    if (strcmp(patient->typeOfPatient, "住院") != 0) return -2;

    ward = searchWardById(patient->relatedWardId);
    bed = searchBedById(patient->relatedBedId);
    if (ward == NULL) return -3;
    if (bed == NULL) return -4;
    wardId = patient->relatedWardId;
    bedId = patient->relatedBedId;

    record = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (record == NULL) return -5;

    // 出院后床位重新变为空闲。
    bed->status = 0;
    copyText(bed->relatedPatientId, PATIENT_ID_LEN, PATIENT_ID_NONE);
    copyText(patient->typeOfPatient, PATIENT_TYPE_LEN, "门诊");
    patient->relatedWardId = -1;
    patient->relatedBedId = -1;
    recalculateWardAvailability();

    record->medicalRecordId = generateMedicalRecordId();
    copyText(record->patientId, PATIENT_ID_LEN, patientId);
    record->relatedDoctorId = -1;
    record->regType = REG_TYPE_ONSITE;
    getCurrentDate(record->appointmentDate, MEDICAL_RECORD_DATE);
    record->appointmentSlot = SLOT_UNKNOWN;
    record->queueNumber = 0;
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, "出院");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    copyText(record->detail, MEDICAL_RECORD_DETAIL_LEN, "已出院");
    record->status = RECORD_STATUS_DISCHARGED;
    record->next = NULL;
    addMedicalRecord(record);
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildPatientTargetName(patient, patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "办理出院: 病房%d 床位%d", wardId, bedId);
        addChangeLogEntry("挂号管理", target, content);
    }
    return record->medicalRecordId;
}

// 开处方发药，并同时生成病历和药品流水。
int addPrescription(const char* patientId, int doctorId, int medicineId, int quantity) {
    Patient* patient = searchPatientById(patientId);
    Doctor* doctor = searchDoctorById(doctorId);
    Medicine* medicine = searchMedicineById(medicineId);
    MedicalRecord* record;
    int stockResult;

    if (patient == NULL) return -1;
    if (doctor == NULL) return -2;
    if (medicine == NULL) return -3;
    if (quantity <= 0) return -4;

    // 先扣库存，库存不足就不生成病历和流水。
    stockResult = reduceMedicineStock(medicineId, quantity);
    if (stockResult == -3) return -5;
    if (stockResult != 1) return -6;

    record = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (record == NULL) return -7;

    record->medicalRecordId = generateMedicalRecordId();
    copyText(record->patientId, PATIENT_ID_LEN, patientId);
    record->relatedDoctorId = doctorId;
    record->regType = REG_TYPE_ONSITE;
    getCurrentDate(record->appointmentDate, MEDICAL_RECORD_DATE);
    record->appointmentSlot = SLOT_UNKNOWN;
    record->queueNumber = 0;
    copyText(record->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, "处方发药");
    getCurrentTime(record->time, MEDICAL_RECORD_TIME);
    snprintf(record->detail, MEDICAL_RECORD_DETAIL_LEN, "药品ID:%d 药品:%s 数量:%d", medicineId, medicine->genericName, quantity);
    record->status = RECORD_STATUS_PRESCRIPTION;
    record->next = NULL;
    addMedicalRecord(record);
    addPharmacyLogEntry(medicineId, "发药", quantity, patientId, doctorId, record->detail);
    {
        char target[CHANGE_LOG_TARGET_LEN];
        char content[CHANGE_LOG_CONTENT_LEN];
        buildPatientTargetName(patient, patientId, target, sizeof(target));
        snprintf(content, sizeof(content), "处方发药: 药品%s 数量%d 医生ID:%d", medicine->genericName, quantity, doctorId);
        addChangeLogEntry("挂号管理", target, content);
    }
    return record->medicalRecordId;
}

// 统计病历总数。
int countMedicalRecords() {
    MedicalRecord* current = medicalRecordListHead;
    int count = 0;

    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

// 统计某医生关联的病历数量。
int countMedicalRecordsByDoctor(int doctorId) {
    MedicalRecord* current = medicalRecordListHead;
    int count = 0;

    while (current != NULL) {
        if (current->relatedDoctorId == doctorId) count++;
        current = current->next;
    }
    return count;
}

// 保存病历链表到文本文件。
void saveMedicalRecordsToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    MedicalRecord* current = medicalRecordListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%d|%s|%d|%d|%s|%d|%d|%s|%s|%s|%d\n",
            current->medicalRecordId, current->patientId, current->relatedDoctorId,
            current->regType, current->appointmentDate, current->appointmentSlot,
            current->queueNumber, current->typeOfMedicalRecord, current->time,
            current->detail, current->status);
        current = current->next;
    }
    fclose(file);
}

// 解析一行病历数据。
static int parseMedicalRecordLine(char* line, MedicalRecord* mr) {
    char* fields[11];

    // 病历详情可能含有中文说明，所以优先使用 | 分隔的固定字段格式。
    if (strchr(line, '|') != NULL) {
        if (splitFields(line, fields, 11) != 11) return 0;
        if (!parseIntField(fields[0], &mr->medicalRecordId)) return 0;
        copyText(mr->patientId, PATIENT_ID_LEN, fields[1]);
        if (!parseIntField(fields[2], &mr->relatedDoctorId)) return 0;
        if (!parseIntField(fields[3], &mr->regType)) return 0;
        copyText(mr->appointmentDate, MEDICAL_RECORD_DATE, fields[4]);
        if (!parseIntField(fields[5], &mr->appointmentSlot)) return 0;
        if (!parseIntField(fields[6], &mr->queueNumber)) return 0;
        copyText(mr->typeOfMedicalRecord, TYPE_OF_MEDICAL_RECORD, fields[7]);
        copyText(mr->time, MEDICAL_RECORD_TIME, fields[8]);
        copyText(mr->detail, MEDICAL_RECORD_DETAIL_LEN, fields[9]);
        if (!parseIntField(fields[10], &mr->status)) return 0;
        return 1;
    }

    return sscanf(line, "%d %19s %d %d %10s %d %d %29s %49s %159s %d",
        &mr->medicalRecordId, mr->patientId, &mr->relatedDoctorId,
        &mr->regType, mr->appointmentDate, &mr->appointmentSlot,
        &mr->queueNumber, mr->typeOfMedicalRecord, mr->time,
        mr->detail, &mr->status) == 11;
}

// 从文件加载病历，并在末尾恢复候诊队列。
void loadMedicalRecordsFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[512];
    int lineNo = 0;

    initMedicalRecordList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        MedicalRecord* mr;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        mr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
        if (mr == NULL) break;
        memset(mr, 0, sizeof(MedicalRecord));
        if (!parseMedicalRecordLine(line, mr)) {
            printf("跳过病历数据第 %d 行：格式错误。\n", lineNo);
            free(mr);
            continue;
        }
        mr->next = NULL;
        addMedicalRecord(mr);
    }
    fclose(file);
    // 文件里只保存病历，候诊队列根据 WAITING 状态重新排出来。
    rebuildQueueFromMedicalRecords();
}
