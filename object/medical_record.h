#pragma once
#ifndef MEDICAL_RECORD_H
#define MEDICAL_RECORD_H

#include "patient.h"

#define MEDICAL_RECORD_TIME 50
#define MEDICAL_RECORD_DATE 11
#define TYPE_OF_MEDICAL_RECORD 30
#define MEDICAL_RECORD_DETAIL_LEN 160

#define REG_TYPE_ONSITE 0
#define REG_TYPE_APPOINTMENT 1

#define SLOT_UNKNOWN 0
#define SLOT_MORNING 1
#define SLOT_AFTERNOON 2
#define SLOT_EVENING 3

#define RECORD_STATUS_WAITING 0
#define RECORD_STATUS_CALLED 1
#define RECORD_STATUS_CONSULTED 2
#define RECORD_STATUS_ADMITTED 3
#define RECORD_STATUS_DISCHARGED 4
#define RECORD_STATUS_PRESCRIPTION 5

typedef struct MedicalRecord {
    // 挂号、叫号、看诊、住院等流程都落在这一张链表里。
    int medicalRecordId;
    char patientId[PATIENT_ID_LEN];
    int relatedDoctorId;
    int regType;
    char appointmentDate[MEDICAL_RECORD_DATE];
    int appointmentSlot;
    int queueNumber;
    char typeOfMedicalRecord[TYPE_OF_MEDICAL_RECORD];
    char time[MEDICAL_RECORD_TIME];
    char detail[MEDICAL_RECORD_DETAIL_LEN];
    int status;
    struct MedicalRecord* next;
} MedicalRecord;

// 病历链表也是候诊队列重建的来源。
extern MedicalRecord* medicalRecordListHead;

void initMedicalRecordList();
void addMedicalRecord(MedicalRecord* medicalRecord);
int deleteMedicalRecordById(int medicalRecordId);
MedicalRecord* searchMedicalRecordById(int medicalRecordId);
void modifyMedicalRecord(MedicalRecord* medicalRecord);
void printMedicalRecord(const MedicalRecord* record);
void printAllMedicalRecord();
void printMedicalRecordsByPatientId(const char* patientId);
void printMedicalRecordsByDoctorId(int doctorId);
void printCalledRecordsByDoctorId(int doctorId);

int registerPatient(const char* patientId, int doctorId);
int registerPatientOnsite(const char* patientId, int doctorId);
int registerPatientAppointment(const char* patientId, int doctorId, const char* appointmentDate, int appointmentSlot);
int callNextPatientByDoctor(int doctorId);
int consultPatient(int registrationId, const char* diagnosis);
int addExamination(const char* patientId, int doctorId, const char* examItem);
int admitPatient(const char* patientId, int wardId);
int dischargePatient(const char* patientId);
int addPrescription(const char* patientId, int doctorId, int medicineId, int quantity);
int countMedicalRecords();
int countMedicalRecordsByDoctor(int doctorId);
void saveMedicalRecordsToFile(const char* filename);
void loadMedicalRecordsFromFile(const char* filename);

#endif
