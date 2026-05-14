#define _CRT_SECURE_NO_WARNINGS
#include "file_manager.h"
#include "patient.h"
#include "ward.h"
#include "bed.h"
#include "doctor.h"
#include "medicine.h"
#include "medical_record.h"
#include "department.h"
#include "queue_manager.h"
#include "pharmacy_log.h"
#include "auth.h"
#include "change_log.h"
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <string.h>

#define DATA_PATH_LEN 260

/*
 * 文件管理模块统一处理 data 目录和所有文本表的读写。
 * 业务模块只关心“加载/保存”，不用到处拼路径。
 */

static char gDataDir[DATA_PATH_LEN] = "data";

// 判断路径是否存在。
static int pathExists(const char* path) {
    return _access(path, 0) == 0;
}

// 拼出 data 目录下某个文件的路径。
static void joinPath(char* out, int outSize, const char* dir, const char* fileName) {
    snprintf(out, outSize, "%s/%s", dir, fileName);
}

// 自动识别当前程序能访问到的数据目录。
static void resolveDataDirectory() {
    // 从 exe 目录或项目目录运行都能找到同一份 data。
    if (pathExists("data")) {
        strcpy(gDataDir, "data");
        return;
    }
    if (pathExists("../data")) {
        strcpy(gDataDir, "../data");
        return;
    }
    _mkdir("data");
    strcpy(gDataDir, "data");
}

// 如果某个数据文件不存在，就先创建空文件。
static void ensureFileExists(const char* fileName) {
    char path[DATA_PATH_LEN];
    FILE* file;

    joinPath(path, DATA_PATH_LEN, gDataDir, fileName);
    if (pathExists(path)) return;

    file = fopen(path, "w");
    if (file != NULL) fclose(file);
}

// 初始化 data 目录和所有数据文件。
void initDataFiles() {
    resolveDataDirectory();
    ensureFileExists("departments.txt");
    ensureFileExists("doctors.txt");
    ensureFileExists("patients.txt");
    ensureFileExists("wards.txt");
    ensureFileExists("beds.txt");
    ensureFileExists("medicines.txt");
    ensureFileExists("medical_records.txt");
    ensureFileExists("pharmacy_logs.txt");
    ensureFileExists("change_logs.txt");
    ensureFileExists("users.txt");
}

// 按依赖顺序加载全部业务数据。
void loadAllData() {
    char path[DATA_PATH_LEN];

    resolveDataDirectory();
    setChangeLogEnabled(0);
    initQueueManager();

    // 先读基础表，再读病历，候诊队列最后由病历重建。
    joinPath(path, DATA_PATH_LEN, gDataDir, "departments.txt");
    loadDepartmentsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "doctors.txt");
    loadDoctorsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "patients.txt");
    loadPatientsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "wards.txt");
    loadWardsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "beds.txt");
    loadBedsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "medicines.txt");
    loadMedicinesFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "medical_records.txt");
    loadMedicalRecordsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "pharmacy_logs.txt");
    loadPharmacyLogsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "change_logs.txt");
    loadChangeLogsFromFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "users.txt");
    loadUsersFromFile(path);

    recalculateDepartmentDoctorCounts();
    recalculateWardAvailability();
    setChangeLogEnabled(1);
}

// 保存所有业务数据，退出前统一调用。
void saveAllData() {
    char path[DATA_PATH_LEN];

    resolveDataDirectory();
    recalculateDepartmentDoctorCounts();
    recalculateWardAvailability();

    // 保存前把科室医生数和病房空床数再算一遍。
    joinPath(path, DATA_PATH_LEN, gDataDir, "departments.txt");
    saveDepartmentsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "doctors.txt");
    saveDoctorsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "patients.txt");
    savePatientsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "wards.txt");
    saveWardsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "beds.txt");
    saveBedsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "medicines.txt");
    saveMedicinesToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "medical_records.txt");
    saveMedicalRecordsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "pharmacy_logs.txt");
    savePharmacyLogsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "change_logs.txt");
    saveChangeLogsToFile(path);

    joinPath(path, DATA_PATH_LEN, gDataDir, "users.txt");
    saveUsersToFile(path);
}
