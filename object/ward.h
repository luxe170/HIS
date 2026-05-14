#pragma once
#ifndef WARD_H
#define WARD_H

#define WARD_TYPE_LEN 20

typedef struct Ward {
    // 可用床位数由床位链表重新统计，避免手工修改出错。
    int wardId;
    char typeOfWard[WARD_TYPE_LEN];
    int relatedDepartmentId;
    int totalBedOfWard;
    int availableBedOfWard;
    struct Ward* next;
} Ward;

// 病房和床位分开建链表，住院时先找病房，再挑空床。
extern Ward* wardListHead;

void initWardList();
void addWard(Ward* ward);
int deleteWardById(int wardId);
Ward* searchWardById(int wardId);
void modifyWard(Ward* ward);
void printWard(const Ward* ward);
void printAllWards();
int countWards();
int countWardTypes();
void recalculateWardAvailability();
void saveWardsToFile(const char* filename);
void loadWardsFromFile(const char* filename);

#endif
