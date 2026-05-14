#pragma once
#ifndef MEDICINE_H
#define MEDICINE_H

#define MEDICINE_NAME_LEN 50
#define MEDICINE_CATEGORY_LEN 20
#define MEDICINE_UNIT_LEN 10

typedef struct Medicine {
    // 同一种药可能有几种叫法，查询时三个名称一起匹配。
    int medicineId;
    char genericName[MEDICINE_NAME_LEN];
    char brandName[MEDICINE_NAME_LEN];
    char aliasName[MEDICINE_NAME_LEN];
    char category[MEDICINE_CATEGORY_LEN];
    char unit[MEDICINE_UNIT_LEN];
    int inventory;
    int price;
    int relatedDepartmentId;
    struct Medicine* next;
} Medicine;

// 库存只保存在药品表中，流水表负责追踪变化来源。
extern Medicine* medicineListHead;

void initMedicineList();
void addMedicine(Medicine* medicine);
int deleteMedicineById(int medicineId);
Medicine* searchMedicineById(int medicineId);
Medicine* searchMedicineByName(char* medicineName);
void modifyMedicine(Medicine* medicine);
void printMedicine(const Medicine* medicine);
void printMedicineTableHeader();
void printMedicineTableBorder();
void printAllMedicines();
int printMedicinesByName(const char* medicineName);
int countMedicines();
int addMedicineStock(int medicineId, int quantity);
int reduceMedicineStock(int medicineId, int quantity);
void saveMedicinesToFile(const char* filename);
void loadMedicinesFromFile(const char* filename);

#endif
