#pragma once
#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "medical_record.h"

// 队列模块只管理“待叫号”的记录 ID，具体病历内容仍回 medical_record.c 查。
void initQueueManager();
void rebuildQueueFromMedicalRecords();
int enqueueMedicalRecord(const MedicalRecord* record);
int dequeueNextRecordIdByDoctor(int doctorId);
void printWaitingQueueByDoctor(int doctorId);
void printCurrentCalledPatients();
int countWaitingByDoctor(int doctorId);
int countRegistrationsByDateAndType(const char* date, int regType);

#endif
