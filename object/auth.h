#pragma once
#ifndef AUTH_H
#define AUTH_H

#define AUTH_ACCOUNT_LEN 32
#define AUTH_PASSWORD_LEN 32
#define AUTH_BOUND_ID_LEN 32

// 三类入口共用一套登录逻辑，靠角色决定能进入哪个工作台。
typedef enum UserRole {
    ROLE_NONE = 0,
    ROLE_ADMIN = 1,
    ROLE_DOCTOR = 2,
    ROLE_PATIENT = 3
} UserRole;

typedef struct Session {
    // 登录成功后保存在内存里的会话信息，不写入文件。
    int isLoggedIn;
    UserRole role;
    char account[AUTH_ACCOUNT_LEN];
    char boundId[AUTH_BOUND_ID_LEN];
    int boundDoctorId;
} Session;

void initAuthList();
void initSession(Session* session);
int authLogin(const char* account, const char* password, Session* session);
int registerPatientAccount(const char* idCard, const char* password);
int registerDoctorAccount(const char* idCard, const char* password);
int authAccountExists(const char* account);
void loadUsersFromFile(const char* filename);
void saveUsersToFile(const char* filename);

#endif
