#define _CRT_SECURE_NO_WARNINGS
#define UTIL_IMPLEMENTATION
#include "util.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#endif

/*
 * util.c 放输入、字符串和控制台编码这些“到处会用到”的小工具。
 * 业务代码尽量用这里的函数读入数据，提示和校验会更一致。
 */

// 设置控制台编码和本地化环境。
void setupConsoleEncoding() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX fontInfo;

    // Windows 控制台对中文比较挑剔，先统一成 UTF-8 输入输出。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    memset(&fontInfo, 0, sizeof(fontInfo));
    fontInfo.cbSize = sizeof(fontInfo);
    if (out != INVALID_HANDLE_VALUE && GetCurrentConsoleFontEx(out, FALSE, &fontInfo)) {
        fontInfo.dwFontSize.Y = fontInfo.dwFontSize.Y > 0 ? fontInfo.dwFontSize.Y : 16;
        wcscpy_s(fontInfo.FaceName, LF_FACESIZE, L"NSimSun");
        SetCurrentConsoleFontEx(out, FALSE, &fontInfo);
    }
#endif
    setlocale(LC_ALL, "");
}

// 统一处理中文输出，其他文件里的 printf 会映射到这里。
int consolePrintf(const char* format, ...) {
    char buffer[8192];
    va_list args;
    int len;

    va_start(args, format);
    len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len < 0) return len;
    buffer[sizeof(buffer) - 1] = '\0';

#ifdef _WIN32
    {
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        int wideLen;
        wchar_t* wide;

        if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode)) {
            wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
            if (wideLen > 0) {
                wide = (wchar_t*)malloc(sizeof(wchar_t) * wideLen);
                if (wide != NULL) {
                    DWORD written = 0;
                    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wide, wideLen);
                    WriteConsoleW(out, wide, (DWORD)wcslen(wide), &written, NULL);
                    free(wide);
                    return len;
                }
            }
        }

        wideLen = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
        if (wideLen > 0) {
            wide = (wchar_t*)malloc(sizeof(wchar_t) * wideLen);
            if (wide != NULL) {
                int mbLen;
                char* mb;

                MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wide, wideLen);
                mbLen = WideCharToMultiByte(936, 0, wide, -1, NULL, 0, NULL, NULL);
                if (mbLen > 0) {
                    mb = (char*)malloc((size_t)mbLen);
                    if (mb != NULL) {
                        WideCharToMultiByte(936, 0, wide, -1, mb, mbLen, NULL, NULL);
                        fputs(mb, stdout);
                        free(mb);
                        free(wide);
                        return len;
                    }
                }
                free(wide);
            }
        }
    }
#endif

    fputs(buffer, stdout);
    return len;
}

// 取得当前时间字符串。
void getCurrentTime(char* buffer, int bufferSize) {
    time_t nowtime;
    struct tm local;

    if (buffer == NULL || bufferSize <= 0) return;

    time(&nowtime);
    localtime_s(&local, &nowtime);
    snprintf(buffer, bufferSize, "%04d/%02d/%02d_%02d:%02d:%02d",
        local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
        local.tm_hour, local.tm_min, local.tm_sec);
}

// 清掉当前输入行剩余字符。
void clearInputLine() {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

// 兼容旧版数据文件：如果一行不是合法 UTF-8，就按 GBK 转成 UTF-8。
void normalizeTextEncoding(char* buffer, int bufferSize) {
    if (buffer == NULL || bufferSize <= 0 || buffer[0] == '\0') return;

#ifdef _WIN32
    {
        int wideLen;
        int utf8Len;
        wchar_t* wide;
        char* utf8;

        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buffer, -1, NULL, 0) > 0) return;

        wideLen = MultiByteToWideChar(936, 0, buffer, -1, NULL, 0);
        if (wideLen <= 0) return;

        wide = (wchar_t*)malloc(sizeof(wchar_t) * wideLen);
        if (wide == NULL) return;

        MultiByteToWideChar(936, 0, buffer, -1, wide, wideLen);
        utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
        if (utf8Len <= 0 || utf8Len > bufferSize) {
            free(wide);
            return;
        }

        utf8 = (char*)malloc((size_t)utf8Len);
        if (utf8 == NULL) {
            free(wide);
            return;
        }

        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Len, NULL, NULL);
        copyText(buffer, bufferSize, utf8);
        free(utf8);
        free(wide);
    }
#else
    (void)buffer;
    (void)bufferSize;
#endif
}

// 去掉字符串首尾空白和 UTF-8 BOM。
void trimLine(char* text) {
    int start = 0;
    int end;

    if (text == NULL) return;

    if ((unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        memmove(text, text + 3, strlen(text + 3) + 1);
    }

    end = (int)strlen(text) - 1;
    while (end >= 0 && (text[end] == '\n' || text[end] == '\r' || isspace((unsigned char)text[end]))) {
        text[end--] = '\0';
    }
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        ++start;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

// 安全复制字符串并保证目标以 \0 结尾。
void copyText(char* dest, int destSize, const char* src) {
    if (dest == NULL || destSize <= 0) return;
    if (src == NULL) src = "";
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

static int isWideCodePoint(unsigned int codePoint) {
    return (codePoint >= 0x1100 && codePoint <= 0x115F) ||
        (codePoint >= 0x2E80 && codePoint <= 0xA4CF) ||
        (codePoint >= 0xAC00 && codePoint <= 0xD7A3) ||
        (codePoint >= 0xF900 && codePoint <= 0xFAFF) ||
        (codePoint >= 0xFE10 && codePoint <= 0xFE6F) ||
        (codePoint >= 0xFF00 && codePoint <= 0xFFE6);
}

static unsigned int readUtf8CodePoint(const unsigned char** cursor) {
    const unsigned char* p = *cursor;
    unsigned int codePoint;

    if (*p < 0x80) {
        (*cursor)++;
        return *p;
    }
    if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        codePoint = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
        *cursor += 2;
        return codePoint;
    }
    if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        codePoint = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *cursor += 3;
        return codePoint;
    }
    if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 &&
        (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        codePoint = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
            ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        *cursor += 4;
        return codePoint;
    }

    (*cursor)++;
    return '?';
}

int getDisplayWidth(const char* text) {
    const unsigned char* cursor;
    int width = 0;

    if (text == NULL) return 0;
    cursor = (const unsigned char*)text;
    while (*cursor != '\0') {
        unsigned int codePoint = readUtf8CodePoint(&cursor);
        if (codePoint == '\t') width += 4;
        else if (codePoint < 32) width += 0;
        else width += isWideCodePoint(codePoint) ? 2 : 1;
    }
    return width;
}

void printTableBorder(const int widths[], int count) {
    int i;
    int j;

    if (widths == NULL || count <= 0) return;
    consolePrintf("+");
    for (i = 0; i < count; ++i) {
        for (j = 0; j < widths[i] + 2; ++j) consolePrintf("-");
        consolePrintf("+");
    }
    consolePrintf("\n");
}

void printTableRow(const char* cells[], const int widths[], int count) {
    int i;

    if (cells == NULL || widths == NULL || count <= 0) return;
    consolePrintf("|");
    for (i = 0; i < count; ++i) {
        const char* text = cells[i] == NULL ? "" : cells[i];
        int pad = widths[i] - getDisplayWidth(text);

        consolePrintf(" %s", text);
        while (pad-- > 0) consolePrintf(" ");
        consolePrintf(" |");
    }
    consolePrintf("\n");
}

// 把文本字段解析成整数。
int parseIntField(const char* text, int* value) {
    char* end = NULL;
    long parsed;

    if (text == NULL || value == NULL) return 0;
    while (*text != '\0' && isspace((unsigned char)*text)) ++text;
    if (*text == '\0') return 0;

    parsed = strtol(text, &end, 10);
    while (end != NULL && *end != '\0' && isspace((unsigned char)*end)) ++end;
    if (end == NULL || *end != '\0') return 0;

    *value = (int)parsed;
    return 1;
}

// 读取一整行输入并做基础清理。
int readLinePrompt(const char* prompt, char* buffer, int bufferSize) {
    if (buffer == NULL || bufferSize <= 0) return 0;
    if (prompt != NULL) {
        consolePrintf("%s", prompt);
        fflush(stdout);
    }
    if (fgets(buffer, bufferSize, stdin) == NULL) {
        buffer[0] = '\0';
        return 0;
    }
    normalizeTextEncoding(buffer, bufferSize);
    trimLine(buffer);
    return 1;
}

// 读取并校验整数输入。
int readIntPrompt(const char* prompt, int* value) {
    char buffer[128];

    if (!readLinePrompt(prompt, buffer, sizeof(buffer))) return 0;
    if (!parseIntField(buffer, value)) {
        consolePrintf("输入必须是整数。\n");
        return 0;
    }
    return 1;
}

// 读取不允许包含空格的短文本。
int readTokenPrompt(const char* prompt, char* buffer, int bufferSize) {
    char line[256];

    if (!readLinePrompt(prompt, line, sizeof(line))) return 0;
    if (line[0] == '\0') {
        consolePrintf("输入不能为空。\n");
        return 0;
    }
    if (strchr(line, ' ') != NULL || strchr(line, '\t') != NULL) {
        consolePrintf("该项不能包含空格。\n");
        return 0;
    }
    copyText(buffer, bufferSize, line);
    return 1;
}

// 按 | 分隔一行文本字段。
int splitFields(char* line, char* fields[], int maxFields) {
    int count = 0;
    char* cursor;

    if (line == NULL || fields == NULL || maxFields <= 0) return 0;
    trimLine(line);
    if (line[0] == '\0') return 0;

    // 直接在原字符串上把 | 改成 \0，少分配内存，也方便各模块解析。
    cursor = line;
    fields[count++] = cursor;
    while (*cursor != '\0') {
        if (*cursor == '|') {
            *cursor = '\0';
            if (count < maxFields) {
                fields[count++] = cursor + 1;
            }
        }
        ++cursor;
    }
    return count;
}
