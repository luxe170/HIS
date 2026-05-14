#pragma once
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

// 程序启动、退出时只调用这三个入口，文件细节集中在 file_manager.c。
void initDataFiles();
void loadAllData();
void saveAllData();

#endif
