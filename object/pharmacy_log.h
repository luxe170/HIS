#pragma once
#ifndef PHARMACY_LOG_H
#define PHARMACY_LOG_H

#include "patient.h"

#define PHARMACY_LOG_TYPE_LEN 20
#define PHARMACY_LOG_TIME_LEN 50
#define PHARMACY_LOG_REMARK_LEN 120

typedef struct PharmacyLog {
    // 入库和发药都记一条流水，后面查库存变化更方便。
    int logId;
    int medicineId;
    char type[PHARMACY_LOG_TYPE_LEN];
    int quantity;
    char patientId[PATIENT_ID_LEN];
    int doctorId;
    char time[PHARMACY_LOG_TIME_LEN];
    char remark[PHARMACY_LOG_REMARK_LEN];
    struct PharmacyLog* next;
} PharmacyLog;

// 流水只追加记录，不反向修改病历或药品基础信息。
extern PharmacyLog* pharmacyLogListHead;

void initPharmacyLogList();
void addPharmacyLog(PharmacyLog* log);
int addPharmacyLogEntry(int medicineId, const char* type, int quantity, const char* patientId, int doctorId, const char* remark);
void printAllPharmacyLogs();
void printPharmacyLogsByMedicineId(int medicineId);
void printPharmacyLogsByPatientId(const char* patientId);
void printPharmacyLogsByDoctorId(int doctorId);
void savePharmacyLogsToFile(const char* filename);
void loadPharmacyLogsFromFile(const char* filename);

#endif
