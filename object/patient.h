#pragma once
#ifndef PATIENT_H
#define PATIENT_H

#define PATIENT_NAME_LEN 50
#define PATIENT_GENDER_LEN 10
#define PATIENT_DIAGNOSIS_LEN 100
#define PATIENT_PHONE_LEN 20
#define PATIENT_TYPE_LEN 20
#define PATIENT_ID_LEN 20
#define PATIENT_ID_NONE "-1"

typedef struct Patient {
    // 住院患者会同时挂上病房号和床位号，门诊患者保持 -1。
    char patientId[PATIENT_ID_LEN];
    char name[PATIENT_NAME_LEN];
    char gender[PATIENT_GENDER_LEN];
    int age;
    char diagnosis[PATIENT_DIAGNOSIS_LEN];
    char typeOfPatient[PATIENT_TYPE_LEN];
    int relatedWardId;
    int relatedBedId;
    char phone[PATIENT_PHONE_LEN];
    struct Patient* next;
} Patient;

// 全局链表头在 patient.c 中定义，其他模块只通过函数访问它。
extern Patient* patientListHead;

void initPatientList();
void addPatient(Patient* patient);
int deletePatientById(const char* patientId);
Patient* searchPatientById(const char* patientId);
Patient* searchPatientByName(char* patientName);
void modifyPatient(Patient* patient);
int isValidPatientIdCard(const char* patientId);
void printPatient(const Patient* patient);
void printPatientTableHeader();
void printPatientTableBorder();
void printAllPatients();
int printPatientsByName(const char* patientName);
int countPatients();
int countPatientsByType(const char* typeOfPatient);
void savePatientsToFile(const char* filename);
void loadPatientsFromFile(const char* filename);

#endif
