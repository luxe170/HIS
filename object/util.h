#pragma once
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

// 控制台、输入、字符串处理的公共工具，减少每个菜单重复写校验代码。
void setupConsoleEncoding();
int consolePrintf(const char* format, ...);
void getCurrentTime(char* buffer, int bufferSize);
void clearInputLine();
void normalizeTextEncoding(char* buffer, int bufferSize);
void trimLine(char* text);
void copyText(char* dest, int destSize, const char* src);
int getDisplayWidth(const char* text);
void printTableBorder(const int widths[], int count);
void printTableRow(const char* cells[], const int widths[], int count);

int readLinePrompt(const char* prompt, char* buffer, int bufferSize);
int readIntPrompt(const char* prompt, int* value);
int readTokenPrompt(const char* prompt, char* buffer, int bufferSize);
int parseIntField(const char* text, int* value);
int splitFields(char* line, char* fields[], int maxFields);

#ifndef UTIL_IMPLEMENTATION
// 其他源文件里的 printf 会走 consolePrintf，中文输出在 Windows 控制台更稳。
#define printf consolePrintf
#endif

#endif
