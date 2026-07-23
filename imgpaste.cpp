#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus")

using namespace Gdiplus;

struct AppSettings {
    char customOutputDirectory[MAX_PATH];
    char imageViewerDirectory[MAX_PATH];
    bool openImage;
};

static void trim_quotes_and_whitespace(char* str) {
    if (!str || *str == '\0') return;

    // Remove trailing newlines, carriage returns, spaces, and quotes
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '"' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[--len] = '\0';
    }

    // Remove leading spaces, tabs, and quotes
    char* start = str;
    while (*start == ' ' || *start == '\t' || *start == '"') {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

static bool directory_exists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

static bool file_exists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

static void load_settings(AppSettings* settings) {
    // Set defaults
    settings->customOutputDirectory[0] = '\0';
    settings->imageViewerDirectory[0] = '\0';
    settings->openImage = false;

    // Look for any file ending in .img.config in the current directory
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("*.img.config", &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return; // No config file found, stick to defaults
    }

    FILE* f = fopen(findData.cFileName, "r");
    FindClose(hFind);
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Strip out any trailing newlines or carriage returns
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "CustomImageOutputDirectory=", 27) == 0) {
            char* val = line + 27;
            trim_quotes_and_whitespace(val);
            strncpy(settings->customOutputDirectory, val, MAX_PATH - 1);
        }
        else if (strncmp(line, "ImageViewerDirectory=", 21) == 0) {
            char* val = line + 21;
            trim_quotes_and_whitespace(val);
            strncpy(settings->imageViewerDirectory, val, MAX_PATH - 1);
            settings->openImage = true; // Flag that we want to open it
        }
    }
    fclose(f);

    // --- Validate Settings ---
    bool hasError = false;

    if (settings->customOutputDirectory[0] != '\0') {
        if (!directory_exists(settings->customOutputDirectory)) {
            fprintf(stderr, "Configuration error in .img.config:\nCustomImageOutputDirectory path does not exist or is invalid:\n  \"%s\"\n", settings->customOutputDirectory);
            hasError = true; // Mark that we found an error, but keep checking others
        }
    }

    if (settings->openImage && settings->imageViewerDirectory[0] != '\0') {
        if (_stricmp(settings->imageViewerDirectory, "Default") != 0) {
            if (!file_exists(settings->imageViewerDirectory)) {
                fprintf(stderr, "\n");
                fprintf(stderr, "Configuration error in .img.config:\nImageViewerDirectory program path does not exist or is invalid:\n  \"%s\"\n", settings->imageViewerDirectory);
                hasError = true; // Mark that we found an error
            }
        }
    }

    // If any of the settings were invalid, exit now before doing any clipboard stuff
    if (hasError) {
        exit(1);
    }
}

static void print_failed_and_exit(void) {
    fputs("pasting image from clipboard failed\n", stderr);
    fputs("\n", stderr);
    fputs("\n", stderr);
    fputs("tip: to configure this command line program, create a \"settings.img.config\" file. note that it doesn't matter what name you give to the file, as long as it uses .img.config file extension\n", stderr);
    exit(1);
}

static int make_output_name(const char* customDir, const char* ext, char* out, size_t outlen) {
    char targetDir[MAX_PATH];

    // Use custom directory if provided, otherwise fallback to Temp Path
    if (customDir && customDir[0] != '\0') {
        strncpy(targetDir, customDir, MAX_PATH - 1);
        targetDir[MAX_PATH - 1] = '\0';
    }
    else {
        if (!GetTempPathA(MAX_PATH, targetDir)) return 0;
    }

    char tmpFile[MAX_PATH];
    if (!GetTempFileNameA(targetDir, "cb", 0, tmpFile)) return 0;

    char newName[MAX_PATH];
    strncpy(newName, tmpFile, MAX_PATH - 1);
    newName[MAX_PATH - 1] = '\0';

    char* dot = strrchr(newName, '.');
    if (!dot) return 0;
    snprintf(dot, MAX_PATH - (dot - newName), "%s", ext);

    MoveFileA(tmpFile, newName);
    strncpy(out, newName, outlen - 1);
    out[outlen - 1] = '\0';
    return 1;
}

static CLSID GetEncoderClsid(const WCHAR* format) {
    CLSID clsid = { 0 };
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return clsid;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (!pImageCodecInfo) return clsid;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0 ||
            wcscmp(pImageCodecInfo[j].FormatDescription, format) == 0) {
            clsid = pImageCodecInfo[j].Clsid;
            break;
        }
    }
    free(pImageCodecInfo);
    return clsid;
}

static void to_wide(const char* in, WCHAR* out, int outlen) {
    MultiByteToWideChar(CP_ACP, 0, in, -1, out, outlen);
}

int main(void) {
    // --- NEW CODE: Load our settings ---
    AppSettings settings;
    load_settings(&settings);
    // -----------------------------------

    if (!OpenClipboard(NULL)) {
        print_failed_and_exit();
    }


    UINT pngFmt = RegisterClipboardFormatA("PNG");
    HGLOBAL hdata = NULL;
    char outpath[MAX_PATH] = { 0 };
    int wrote = 0;

    if (pngFmt && IsClipboardFormatAvailable(pngFmt)) {
        hdata = GetClipboardData(pngFmt);
        if (!hdata) { CloseClipboard(); print_failed_and_exit(); }
        void* ptr = GlobalLock(hdata);
        if (!ptr) { GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }
        SIZE_T sz = GlobalSize(hdata);
        if (sz == 0) { GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }

        if (!make_output_name(settings.customOutputDirectory, ".png", outpath, sizeof(outpath))) {
            GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit();
        }
        FILE* f = fopen(outpath, "wb");
        if (!f) { GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }
        if (fwrite(ptr, 1, sz, f) != sz) { fclose(f); GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }
        fclose(f);
        GlobalUnlock(hdata);
        wrote = 1;
    }
    else {
        HBITMAP hBitmap = NULL;

        if (IsClipboardFormatAvailable(CF_DIB)) {
            hdata = GetClipboardData(CF_DIB);
            if (!hdata) { CloseClipboard(); print_failed_and_exit(); }
            void* ptr = GlobalLock(hdata);
            if (!ptr) { GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }

            BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)ptr;
            if (!bih) { GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }

            DWORD colors = 0;
            if (bih->biClrUsed) colors = bih->biClrUsed;
            else {
                if (bih->biBitCount <= 8) colors = 1u << bih->biBitCount;
                else colors = 0;
            }
            DWORD headerSize = sizeof(BITMAPINFOHEADER) + colors * sizeof(RGBQUAD);
            BYTE* bits = (BYTE*)ptr + headerSize;

            HDC hdc = GetDC(NULL);
            if (!hdc) { GlobalUnlock(hdata); CloseClipboard(); print_failed_and_exit(); }

            HBITMAP hbm = CreateDIBitmap(hdc, bih, CBM_INIT, bits, (BITMAPINFO*)bih, DIB_RGB_COLORS);
            ReleaseDC(NULL, hdc);

            GlobalUnlock(hdata);
            if (!hbm) { CloseClipboard(); print_failed_and_exit(); }
            hBitmap = hbm;
        }
        else if (IsClipboardFormatAvailable(CF_BITMAP))
        {
            HBITMAP hbmp = (HBITMAP)GetClipboardData(CF_BITMAP);
            if (!hbmp)
            {
                CloseClipboard(); print_failed_and_exit();
            }
            hBitmap = hbmp;
        }
        else {
            CloseClipboard();
            print_failed_and_exit();
        }

        GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
            CloseClipboard();
            print_failed_and_exit();
        }

        Bitmap* gdiBmp = Bitmap::FromHBITMAP(hBitmap, NULL);
        if (!gdiBmp) {
            GdiplusShutdown(gdiplusToken);
            CloseClipboard();
            print_failed_and_exit();
        }

        if (!make_output_name(settings.customOutputDirectory, ".png", outpath, sizeof(outpath))) {
            delete gdiBmp;
            GdiplusShutdown(gdiplusToken);
            CloseClipboard();
            print_failed_and_exit();
        }

        WCHAR wpath[MAX_PATH];
        to_wide(outpath, wpath, MAX_PATH);

        CLSID pngClsid;
        UINT numEnc = 0, sizeEnc = 0;
        GetImageEncodersSize(&numEnc, &sizeEnc);
        if (sizeEnc == 0) {
            delete gdiBmp;
            GdiplusShutdown(gdiplusToken);
            CloseClipboard();
            print_failed_and_exit();
        }
        ImageCodecInfo* pCodecInfo = (ImageCodecInfo*)malloc(sizeEnc);
        if (!pCodecInfo) {
            delete gdiBmp;
            GdiplusShutdown(gdiplusToken);
            CloseClipboard();
            print_failed_and_exit();
        }
        GetImageEncoders(numEnc, sizeEnc, pCodecInfo);
        bool found = false;
        for (UINT i = 0; i < numEnc; ++i) {
            if (wcscmp(pCodecInfo[i].MimeType, L"image/png") == 0) {
                pngClsid = pCodecInfo[i].Clsid;
                found = true;
                break;
            }
        }
        free(pCodecInfo);
        if (!found) {
            delete gdiBmp;
            GdiplusShutdown(gdiplusToken);
            CloseClipboard();
            print_failed_and_exit();
        }

        Status s = gdiBmp->Save(wpath, &pngClsid, NULL);
        delete gdiBmp;
        GdiplusShutdown(gdiplusToken);
        if (s != Ok) {
            DeleteFileA(outpath);
            CloseClipboard();
            print_failed_and_exit();
        }

        wrote = 1;
    }

    CloseClipboard();

    if (wrote) {
        printf("%s\n", outpath);
        fflush(stdout);

        // --- NEW CODE: Open the image if requested ---
        if (settings.openImage) {
            if (_stricmp(settings.imageViewerDirectory, "Default") == 0) {
                // Equivalent to `start image.png`
                ShellExecuteA(NULL, "open", outpath, NULL, NULL, SW_SHOWNORMAL);
            }
            else {
                // Open with a custom 3rd party program.
                // We wrap the image path in quotes safely in case the path has spaces.
                char args[MAX_PATH + 2];
                snprintf(args, sizeof(args), "\"%s\"", outpath);
                ShellExecuteA(NULL, "open", settings.imageViewerDirectory, args, NULL, SW_SHOWNORMAL);
            }
        }
        // ---------------------------------------------

        fputs("\n", stderr);
        fputs("\n", stderr);
        fputs("tip: to configure this command line program, create a \"settings.img.config\" file. note that it doesn't matter what name you give to the file, as long as it uses .img.config file extension\n", stderr);

        return 0;
    }
    else {
        print_failed_and_exit();
    }
    return 0;
}
