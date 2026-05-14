#pragma once
#ifndef BED_H
#define BED_H

#include "patient.h"

typedef struct Bed {
    // status: 0 空闲，1 占用。
    int bedId;
    int relatedWardId;
    int status;
    char relatedPatientId[PATIENT_ID_LEN];
    struct Bed* next;
} Bed;

// relatedPatientId 为 -1 时表示这张床没有绑定患者。
extern Bed* bedListHead;

void initBedList();
void addBed(Bed* bed);
int deleteBedById(int bedId);
Bed* searchBedById(int bedId);
void modifyBed(Bed* bed);
void printBed(const Bed* bed);
void printAllBeds();
int countBeds();
int countBedsByWard(int wardId);
int countOccupiedBedsByWard(int wardId);
int generateNextBedId();
void saveBedsToFile(const char* filename);
void loadBedsFromFile(const char* filename);

#endif
