#pragma once
#ifndef DOCTOR_H
#define DOCTOR_H

#define DOCTOR_NAME_LEN 50
#define DOCTOR_DEPARTMENT_LEN 50
#define DOCTOR_PHONE_LEN 20
#define DOCTOR_TITLE_LEN 30
#define DOCTOR_ID_CARD_LEN 20
#define DOCTOR_SPECIALTY_LEN 50

typedef struct Doctor {
    // 科室名保存一份，打印时不用再让用户自己对照科室表。
    int doctorId;
    char idCard[DOCTOR_ID_CARD_LEN];
    char name[DOCTOR_NAME_LEN];
    char title[DOCTOR_TITLE_LEN];
    char specialty[DOCTOR_SPECIALTY_LEN];
    int onDuty;
    int departmentId;
    char departmentName[DOCTOR_DEPARTMENT_LEN];
    char phone[DOCTOR_PHONE_LEN];
    char gender[10];
    int age;
    char type[DOCTOR_SPECIALTY_LEN];
    int bedId;
    struct Doctor* next;
} Doctor;

// 医生 ID 用于业务关联，身份证号主要用于医生账号注册。
extern Doctor* doctorListHead;

void initDoctorList();
void addDoctor(Doctor* doctor);
int deleteDoctorById(int doctorId);
Doctor* searchDoctorById(int doctorId);
Doctor* searchDoctorByIdCard(const char* idCard);
Doctor* searchDoctorByName(char* doctorName);
void modifyDoctor(Doctor* doctor);
void printDoctor(const Doctor* doctor);
void printAllDoctors();
void printDoctorsByDepartment(int departmentId);
int countDoctors();
int countDoctorsByDepartment(int departmentId);
void saveDoctorsToFile(const char* filename);
void loadDoctorsFromFile(const char* filename);

#endif
