#pragma once
#ifndef DEPARTMENT_H
#define DEPARTMENT_H

#define DEPARTMENT_NAME_LEN 50

typedef struct Department {
    // 医生数量保存统计结果，保存前会重新计算。
    int departmentId;
    char name[DEPARTMENT_NAME_LEN];
    int numberOfDoctors;
    struct Department* next;
} Department;

// 科室表是医生、病房、药品的公共基础表。
extern Department* departmentListHead;

void initDepartmentList();
void addDepartment(Department* department);
int deleteDepartmentById(int departmentId);
Department* searchDepartmentById(int departmentId);
void modifyDepartment(Department* department);
void printDepartment(const Department* department);
void printAllDepartments();
void printDepartmentCompliance();
int countDepartments();
void recalculateDepartmentDoctorCounts();
void saveDepartmentsToFile(const char* filename);
void loadDepartmentsFromFile(const char* filename);

#endif
