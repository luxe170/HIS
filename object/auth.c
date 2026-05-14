#define _CRT_SECURE_NO_WARNINGS
#include "auth.h"
#include "doctor.h"
#include "patient.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 登录模块只保存账号、密码和角色绑定关系。
 * 医生/患者的详细资料仍然放在各自模块里，登录后通过 boundId 找回业务身份。
 */

typedef struct AuthUser {
    char account[AUTH_ACCOUNT_LEN];
    char password[AUTH_PASSWORD_LEN];
    UserRole role;
    char boundId[AUTH_BOUND_ID_LEN];
    int status;
    struct AuthUser* next;
} AuthUser;

static AuthUser* authUserListHead = NULL;

// 把角色枚举转成文件里保存的英文标记。
static const char* roleToText(UserRole role) {
    if (role == ROLE_DOCTOR) return "DOCTOR";
    if (role == ROLE_PATIENT) return "PATIENT";
    return "NONE";
}

// 把文件里的角色标记还原成程序内部枚举。
static UserRole roleFromText(const char* text) {
    if (text == NULL) return ROLE_NONE;
    if (strcmp(text, "DOCTOR") == 0) return ROLE_DOCTOR;
    if (strcmp(text, "PATIENT") == 0) return ROLE_PATIENT;
    return ROLE_NONE;
}

// 检查账号、密码等字段是否能安全写入分隔文本。
static int isValidAuthText(const char* text) {
    return text != NULL && text[0] != '\0' && strchr(text, '|') == NULL;
}

// 把一个账号节点追加到账号链表尾部。
static void addAuthUser(AuthUser* user) {
    AuthUser* current;

    if (user == NULL) return;
    user->next = NULL;
    if (authUserListHead == NULL) {
        authUserListHead = user;
        return;
    }
    current = authUserListHead;
    while (current->next != NULL) current = current->next;
    current->next = user;
}

// 按账号名查找用户节点。
static AuthUser* searchAuthUser(const char* account) {
    AuthUser* current = authUserListHead;

    if (account == NULL || account[0] == '\0') return NULL;
    while (current != NULL) {
        if (strcmp(current->account, account) == 0) return current;
        current = current->next;
    }
    return NULL;
}

// 清空账号链表，重新读文件前会先调用。
void initAuthList() {
    AuthUser* current = authUserListHead;

    while (current != NULL) {
        AuthUser* next = current->next;
        free(current);
        current = next;
    }
    authUserListHead = NULL;
}

// 初始化登录会话，默认是不登录状态。
void initSession(Session* session) {
    if (session == NULL) return;
    memset(session, 0, sizeof(Session));
    session->role = ROLE_NONE;
    session->boundDoctorId = -1;
}

// 判断账号是否已经存在，内置管理员也算已占用。
int authAccountExists(const char* account) {
    if (account == NULL) return 0;
    // administer 是内置管理员账号，文件里不再重复保存。
    if (strcmp(account, "administer") == 0) return 1;
    return searchAuthUser(account) != NULL;
}

// 校验账号密码并写入本次登录的会话信息。
int authLogin(const char* account, const char* password, Session* session) {
    AuthUser* user;

    if (session == NULL) return 0;
    initSession(session);
    if (!isValidAuthText(account) || password == NULL) return 0;

    // 管理员账号固定，方便课堂演示和初次进入系统。
    if (strcmp(account, "administer") == 0) {
        if (strcmp(password, "1234") != 0) return 0;
        session->isLoggedIn = 1;
        session->role = ROLE_ADMIN;
        copyText(session->account, AUTH_ACCOUNT_LEN, account);
        copyText(session->boundId, AUTH_BOUND_ID_LEN, "-1");
        session->boundDoctorId = -1;
        return 1;
    }

    user = searchAuthUser(account);
    if (user == NULL || user->status != 1) return 0;
    if (strcmp(user->password, password) != 0) return 0;

    session->isLoggedIn = 1;
    session->role = user->role;
    copyText(session->account, AUTH_ACCOUNT_LEN, user->account);
    copyText(session->boundId, AUTH_BOUND_ID_LEN, user->boundId);
    session->boundDoctorId = -1;
    if (user->role == ROLE_DOCTOR) parseIntField(user->boundId, &session->boundDoctorId);
    return 1;
}

// 注册通用账号，患者和医生注册都走这里。
static int registerUser(const char* account, const char* password, UserRole role, const char* boundId) {
    AuthUser* user;

    // 统一注册入口：患者账号绑定身份证号，医生账号绑定医生 ID。
    if (!isValidAuthText(account) || !isValidAuthText(password) || !isValidAuthText(boundId)) return -1;
    if (strcmp(account, "administer") == 0 || searchAuthUser(account) != NULL) return -3;

    user = (AuthUser*)malloc(sizeof(AuthUser));
    if (user == NULL) return -4;
    memset(user, 0, sizeof(AuthUser));
    copyText(user->account, AUTH_ACCOUNT_LEN, account);
    copyText(user->password, AUTH_PASSWORD_LEN, password);
    user->role = role;
    copyText(user->boundId, AUTH_BOUND_ID_LEN, boundId);
    user->status = 1;
    addAuthUser(user);
    return 1;
}

// 用患者身份证注册患者账号。
int registerPatientAccount(const char* idCard, const char* password) {
    if (searchPatientById(idCard) == NULL) return -2;
    return registerUser(idCard, password, ROLE_PATIENT, idCard);
}

// 用医生身份证注册医生账号，并绑定到医生 ID。
int registerDoctorAccount(const char* idCard, const char* password) {
    Doctor* doctor = searchDoctorByIdCard(idCard);
    char boundId[AUTH_BOUND_ID_LEN];

    if (doctor == NULL) return -2;
    snprintf(boundId, sizeof(boundId), "%d", doctor->doctorId);
    return registerUser(idCard, password, ROLE_DOCTOR, boundId);
}

// 把账号链表保存到 users.txt。
void saveUsersToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    AuthUser* current = authUserListHead;

    if (!file) return;
    while (current != NULL) {
        fprintf(file, "%s|%s|%s|%s|%d\n",
            current->account, current->password, roleToText(current->role),
            current->boundId, current->status);
        current = current->next;
    }
    fclose(file);
}

// 解析 users.txt 中的一行账号数据。
static int parseUserLine(char* line, AuthUser* user) {
    char* fields[5];
    int status;
    UserRole role;

    if (splitFields(line, fields, 5) != 5) return 0;
    role = roleFromText(fields[2]);
    if (role == ROLE_NONE) return 0;
    if (!parseIntField(fields[4], &status)) return 0;
    copyText(user->account, AUTH_ACCOUNT_LEN, fields[0]);
    copyText(user->password, AUTH_PASSWORD_LEN, fields[1]);
    user->role = role;
    copyText(user->boundId, AUTH_BOUND_ID_LEN, fields[3]);
    user->status = status;
    return 1;
}

// 从文件加载账号数据，格式不对的行会跳过。
void loadUsersFromFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    char line[256];
    int lineNo = 0;

    initAuthList();
    if (!file) return;

    while (fgets(line, sizeof(line), file) != NULL) {
        AuthUser* user;
        lineNo++;
        normalizeTextEncoding(line, sizeof(line));
        trimLine(line);
        if (line[0] == '\0') continue;

        user = (AuthUser*)malloc(sizeof(AuthUser));
        if (user == NULL) break;
        memset(user, 0, sizeof(AuthUser));
        if (!parseUserLine(line, user) || strcmp(user->account, "administer") == 0) {
            printf("跳过账号数据第 %d 行：格式错误。\n", lineNo);
            free(user);
            continue;
        }
        if (searchAuthUser(user->account) != NULL) {
            free(user);
            continue;
        }
        addAuthUser(user);
    }
    fclose(file);
}
