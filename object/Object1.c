#define _CRT_SECURE_NO_WARNINGS
#include "patient.h"
#include "ward.h"
#include "bed.h"
#include "doctor.h"
#include "medicine.h"
#include "medical_record.h"
#include "department.h"
#include "file_manager.h"
#include "queue_manager.h"
#include "pharmacy_log.h"
#include "auth.h"
#include "change_log.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * 主程序只做两件事：组织菜单入口、把输入交给各业务模块。
 * 答辩时可以从 main -> 角色入口 -> 对应工作台这条线讲，结构会比较清楚。
 */

// 反复读取菜单编号，直到拿到合法整数或遇到输入结束。
static int readChoice(const char* prompt) {
    int choice;
    while (1) {
        if (readIntPrompt(prompt, &choice)) return choice;
        if (feof(stdin)) return 0;
    }
}

// 比较两个患者 ID，顺手处理空指针。
static int isSamePatientId(const char* left, const char* right) {
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

// 判断医生是否接触过某位患者，用于医生端权限控制。
static int doctorHasPatientRecord(int doctorId, const char* patientId) {
    MedicalRecord* current = medicalRecordListHead;

    // 医生端开检查、处方前先确认“这个患者确实和我有过接诊记录”。
    if (patientId == NULL || patientId[0] == '\0') return 0;
    while (current != NULL) {
        if (current->relatedDoctorId == doctorId && isSamePatientId(current->patientId, patientId)) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

// 判断某条病历是否属于当前医生。
static int doctorOwnsRecord(int doctorId, int recordId) {
    MedicalRecord* record = searchMedicalRecordById(recordId);
    return record != NULL && record->relatedDoctorId == doctorId;
}

// 检查输入文本里是否含有文件分隔符。
static int containsSeparator(const char* text) {
    // 文本文件用 | 分隔字段，用户输入里不能混进这个字符。
    return text != NULL && strchr(text, '|') != NULL;
}

// 校验性别字段是否为系统支持的取值。
static int isValidGender(const char* gender) {
    return strcmp(gender, "男") == 0 || strcmp(gender, "女") == 0;
}

// 校验患者类型是否为门诊或住院。
static int isValidPatientType(const char* type) {
    return strcmp(type, "门诊") == 0 || strcmp(type, "住院") == 0;
}

// 检查普通文本输入是否为空或含有分隔符。
static int validateCommonText(const char* label, const char* value) {
    if (value == NULL || value[0] == '\0') {
        printf("%s不能为空。\n", label);
        return 0;
    }
    if (containsSeparator(value)) {
        printf("%s不能包含竖线 |。\n", label);
        return 0;
    }
    return 1;
}

// 取得今天的日期字符串。
static void getTodayDate(char* buffer, int bufferSize) {
    time_t nowtime;
    struct tm local;

    if (buffer == NULL || bufferSize <= 0) return;
    time(&nowtime);
    localtime_s(&local, &nowtime);
    snprintf(buffer, bufferSize, "%04d-%02d-%02d",
        local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

// 打印单个患者，找不到时给出提示。
static void printSinglePatient(Patient* patient) {
    if (patient == NULL) {
        printf("未找到患者。\n");
        return;
    }
    printPatientTableHeader();
    printPatient(patient);
    printPatientTableBorder();
}

// 打印单个医生，找不到时给出提示。
static void printSingleDoctor(Doctor* doctor) {
    if (doctor == NULL) {
        printf("未找到医生。\n");
        return;
    }
    printDoctor(doctor);
}

// 处理新增患者表单，并按需要继续办理住院。
static void runAddPatient() {
    Patient temp;
    Patient* patient;
    char requestedType[PATIENT_TYPE_LEN];
    int wardId;

    memset(&temp, 0, sizeof(temp));
    if (!readTokenPrompt("患者ID(身份证号): ", temp.patientId, PATIENT_ID_LEN)) return;
    if (!isValidPatientIdCard(temp.patientId)) {
        printf("身份证号格式错误：请输入 18 位身份证号，前 17 位为数字，最后 1 位为数字或大写 X。\n");
        return;
    }
    if (searchPatientById(temp.patientId) != NULL) {
        printf("患者ID已存在。\n");
        return;
    }
    if (!readLinePrompt("姓名: ", temp.name, PATIENT_NAME_LEN) || !validateCommonText("姓名", temp.name)) return;
    if (!readTokenPrompt("性别(男/女): ", temp.gender, PATIENT_GENDER_LEN) || !isValidGender(temp.gender)) {
        printf("性别只能是 男 或 女。\n");
        return;
    }
    if (!readIntPrompt("年龄: ", &temp.age) || temp.age < 0 || temp.age > 120) {
        printf("年龄需在 0-120 之间。\n");
        return;
    }
    if (!readTokenPrompt("电话: ", temp.phone, PATIENT_PHONE_LEN)) return;
    readLinePrompt("诊断(可空，默认无): ", temp.diagnosis, PATIENT_DIAGNOSIS_LEN);
    if (temp.diagnosis[0] == '\0') copyText(temp.diagnosis, PATIENT_DIAGNOSIS_LEN, "无");
    if (containsSeparator(temp.diagnosis)) {
        printf("诊断不能包含竖线 |。\n");
        return;
    }
    if (!readTokenPrompt("患者类型(门诊/住院): ", requestedType, PATIENT_TYPE_LEN) || !isValidPatientType(requestedType)) {
        printf("患者类型只能是 门诊 或 住院。\n");
        return;
    }

    // 先按门诊身份入库；如果选择住院，再走住院流程去分配床位。
    copyText(temp.typeOfPatient, PATIENT_TYPE_LEN, "门诊");
    temp.relatedWardId = -1;
    temp.relatedBedId = -1;
    temp.next = NULL;

    patient = (Patient*)malloc(sizeof(Patient));
    if (patient == NULL) {
        printf("内存分配失败。\n");
        return;
    }
    *patient = temp;
    addPatient(patient);
    printf("患者添加成功。\n");

    if (strcmp(requestedType, "住院") == 0) {
        if (!readIntPrompt("住院患者请输入病房ID: ", &wardId)) return;
        int result = admitPatient(temp.patientId, wardId);
        if (result > 0) printf("住院绑定成功，记录ID=%d。\n", result);
        else printf("住院绑定失败，患者已保留为门诊状态，错误码=%d。\n", result);
    }
}

// 处理患者信息修改。
static void runModifyPatient() {
    char patientId[PATIENT_ID_LEN];
    Patient* target;
    Patient temp;

    if (!readTokenPrompt("要修改的患者ID(身份证号): ", patientId, sizeof(patientId))) return;
    target = searchPatientById(patientId);
    if (target == NULL) {
        printf("患者不存在。\n");
        return;
    }
    temp = *target;
    if (!readLinePrompt("新姓名: ", temp.name, PATIENT_NAME_LEN) || !validateCommonText("姓名", temp.name)) return;
    if (!readTokenPrompt("新性别(男/女): ", temp.gender, PATIENT_GENDER_LEN) || !isValidGender(temp.gender)) {
        printf("性别只能是 男 或 女。\n");
        return;
    }
    if (!readIntPrompt("新年龄: ", &temp.age) || temp.age < 0 || temp.age > 120) {
        printf("年龄需在 0-120 之间。\n");
        return;
    }
    if (!readTokenPrompt("新电话: ", temp.phone, PATIENT_PHONE_LEN)) return;
    if (!readLinePrompt("新诊断: ", temp.diagnosis, PATIENT_DIAGNOSIS_LEN) || !validateCommonText("诊断", temp.diagnosis)) return;
    modifyPatient(&temp);
    printf("患者信息已修改。\n");
}

// 删除门诊患者，住院患者必须先出院。
static void runDeletePatient() {
    char patientId[PATIENT_ID_LEN];
    Patient* patient;
    int result;

    if (!readTokenPrompt("要删除的患者ID(身份证号): ", patientId, sizeof(patientId))) return;
    patient = searchPatientById(patientId);
    if (patient != NULL && strcmp(patient->typeOfPatient, "住院") == 0) {
        printf("患者仍在住院，请先办理出院。\n");
        return;
    }
    result = deletePatientById(patientId);
    printf(result == 1 ? "患者已删除。\n" : "患者不存在。\n");
}

// 患者资料管理菜单。
static void patientMenu() {
    while (1) {
        printf("\n===== 患者管理 =====\n");
        printf("1. 新增患者\n2. 修改患者\n3. 删除患者\n4. 按ID查询\n5. 按姓名查询\n6. 查看全部患者\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: runAddPatient(); break;
        case 2: runModifyPatient(); break;
        case 3: runDeletePatient(); break;
        case 4: {
            char id[PATIENT_ID_LEN];
            if (readTokenPrompt("患者ID(身份证号): ", id, sizeof(id))) printSinglePatient(searchPatientById(id));
            break;
        }
        case 5: {
            char name[PATIENT_NAME_LEN];
            if (readLinePrompt("患者姓名: ", name, sizeof(name))) printPatientsByName(name);
            break;
        }
        case 6: printAllPatients(); break;
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 处理新增科室表单。
static void runAddDepartment() {
    Department* dep;

    dep = (Department*)malloc(sizeof(Department));
    if (dep == NULL) {
        printf("内存分配失败。\n");
        return;
    }
    memset(dep, 0, sizeof(Department));
    if (!readIntPrompt("科室ID: ", &dep->departmentId)) { free(dep); return; }
    if (searchDepartmentById(dep->departmentId) != NULL) {
        printf("科室ID已存在。\n");
        free(dep);
        return;
    }
    if (!readLinePrompt("科室名称: ", dep->name, DEPARTMENT_NAME_LEN) || !validateCommonText("科室名称", dep->name)) {
        free(dep);
        return;
    }
    dep->numberOfDoctors = 0;
    dep->next = NULL;
    addDepartment(dep);
    printf("科室添加成功。\n");
}

// 处理科室名称修改。
static void runModifyDepartment() {
    int id;
    Department temp;
    Department* dep;

    if (!readIntPrompt("要修改的科室ID: ", &id)) return;
    dep = searchDepartmentById(id);
    if (dep == NULL) {
        printf("科室不存在。\n");
        return;
    }
    temp = *dep;
    if (!readLinePrompt("新科室名称: ", temp.name, DEPARTMENT_NAME_LEN) || !validateCommonText("科室名称", temp.name)) return;
    modifyDepartment(&temp);
    printf("科室已修改。\n");
}

// 删除没有关联医生的科室。
static void runDeleteDepartment() {
    int id;

    if (!readIntPrompt("要删除的科室ID: ", &id)) return;
    if (countDoctorsByDepartment(id) > 0) {
        printf("该科室仍有关联医生，不能删除。\n");
        return;
    }
    printf(deleteDepartmentById(id) == 1 ? "科室已删除。\n" : "科室不存在。\n");
}

// 科室管理菜单。
static void departmentMenu() {
    while (1) {
        printf("\n===== 科室管理 =====\n");
        printf("1. 新增科室\n2. 修改科室\n3. 删除科室\n4. 按ID查询\n5. 查看全部科室\n6. 医生数量达标检查\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: runAddDepartment(); break;
        case 2: runModifyDepartment(); break;
        case 3: runDeleteDepartment(); break;
        case 4: {
            int id;
            Department* dep;
            if (readIntPrompt("科室ID: ", &id)) {
                dep = searchDepartmentById(id);
                if (dep == NULL) printf("科室不存在。\n");
                else {
                    const int widths[] = { 10, 18, 10, 8 };
                    const char* headers[] = { "科室ID", "科室名称", "医生数", "状态" };
                    recalculateDepartmentDoctorCounts();
                    printTableBorder(widths, 4);
                    printTableRow(headers, widths, 4);
                    printTableBorder(widths, 4);
                    printDepartment(dep);
                    printTableBorder(widths, 4);
                }
            }
            break;
        }
        case 5: printAllDepartments(); break;
        case 6: printDepartmentCompliance(); break;
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 处理新增医生表单。
static void runAddDoctor() {
    Doctor* doctor;

    doctor = (Doctor*)malloc(sizeof(Doctor));
    if (doctor == NULL) {
        printf("内存分配失败。\n");
        return;
    }
    memset(doctor, 0, sizeof(Doctor));
    if (!readIntPrompt("医生ID: ", &doctor->doctorId)) { free(doctor); return; }
    if (searchDoctorById(doctor->doctorId) != NULL) {
        printf("医生ID已存在。\n");
        free(doctor);
        return;
    }
    if (!readLinePrompt("姓名: ", doctor->name, DOCTOR_NAME_LEN) || !validateCommonText("姓名", doctor->name)) { free(doctor); return; }
    if (!readTokenPrompt("性别(男/女): ", doctor->gender, sizeof(doctor->gender)) || !isValidGender(doctor->gender)) { printf("性别只能是 男 或 女。\n"); free(doctor); return; }
    if (!readIntPrompt("年龄: ", &doctor->age) || doctor->age < 20 || doctor->age > 80) { printf("年龄需在 20-80 之间。\n"); free(doctor); return; }
    if (!readTokenPrompt("电话: ", doctor->phone, DOCTOR_PHONE_LEN)) { free(doctor); return; }
    if (!readIntPrompt("科室ID: ", &doctor->departmentId) || searchDepartmentById(doctor->departmentId) == NULL) { printf("科室不存在。\n"); free(doctor); return; }
    if (!readLinePrompt("职称: ", doctor->title, DOCTOR_TITLE_LEN) || !validateCommonText("职称", doctor->title)) { free(doctor); return; }
    if (!readLinePrompt("专业: ", doctor->specialty, DOCTOR_SPECIALTY_LEN) || !validateCommonText("专业", doctor->specialty)) { free(doctor); return; }
    readLinePrompt("身份证号(可空): ", doctor->idCard, DOCTOR_ID_CARD_LEN);
    if (doctor->idCard[0] == '\0') copyText(doctor->idCard, DOCTOR_ID_CARD_LEN, "none");
    copyText(doctor->type, DOCTOR_SPECIALTY_LEN, doctor->title);
    doctor->onDuty = 1;
    doctor->bedId = -1;
    doctor->next = NULL;
    addDoctor(doctor);
    printf("医生添加成功。\n");
}

// 处理医生信息修改。
static void runModifyDoctor() {
    int id;
    Doctor* target;
    Doctor temp;

    if (!readIntPrompt("要修改的医生ID: ", &id)) return;
    target = searchDoctorById(id);
    if (target == NULL) {
        printf("医生不存在。\n");
        return;
    }
    temp = *target;
    if (!readLinePrompt("新姓名: ", temp.name, DOCTOR_NAME_LEN) || !validateCommonText("姓名", temp.name)) return;
    if (!readTokenPrompt("新性别(男/女): ", temp.gender, sizeof(temp.gender)) || !isValidGender(temp.gender)) { printf("性别只能是 男 或 女。\n"); return; }
    if (!readIntPrompt("新年龄: ", &temp.age) || temp.age < 20 || temp.age > 80) { printf("年龄需在 20-80 之间。\n"); return; }
    if (!readTokenPrompt("新电话: ", temp.phone, DOCTOR_PHONE_LEN)) return;
    if (!readIntPrompt("新科室ID: ", &temp.departmentId) || searchDepartmentById(temp.departmentId) == NULL) { printf("科室不存在。\n"); return; }
    if (!readLinePrompt("新职称: ", temp.title, DOCTOR_TITLE_LEN) || !validateCommonText("职称", temp.title)) return;
    if (!readLinePrompt("新专业: ", temp.specialty, DOCTOR_SPECIALTY_LEN) || !validateCommonText("专业", temp.specialty)) return;
    if (!readIntPrompt("是否在岗(1是/0否): ", &temp.onDuty) || (temp.onDuty != 0 && temp.onDuty != 1)) { printf("状态只能是 1 或 0。\n"); return; }
    modifyDoctor(&temp);
    printf("医生信息已修改。\n");
}

// 删除没有历史病历的医生。
static void runDeleteDoctor() {
    int id;

    if (!readIntPrompt("要删除的医生ID: ", &id)) return;
    if (countMedicalRecordsByDoctor(id) > 0) {
        printf("该医生已有医疗记录，不能删除。\n");
        return;
    }
    printf(deleteDoctorById(id) == 1 ? "医生已删除。\n" : "医生不存在。\n");
}

// 医生基础资料管理菜单。
static void doctorMenu() {
    while (1) {
        printf("\n===== 医生管理 =====\n");
        printf("1. 新增医生\n2. 修改医生\n3. 删除医生\n4. 按ID查询\n5. 查看全部医生\n6. 按科室查看医生\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: runAddDoctor(); break;
        case 2: runModifyDoctor(); break;
        case 3: runDeleteDoctor(); break;
        case 4: {
            int id;
            if (readIntPrompt("医生ID: ", &id)) printSingleDoctor(searchDoctorById(id));
            break;
        }
        case 5: printAllDoctors(); break;
        case 6: {
            int deptId;
            if (readIntPrompt("科室ID: ", &deptId)) printDoctorsByDepartment(deptId);
            break;
        }
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 新建病房时自动补齐对应数量的空床。
static void createBedsForWard(int wardId, int totalBeds) {
    int i;

    // 新建病房时顺手生成床位，演示时能体现病房和床位的联动。
    for (i = 0; i < totalBeds; ++i) {
        Bed* bed = (Bed*)malloc(sizeof(Bed));
        if (bed == NULL) return;
        bed->bedId = generateNextBedId();
        bed->relatedWardId = wardId;
        bed->status = 0;
        copyText(bed->relatedPatientId, PATIENT_ID_LEN, PATIENT_ID_NONE);
        bed->next = NULL;
        addBed(bed);
    }
}

// 处理新增病房并初始化床位。
static void runAddWard() {
    Ward* ward = (Ward*)malloc(sizeof(Ward));

    if (ward == NULL) {
        printf("内存分配失败。\n");
        return;
    }
    memset(ward, 0, sizeof(Ward));
    if (!readIntPrompt("病房ID: ", &ward->wardId)) { free(ward); return; }
    if (searchWardById(ward->wardId) != NULL) { printf("病房ID已存在。\n"); free(ward); return; }
    if (!readTokenPrompt("病房类型(如普通/重症/专科): ", ward->typeOfWard, WARD_TYPE_LEN)) { free(ward); return; }
    if (!readIntPrompt("关联科室ID: ", &ward->relatedDepartmentId) || searchDepartmentById(ward->relatedDepartmentId) == NULL) { printf("科室不存在。\n"); free(ward); return; }
    if (!readIntPrompt("总床位数: ", &ward->totalBedOfWard) || ward->totalBedOfWard <= 0) { printf("总床位数必须为正数。\n"); free(ward); return; }
    ward->availableBedOfWard = ward->totalBedOfWard;
    ward->next = NULL;
    addWard(ward);
    createBedsForWard(ward->wardId, ward->totalBedOfWard);
    recalculateWardAvailability();
    printf("病房添加成功，并已自动初始化 %d 张空闲床位。\n", ward->totalBedOfWard);
}

// 处理新增单个床位。
static void runAddBed() {
    Bed* bed = (Bed*)malloc(sizeof(Bed));

    if (bed == NULL) {
        printf("内存分配失败。\n");
        return;
    }
    memset(bed, 0, sizeof(Bed));
    if (!readIntPrompt("床位ID(输入0自动生成): ", &bed->bedId)) { free(bed); return; }
    if (bed->bedId == 0) bed->bedId = generateNextBedId();
    if (searchBedById(bed->bedId) != NULL) { printf("床位ID已存在。\n"); free(bed); return; }
    if (!readIntPrompt("所属病房ID: ", &bed->relatedWardId) || searchWardById(bed->relatedWardId) == NULL) { printf("病房不存在。\n"); free(bed); return; }
    bed->status = 0;
    copyText(bed->relatedPatientId, PATIENT_ID_LEN, PATIENT_ID_NONE);
    bed->next = NULL;
    addBed(bed);
    recalculateWardAvailability();
    printf("床位添加成功。\n");
}

// 病房和床位管理菜单。
static void wardBedMenu() {
    while (1) {
        printf("\n===== 病房床位管理 =====\n");
        printf("1. 新增病房并初始化床位\n2. 新增单个床位\n3. 办理住院\n4. 办理出院\n5. 查看病房列表\n6. 查看床位列表\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: runAddWard(); break;
        case 2: runAddBed(); break;
        case 3: {
            char patientId[PATIENT_ID_LEN];
            int wardId, result;
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            if (!readIntPrompt("病房ID: ", &wardId)) break;
            result = admitPatient(patientId, wardId);
            if (result > 0) printf("办理住院成功，记录ID=%d。\n", result);
            else printf("办理住院失败，错误码=%d。\n", result);
            break;
        }
        case 4: {
            char patientId[PATIENT_ID_LEN];
            int result;
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            result = dischargePatient(patientId);
            if (result > 0) printf("办理出院成功，记录ID=%d。\n", result);
            else printf("办理出院失败，错误码=%d。\n", result);
            break;
        }
        case 5: printAllWards(); break;
        case 6: printAllBeds(); break;
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 打印某科室可以挂号的医生列表。
static int printRegistrationDoctorsByDepartment(int departmentId) {
    Doctor* current = doctorListHead;
    Department* department = searchDepartmentById(departmentId);
    const int widths[] = { 8, 12, 6, 6, 12, 16, 8, 8 };
    const char* headers[] = { "医生ID", "姓名", "性别", "年龄", "职称", "专业", "状态", "候诊数" };
    int found = 0;

    printf("\n========== %s 可挂号医生 ==========\n", department == NULL ? "该科室" : department->name);
    printTableBorder(widths, 8);
    printTableRow(headers, widths, 8);
    printTableBorder(widths, 8);
    while (current != NULL) {
        if (current->departmentId == departmentId) {
            char doctorId[16];
            char age[16];
            char waitingCount[16];
            const char* cells[8];

            snprintf(doctorId, sizeof(doctorId), "%d", current->doctorId);
            snprintf(age, sizeof(age), "%d", current->age);
            snprintf(waitingCount, sizeof(waitingCount), "%d", countWaitingByDoctor(current->doctorId));
            cells[0] = doctorId;
            cells[1] = current->name;
            cells[2] = current->gender;
            cells[3] = age;
            cells[4] = current->title;
            cells[5] = current->specialty;
            cells[6] = current->onDuty ? "在岗" : "停诊";
            cells[7] = waitingCount;
            printTableRow(cells, widths, 8);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(该科室暂无医生)\n");
    printTableBorder(widths, 8);
    return found;
}

// 让用户先选科室再选医生，返回可挂号医生 ID。
static int chooseDoctorByDepartmentForRegistration() {
    int departmentId;
    int doctorId;
    Doctor* doctor;

    if (countDepartments() == 0) {
        printAllDepartments();
        return -1;
    }

    printAllDepartments();
    if (!readIntPrompt("请选择科室ID: ", &departmentId)) return -1;
    if (searchDepartmentById(departmentId) == NULL) {
        printf("科室不存在。\n");
        return -1;
    }
    if (!printRegistrationDoctorsByDepartment(departmentId)) return -1;

    if (!readIntPrompt("请选择医生ID: ", &doctorId)) return -1;
    doctor = searchDoctorById(doctorId);
    if (doctor == NULL) {
        printf("医生不存在。\n");
        return -1;
    }
    if (doctor->departmentId != departmentId) {
        printf("该医生不属于所选科室。\n");
        return -1;
    }
    if (!doctor->onDuty) {
        printf("该医生当前停诊，不能挂号。\n");
        return -1;
    }
    return doctorId;
}

// 把挂号返回码翻译成用户能看懂的提示。
static void printRegistrationResult(const char* registrationName, int result) {
    if (result > 0) {
        printf("%s成功，记录ID=%d。\n", registrationName, result);
    }
    else if (result == -1) printf("%s失败：患者不存在。\n", registrationName);
    else if (result == -2) printf("%s失败：医生不存在。\n", registrationName);
    else if (result == -3) printf("%s失败：预约时段只能输入 1、2 或 3。\n", registrationName);
    else if (result == -4) printf("%s失败：内存分配失败。\n", registrationName);
    else if (result == -5) printf("%s失败：候诊队列写入失败。\n", registrationName);
    else if (result == -6) printf("%s失败：该患者在同一天同一时段已挂过该医生。\n", registrationName);
    else if (result == -7) printf("%s失败：预约日期不能为空。\n", registrationName);
    else if (result == -8) printf("%s失败：预约日期不合法，请按 YYYY-MM-DD 输入真实日期。\n", registrationName);
    else printf("%s失败，错误码=%d。\n", registrationName, result);
}

// 患者或管理员发起现场挂号。
static void runPatientOnsiteRegistration(const char* patientId) {
    int doctorId = chooseDoctorByDepartmentForRegistration();
    int result;

    if (doctorId < 0) return;
    result = registerPatientOnsite(patientId, doctorId);
    printRegistrationResult("现场挂号", result);
}

// 患者或管理员发起预约挂号。
static void runPatientAppointmentRegistration(const char* patientId) {
    int doctorId = chooseDoctorByDepartmentForRegistration();
    int slot;
    int result;
    char date[32];

    if (doctorId < 0) return;
    if (!readTokenPrompt("预约日期(YYYY-MM-DD): ", date, sizeof(date))) return;
    if (!readIntPrompt("预约时段(1上午/2下午/3晚上): ", &slot)) return;
    result = registerPatientAppointment(patientId, doctorId, date, slot);
    printRegistrationResult("预约挂号", result);
}

// 挂号诊疗管理菜单。
static void medicalMenu() {
    // 管理员入口可以跑完整诊疗流程：挂号 -> 叫号 -> 看诊 -> 检查/住院/发药。
    while (1) {
        printf("\n===== 挂号诊疗管理 =====\n");
        printf("1. 现场挂号\n2. 预约挂号\n3. 医生叫号\n4. 查看医生候诊队列\n5. 看诊\n6. 添加检查\n7. 查看全部病历\n8. 按患者查询病历\n9. 按医生查询病历\n10. 当前叫号信息\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: {
            char patientId[PATIENT_ID_LEN];
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            runPatientOnsiteRegistration(patientId);
            break;
        }
        case 2: {
            char patientId[PATIENT_ID_LEN];
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            runPatientAppointmentRegistration(patientId);
            break;
        }
        case 3: {
            int doctorId, result;
            if (!readIntPrompt("医生ID: ", &doctorId)) break;
            result = callNextPatientByDoctor(doctorId);
            if (result > 0) printf("叫号成功，当前记录ID=%d。\n", result);
            else printf("叫号失败，错误码=%d。\n", result);
            break;
        }
        case 4: {
            int doctorId;
            if (readIntPrompt("医生ID: ", &doctorId)) printWaitingQueueByDoctor(doctorId);
            break;
        }
        case 5: {
            int recordId, result;
            char diagnosis[PATIENT_DIAGNOSIS_LEN];
            if (!readIntPrompt("记录ID: ", &recordId)) break;
            if (!readLinePrompt("诊断: ", diagnosis, sizeof(diagnosis)) || !validateCommonText("诊断", diagnosis)) break;
            result = consultPatient(recordId, diagnosis);
            if (result == 1) printf("看诊完成。\n");
            else printf("看诊失败，错误码=%d。\n", result);
            break;
        }
        case 6: {
            char patientId[PATIENT_ID_LEN];
            int doctorId, result;
            char item[MEDICAL_RECORD_DETAIL_LEN];
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            if (!readIntPrompt("医生ID: ", &doctorId)) break;
            if (!readLinePrompt("检查项目: ", item, sizeof(item)) || !validateCommonText("检查项目", item)) break;
            result = addExamination(patientId, doctorId, item);
            if (result > 0) printf("检查记录已添加，记录ID=%d。\n", result);
            else printf("添加检查失败，错误码=%d。\n", result);
            break;
        }
        case 7: printAllMedicalRecord(); break;
        case 8: {
            char patientId[PATIENT_ID_LEN];
            if (readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) printMedicalRecordsByPatientId(patientId);
            break;
        }
        case 9: {
            int doctorId;
            if (readIntPrompt("医生ID: ", &doctorId)) printMedicalRecordsByDoctorId(doctorId);
            break;
        }
        case 10: printCurrentCalledPatients(); break;
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 处理新增药品表单。
static void runAddMedicine() {
    Medicine* medicine = (Medicine*)malloc(sizeof(Medicine));

    if (medicine == NULL) {
        printf("内存分配失败。\n");
        return;
    }
    memset(medicine, 0, sizeof(Medicine));
    if (!readIntPrompt("药品ID: ", &medicine->medicineId)) { free(medicine); return; }
    if (searchMedicineById(medicine->medicineId) != NULL) { printf("药品ID已存在。\n"); free(medicine); return; }
    if (!readLinePrompt("通用名: ", medicine->genericName, MEDICINE_NAME_LEN) || !validateCommonText("通用名", medicine->genericName)) { free(medicine); return; }
    if (!readLinePrompt("商品名: ", medicine->brandName, MEDICINE_NAME_LEN) || !validateCommonText("商品名", medicine->brandName)) { free(medicine); return; }
    if (!readLinePrompt("别名: ", medicine->aliasName, MEDICINE_NAME_LEN) || !validateCommonText("别名", medicine->aliasName)) { free(medicine); return; }
    if (!readLinePrompt("类别: ", medicine->category, MEDICINE_CATEGORY_LEN) || !validateCommonText("类别", medicine->category)) { free(medicine); return; }
    if (!readTokenPrompt("单位: ", medicine->unit, MEDICINE_UNIT_LEN)) { free(medicine); return; }
    if (!readIntPrompt("库存: ", &medicine->inventory) || medicine->inventory < 0) { printf("库存不能为负数。\n"); free(medicine); return; }
    if (!readIntPrompt("单价: ", &medicine->price) || medicine->price < 0) { printf("单价不能为负数。\n"); free(medicine); return; }
    if (!readIntPrompt("关联科室ID: ", &medicine->relatedDepartmentId) || searchDepartmentById(medicine->relatedDepartmentId) == NULL) { printf("科室不存在。\n"); free(medicine); return; }
    medicine->next = NULL;
    addMedicine(medicine);
    printf("药品添加成功。\n");
}

// 处理药品信息修改。
static void runModifyMedicine() {
    int id;
    Medicine* target;
    Medicine temp;

    if (!readIntPrompt("要修改的药品ID: ", &id)) return;
    target = searchMedicineById(id);
    if (target == NULL) {
        printf("药品不存在。\n");
        return;
    }
    temp = *target;
    if (!readLinePrompt("新通用名: ", temp.genericName, MEDICINE_NAME_LEN) || !validateCommonText("通用名", temp.genericName)) return;
    if (!readLinePrompt("新商品名: ", temp.brandName, MEDICINE_NAME_LEN) || !validateCommonText("商品名", temp.brandName)) return;
    if (!readLinePrompt("新别名: ", temp.aliasName, MEDICINE_NAME_LEN) || !validateCommonText("别名", temp.aliasName)) return;
    if (!readLinePrompt("新类别: ", temp.category, MEDICINE_CATEGORY_LEN) || !validateCommonText("类别", temp.category)) return;
    if (!readTokenPrompt("新单位: ", temp.unit, MEDICINE_UNIT_LEN)) return;
    if (!readIntPrompt("新库存: ", &temp.inventory) || temp.inventory < 0) { printf("库存不能为负数。\n"); return; }
    if (!readIntPrompt("新单价: ", &temp.price) || temp.price < 0) { printf("单价不能为负数。\n"); return; }
    if (!readIntPrompt("新关联科室ID: ", &temp.relatedDepartmentId) || searchDepartmentById(temp.relatedDepartmentId) == NULL) { printf("科室不存在。\n"); return; }
    modifyMedicine(&temp);
    printf("药品已修改。\n");
}

// 药品、库存和药品流水管理菜单。
static void medicineMenu() {
    while (1) {
        printf("\n===== 药品药房管理 =====\n");
        printf("1. 新增药品\n2. 修改药品\n3. 删除药品\n4. 按ID查询\n5. 按名称查询\n6. 查看全部药品\n7. 药品入库\n8. 处方发药\n9. 查看全部药品流水\n10. 按药品ID查流水\n11. 按患者ID查流水\n12. 按医生ID查流水\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: runAddMedicine(); break;
        case 2: runModifyMedicine(); break;
        case 3: {
            int id;
            if (readIntPrompt("要删除的药品ID: ", &id)) printf(deleteMedicineById(id) == 1 ? "药品已删除。\n" : "药品不存在。\n");
            break;
        }
        case 4: {
            int id;
            Medicine* medicine;
            if (readIntPrompt("药品ID: ", &id)) {
                medicine = searchMedicineById(id);
                if (medicine == NULL) printf("药品不存在。\n");
                else printMedicine(medicine);
            }
            break;
        }
        case 5: {
            char name[MEDICINE_NAME_LEN];
            if (readLinePrompt("药品名关键词: ", name, sizeof(name))) printMedicinesByName(name);
            break;
        }
        case 6: printAllMedicines(); break;
        case 7: {
            int id, qty, result;
            if (!readIntPrompt("药品ID: ", &id)) break;
            if (!readIntPrompt("入库数量: ", &qty)) break;
            result = addMedicineStock(id, qty);
            if (result == 1) {
                addPharmacyLogEntry(id, "入库", qty, PATIENT_ID_NONE, -1, "药品入库");
                printf("入库成功。\n");
            }
            else printf("入库失败，错误码=%d。\n", result);
            break;
        }
        case 8: {
            char patientId[PATIENT_ID_LEN];
            int doctorId, medicineId, qty, result;
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            if (!readIntPrompt("医生ID: ", &doctorId)) break;
            if (!readIntPrompt("药品ID: ", &medicineId)) break;
            if (!readIntPrompt("发药数量: ", &qty)) break;
            result = addPrescription(patientId, doctorId, medicineId, qty);
            if (result > 0) printf("发药成功，病历记录ID=%d。\n", result);
            else printf("发药失败，错误码=%d。\n", result);
            break;
        }
        case 9: printAllPharmacyLogs(); break;
        case 10: {
            int medicineId;
            if (readIntPrompt("药品ID: ", &medicineId)) printPharmacyLogsByMedicineId(medicineId);
            break;
        }
        case 11: {
            char patientId[PATIENT_ID_LEN];
            if (readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) printPharmacyLogsByPatientId(patientId);
            break;
        }
        case 12: {
            int doctorId;
            if (readIntPrompt("医生ID: ", &doctorId)) printPharmacyLogsByDoctorId(doctorId);
            break;
        }
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 打印每位医生当前候诊人数。
static void printDoctorWaitingStats() {
    Doctor* doctor = doctorListHead;
    const int widths[] = { 8, 12, 10 };
    const char* headers[] = { "医生ID", "姓名", "候诊人数" };

    printf("\n========== 各医生候诊人数 ==========\n");
    printTableBorder(widths, 3);
    printTableRow(headers, widths, 3);
    printTableBorder(widths, 3);
    while (doctor != NULL) {
        char doctorId[16];
        char waitingCount[16];
        const char* cells[3];

        snprintf(doctorId, sizeof(doctorId), "%d", doctor->doctorId);
        snprintf(waitingCount, sizeof(waitingCount), "%d", countWaitingByDoctor(doctor->doctorId));
        cells[0] = doctorId;
        cells[1] = doctor->name;
        cells[2] = waitingCount;
        printTableRow(cells, widths, 3);
        doctor = doctor->next;
    }
    printTableBorder(widths, 3);
}

// 汇总管理视角的核心运营统计。
static void printManagementReport() {
    Ward* ward;
    Medicine* medicine;
    char today[MEDICAL_RECORD_DATE];
    int totalInventory = 0;
    int totalBeds = 0;
    int occupiedBeds = 0;

    // 报表不直接相信缓存字段，先把几个统计值重新算一遍。
    getTodayDate(today, sizeof(today));
    recalculateDepartmentDoctorCounts();
    recalculateWardAvailability();
    ward = wardListHead;
    while (ward != NULL) {
        totalBeds += ward->totalBedOfWard;
        occupiedBeds += ward->totalBedOfWard - ward->availableBedOfWard;
        ward = ward->next;
    }
    medicine = medicineListHead;
    while (medicine != NULL) {
        totalInventory += medicine->inventory;
        medicine = medicine->next;
    }

    printf("\n================ 管理视角报表 ================\n");
    printf("患者总数: %d\n", countPatients());
    printf("门诊人数: %d\n", countPatientsByType("门诊"));
    printf("住院人数: %d\n", countPatientsByType("住院"));
    printf("医生总数: %d\n", countDoctors());
    printf("科室总数: %d\n", countDepartments());
    printf("病房数量: %d\n", countWards());
    printf("病房类型数: %d\n", countWardTypes());
    printf("床位总数: %d\n", totalBeds);
    printf("已占床位: %d\n", occupiedBeds);
    printf("床位占用率: %.1f%%\n", totalBeds > 0 ? occupiedBeds * 100.0 / totalBeds : 0.0);
    printf("药品种类: %d\n", countMedicines());
    printf("药品库存总量: %d\n", totalInventory);
    printf("病历记录数: %d\n", countMedicalRecords());
    printf("今日预约挂号数: %d\n", countRegistrationsByDateAndType(today, REG_TYPE_APPOINTMENT));
    printf("今日现场挂号数: %d\n", countRegistrationsByDateAndType(today, REG_TYPE_ONSITE));
    printf("==============================================\n");
    printDepartmentCompliance();
    printDoctorWaitingStats();
}

// 按科室展示医生和关联药品。
static void printDepartmentDimension() {
    int deptId;
    Medicine* medicine;
    if (!readIntPrompt("科室ID: ", &deptId)) return;
    if (searchDepartmentById(deptId) == NULL) {
        printf("科室不存在。\n");
        return;
    }
    printDoctorsByDepartment(deptId);
    printf("该科室药品：\n");
    printMedicineTableHeader();
    medicine = medicineListHead;
    while (medicine != NULL) {
        if (medicine->relatedDepartmentId == deptId) printMedicine(medicine);
        medicine = medicine->next;
    }
    printMedicineTableBorder();
}

// 报表查询菜单。
static void reportMenu() {
    while (1) {
        printf("\n===== 报表 =====\n");
        printf("1. 管理视角报表\n2. 医护视角\n3. 患者视角\n4. 按科室维度查询\n5. 按药品名查询\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: printManagementReport(); break;
        case 2: {
            int doctorId;
            if (!readIntPrompt("医生ID: ", &doctorId)) break;
            printSingleDoctor(searchDoctorById(doctorId));
            printWaitingQueueByDoctor(doctorId);
            printCalledRecordsByDoctorId(doctorId);
            printMedicalRecordsByDoctorId(doctorId);
            break;
        }
        case 3: {
            char patientId[PATIENT_ID_LEN];
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            printSinglePatient(searchPatientById(patientId));
            printMedicalRecordsByPatientId(patientId);
            printPharmacyLogsByPatientId(patientId);
            break;
        }
        case 4: printDepartmentDimension(); break;
        case 5: {
            char name[MEDICINE_NAME_LEN];
            if (readLinePrompt("药品名关键词: ", name, sizeof(name))) printMedicinesByName(name);
            break;
        }
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 变更日志查询菜单（管理员专用）。
static void changeLogMenu() {
    while (1) {
        printf("\n===== 变更日志 =====\n");
        printf("1. 查看全部\n2. 按模块筛选\n3. 按对象名称搜索\n0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1:
            printAllChangeLogs();
            break;
        case 2: {
            char moduleName[CHANGE_LOG_MODULE_LEN];
            printf("可选模块: 患者管理 / 医生管理 / 药品管理 / 挂号管理\n");
            if (readLinePrompt("模块名称: ", moduleName, sizeof(moduleName)) &&
                validateCommonText("模块名称", moduleName)) {
                printChangeLogsByModule(moduleName);
            }
            break;
        }
        case 3: {
            char keyword[CHANGE_LOG_TARGET_LEN];
            if (readLinePrompt("对象名称关键词: ", keyword, sizeof(keyword)) &&
                validateCommonText("对象名称关键词", keyword)) {
                printChangeLogsByTargetKeyword(keyword);
            }
            break;
        }
        case 0:
            return;
        default:
            printf("无效输入。\n");
        }
    }
}

// 一次性打印主要基础表和业务表。
static void printAllCoreLists() {
    printAllDepartments();
    printAllDoctors();
    printAllPatients();
    printAllWards();
    printAllBeds();
    printAllMedicines();
    printAllPharmacyLogs();
    printAllMedicalRecord();
}

// 患者注册时自动补一份默认患者档案。
static int syncPatientForAccountRegister(const char* idCard, const char* name, const char* gender, int age) {
    Patient* existing;
    Patient temp;

    if (idCard == NULL || idCard[0] == '\0') return 0;
    if (name == NULL || name[0] == '\0') return 0;
    if (gender == NULL || gender[0] == '\0') return 0;
    if (age < 0 || age > 120) return 0;

    existing = searchPatientById(idCard);
    if (existing != NULL) {
        Patient temp = *existing;
        copyText(temp.name, PATIENT_NAME_LEN, name);
        copyText(temp.gender, PATIENT_GENDER_LEN, gender);
        temp.age = age;
        modifyPatient(&temp);
        return 1;
    }

    memset(&temp, 0, sizeof(temp));
    copyText(temp.patientId, PATIENT_ID_LEN, idCard);
    copyText(temp.name, PATIENT_NAME_LEN, name);
    copyText(temp.gender, PATIENT_GENDER_LEN, gender);
    temp.age = age;
    copyText(temp.diagnosis, PATIENT_DIAGNOSIS_LEN, "无");
    copyText(temp.typeOfPatient, PATIENT_TYPE_LEN, "门诊");
    temp.relatedWardId = -1;
    temp.relatedBedId = -1;
    copyText(temp.phone, PATIENT_PHONE_LEN, "未填写");
    temp.next = NULL;

    existing = (Patient*)malloc(sizeof(Patient));
    if (existing == NULL) {
        printf("内存分配失败。\n");
        return 0;
    }
    *existing = temp;
    addPatient(existing);
    return 1;
}

// 处理患者账号注册。
static void runPatientAccountRegister() {
    char idCard[PATIENT_ID_LEN];
    char password[AUTH_PASSWORD_LEN];
    char name[PATIENT_NAME_LEN];
    char gender[PATIENT_GENDER_LEN];
    int age;
    int result;
    char previousOperator[CHANGE_LOG_OPERATOR_LEN];

    if (!readTokenPrompt("身份证号: ", idCard, sizeof(idCard))) return;
    if (!isValidPatientIdCard(idCard)) {
        printf("身份证号格式错误：请输入 18 位身份证号，前 17 位为数字，最后 1 位为数字或大写 X。\n");
        return;
    }
    if (authAccountExists(idCard)) {
        printf("该账号已存在。\n");
        return;
    }
    if (!readTokenPrompt("密码: ", password, sizeof(password))) return;
    if (!readLinePrompt("姓名: ", name, sizeof(name)) || !validateCommonText("姓名", name)) return;
    if (!readTokenPrompt("性别(男/女): ", gender, sizeof(gender)) || !isValidGender(gender)) {
        printf("性别只能是 男 或 女。\n");
        return;
    }
    if (!readIntPrompt("年龄: ", &age) || age < 0 || age > 120) {
        printf("年龄需在 0-120 之间。\n");
        return;
    }

    copyText(previousOperator, sizeof(previousOperator), getChangeLogOperator());
    setChangeLogOperator(idCard);
    if (!syncPatientForAccountRegister(idCard, name, gender, age)) {
        setChangeLogOperator(previousOperator);
        return;
    }
    result = registerPatientAccount(idCard, password);
    setChangeLogOperator(previousOperator);
    if (result == 1) {
        saveAllData();
        printf("患者账号注册成功，基础信息已同步。\n");
    }
    else if (result == -2) printf("患者基础信息创建失败，不能注册。\n");
    else if (result == -3) printf("该账号已存在。\n");
    else printf("患者账号注册失败。\n");
}

// 处理医生账号注册。
static void runDoctorAccountRegister() {
    char idCard[DOCTOR_ID_CARD_LEN];
    char password[AUTH_PASSWORD_LEN];
    int result;

    if (!readTokenPrompt("医生身份证号: ", idCard, sizeof(idCard))) return;
    if (!readTokenPrompt("初始密码: ", password, sizeof(password))) return;
    result = registerDoctorAccount(idCard, password);
    if (result == 1) {
        saveAllData();
        printf("医生账号注册成功。\n");
    }
    else if (result == -2) printf("未找到该身份证号对应的医生。\n");
    else if (result == -3) printf("该医生账号已存在。\n");
    else printf("医生账号注册失败。\n");
}

// 管理员工作台菜单。
static void adminMenu() {
    // 管理员能维护基础资料，也能查看全局报表和注册医生账号。
    while (1) {
        printf("\n===== 管理员工作台 =====\n");
        printf("1. 患者管理\n");
        printf("2. 医生管理\n");
        printf("3. 科室管理\n");
        printf("4. 病房床位管理\n");
        printf("5. 挂号诊疗管理\n");
        printf("6. 药品药房管理\n");
        printf("7. 报表\n");
        printf("8. 查看全部核心数据\n");
        printf("9. 医生账号注册\n");
        printf("10. 变更日志\n");
        printf("0. 退出登录\n");

        switch (readChoice("请输入功能编号: ")) {
        case 1: patientMenu(); break;
        case 2: doctorMenu(); break;
        case 3: departmentMenu(); break;
        case 4: wardBedMenu(); break;
        case 5: medicalMenu(); break;
        case 6: medicineMenu(); break;
        case 7: reportMenu(); break;
        case 8: printAllCoreLists(); break;
        case 9: runDoctorAccountRegister(); break;
        case 10: changeLogMenu(); break;
        case 0:
            saveAllData();
            return;
        default:
            printf("无效输入。\n");
        }
    }
}

// 医生登录后的工作台菜单。
static void doctorWorkbenchMenu(const Session* session) {
    int doctorId;

    // 医生工作台只使用登录会话里绑定的 doctorId，避免手输 ID 越权。
    if (session == NULL || session->role != ROLE_DOCTOR) return;
    doctorId = session->boundDoctorId;
    while (1) {
        printf("\n===== 医生工作台 =====\n");
        printf("1. 我的资料\n");
        printf("2. 我的候诊队列\n");
        printf("3. 叫号\n");
        printf("4. 看诊\n");
        printf("5. 添加检查\n");
        printf("6. 开处方\n");
        printf("7. 查询我的接诊病历\n");
        printf("0. 退出登录\n");

        switch (readChoice("请选择: ")) {
        case 1:
            printSingleDoctor(searchDoctorById(doctorId));
            break;
        case 2:
            printWaitingQueueByDoctor(doctorId);
            break;
        case 3: {
            int result = callNextPatientByDoctor(doctorId);
            if (result > 0) printf("叫号成功，当前记录ID=%d。\n", result);
            else printf("叫号失败，错误码=%d。\n", result);
            break;
        }
        case 4: {
            int recordId, result;
            char diagnosis[PATIENT_DIAGNOSIS_LEN];
            if (!readIntPrompt("记录ID: ", &recordId)) break;
            if (!doctorOwnsRecord(doctorId, recordId)) {
                printf("无权限操作该记录。\n");
                break;
            }
            if (!readLinePrompt("诊断: ", diagnosis, sizeof(diagnosis)) || !validateCommonText("诊断", diagnosis)) break;
            result = consultPatient(recordId, diagnosis);
            if (result == 1) printf("看诊完成。\n");
            else printf("看诊失败，错误码=%d。\n", result);
            break;
        }
        case 5: {
            char patientId[PATIENT_ID_LEN];
            char item[MEDICAL_RECORD_DETAIL_LEN];
            int result;
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            if (!doctorHasPatientRecord(doctorId, patientId)) {
                printf("无权限操作该患者。\n");
                break;
            }
            if (!readLinePrompt("检查项目: ", item, sizeof(item)) || !validateCommonText("检查项目", item)) break;
            result = addExamination(patientId, doctorId, item);
            if (result > 0) printf("检查记录已添加，记录ID=%d。\n", result);
            else printf("添加检查失败，错误码=%d。\n", result);
            break;
        }
        case 6: {
            char patientId[PATIENT_ID_LEN];
            int medicineId, qty, result;
            if (!readTokenPrompt("患者ID(身份证号): ", patientId, sizeof(patientId))) break;
            if (!doctorHasPatientRecord(doctorId, patientId)) {
                printf("无权限操作该患者。\n");
                break;
            }
            if (!readIntPrompt("药品ID: ", &medicineId)) break;
            if (!readIntPrompt("发药数量: ", &qty)) break;
            result = addPrescription(patientId, doctorId, medicineId, qty);
            if (result > 0) printf("发药成功，病历记录ID=%d。\n", result);
            else printf("发药失败，错误码=%d。\n", result);
            break;
        }
        case 7:
            printMedicalRecordsByDoctorId(doctorId);
            break;
        case 0:
            saveAllData();
            return;
        default:
            printf("无效输入。\n");
        }
    }
}

// 打印当前患者自己的挂号记录。
static void printPatientRegistrationRecords(const char* patientId) {
    MedicalRecord* current = medicalRecordListHead;
    int found = 0;

    printf("\n========== 我的挂号记录 ==========\n");
    while (current != NULL) {
        if (isSamePatientId(current->patientId, patientId) && current->queueNumber > 0) {
            printMedicalRecord(current);
            found = 1;
        }
        current = current->next;
    }
    if (!found) printf("(暂无挂号记录)\n");
    printf("==================================\n");
}

// 打印当前患者的住院和床位信息。
static void printPatientWardBedInfo(const char* patientId) {
    Patient* patient = searchPatientById(patientId);

    if (patient == NULL) {
        printf("未找到患者。\n");
        return;
    }
    if (strcmp(patient->typeOfPatient, "住院") != 0) {
        printf("当前无住院/床位信息。\n");
        return;
    }
    printSinglePatient(patient);
}

// 患者端查询科室和医生的菜单。
static void patientDoctorDepartmentQueryMenu() {
    while (1) {
        printf("\n===== 科室和医生查询 =====\n");
        printf("1. 查看科室\n");
        printf("2. 查看全部医生\n");
        printf("3. 按科室查看医生\n");
        printf("0. 返回\n");
        switch (readChoice("请选择: ")) {
        case 1: printAllDepartments(); break;
        case 2: printAllDoctors(); break;
        case 3: {
            int deptId;
            if (readIntPrompt("科室ID: ", &deptId)) printDoctorsByDepartment(deptId);
            break;
        }
        case 0: return;
        default: printf("无效输入。\n");
        }
    }
}

// 患者登录后的服务菜单。
static void patientWorkbenchMenu(const Session* session) {
    const char* patientId;

    // 患者入口同理，只能查看和操作自己身份证号对应的数据。
    if (session == NULL || session->role != ROLE_PATIENT) return;
    patientId = session->boundId;
    while (1) {
        printf("\n===== 患者服务 =====\n");
        printf("1. 我的基本信息\n");
        printf("2. 现场挂号\n");
        printf("3. 预约挂号\n");
        printf("4. 我的挂号记录\n");
        printf("5. 我的病历\n");
        printf("6. 我的用药记录\n");
        printf("7. 我的住院/床位信息\n");
        printf("8. 查询科室和医生\n");
        printf("0. 退出登录\n");

        switch (readChoice("请选择: ")) {
        case 1:
            printSinglePatient(searchPatientById(patientId));
            break;
        case 2:
            runPatientOnsiteRegistration(patientId);
            break;
        case 3:
            runPatientAppointmentRegistration(patientId);
            break;
        case 4:
            printPatientRegistrationRecords(patientId);
            break;
        case 5:
            printMedicalRecordsByPatientId(patientId);
            break;
        case 6:
            printPharmacyLogsByPatientId(patientId);
            break;
        case 7:
            printPatientWardBedInfo(patientId);
            break;
        case 8:
            patientDoctorDepartmentQueryMenu();
            break;
        case 0:
            saveAllData();
            return;
        default:
            printf("无效输入。\n");
        }
    }
}

// 把角色枚举转换成登录标题里的中文身份名。
static const char* loginRoleName(UserRole role) {
    if (role == ROLE_PATIENT) return "患者";
    if (role == ROLE_DOCTOR) return "医生";
    if (role == ROLE_ADMIN) return "管理员";
    return "未知身份";
}

// 通用登录流程，并根据角色进入对应工作台。
static void runLogin(UserRole expectedRole) {
    char account[AUTH_ACCOUNT_LEN];
    char password[AUTH_PASSWORD_LEN];
    Session session;

    printf("\n===== %s登录 =====\n", loginRoleName(expectedRole));
    if (!readTokenPrompt("账号: ", account, sizeof(account))) return;
    if (!readTokenPrompt("密码: ", password, sizeof(password))) return;
    if (!authLogin(account, password, &session)) {
        printf("账号或密码错误，或账号已停用。\n");
        return;
    }
    if (session.role != expectedRole) {
        printf("当前入口只允许%s账号登录。\n", loginRoleName(expectedRole));
        return;
    }

    setChangeLogOperator(session.account);

    if (session.role == ROLE_ADMIN) adminMenu();
    else if (session.role == ROLE_DOCTOR) doctorWorkbenchMenu(&session);
    else if (session.role == ROLE_PATIENT) patientWorkbenchMenu(&session);
    else printf("未知角色，无法进入系统。\n");

    setChangeLogOperator("system");
}

// 患者入口菜单，提供登录和注册。
static void patientEntryMenu() {
    while (1) {
        printf("\n===== 患者入口 =====\n");
        printf("1. 登录\n");
        printf("2. 注册\n");
        printf("0. 返回\n");

        switch (readChoice("请输入功能编号: ")) {
        case 1: runLogin(ROLE_PATIENT); break;
        case 2: runPatientAccountRegister(); break;
        case 0: return;
        default:
            printf("无效输入。\n");
        }
    }
}

// 程序入口，负责初始化数据并进入一级菜单。
int main() {
    setupConsoleEncoding();
    initDataFiles();
    loadAllData();
    atexit(saveAllData);

    // 程序退出时主动保存，异常关闭时也尽量由 atexit 兜底。
    while (1) {
        printf("\n===== 医院医疗管理系统 =====\n");
        printf("1. 患者\n");
        printf("2. 医生\n");
        printf("3. 管理员\n");
        printf("0. 退出\n");

        switch (readChoice("请输入功能编号: ")) {
        case 1: patientEntryMenu(); break;
        case 2: runLogin(ROLE_DOCTOR); break;
        case 3: runLogin(ROLE_ADMIN); break;
        case 0:
            saveAllData();
            printf("数据已保存，系统退出。\n");
            return 0;
        default:
            printf("无效输入。\n");
        }
    }
}
