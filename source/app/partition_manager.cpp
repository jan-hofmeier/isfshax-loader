#include "partition_manager.h"
#include "menu.h"
#include "gui.h"
#include "filesystem.h"
#include "download.h"
#include "common.h"
#include <malloc.h>
#include <cstring>
#include <coreinit/ios.h>
#include <coreinit/filesystem_fsa.h>

typedef struct __attribute__((packed)) {
    uint32_t unused;
    char device[0x280];
    char filesystem[8];
    uint32_t flags;
    uint32_t param_5;
    uint32_t param_6;
} FSAFormatRequest;

typedef struct __attribute__((packed)) {
    uint32_t unused;
    char device[0x280];
    char path[0x280];
    uint32_t flags;
    uint32_t arg_len;
} FSAMountRequest;

typedef struct __attribute__((packed)) {
    uint32_t unused;
    char path[0x280];
    uint32_t flags;
} FSAUnmountRequest;

typedef struct __attribute__((aligned(0x40))) {
    uint32_t handle;
    uint32_t command;
    union {
        uint8_t inbuf[0x520 - 8];
        FSAFormatRequest format;
        FSAMountRequest mount;
        FSAUnmountRequest unmount;
    };
    uint8_t padding[0x20]; // Align outbuf to 0x40 boundary (0x520 + 0x20 = 0x540)
    uint8_t outbuf[0x293];
} FSAIpcData;

static int32_t FSA_Format(int fsaFd, const char* device, const char* filesystem, uint32_t flags, uint32_t param_5, uint32_t param_6) {
    FSAIpcData* data = (FSAIpcData*)memalign(0x40, sizeof(FSAIpcData));
    if (!data) return -1;
    memset(data, 0, sizeof(FSAIpcData));

    data->handle = (uint32_t)fsaFd;
    data->command = 0x69;

    strncpy(data->format.device, device, sizeof(data->format.device) - 1);
    strncpy(data->format.filesystem, filesystem, sizeof(data->format.filesystem) - 1);

    data->format.flags = flags;
    data->format.param_5 = param_5;
    data->format.param_6 = param_6;

    int32_t ret = IOS_Ioctl(fsaFd, 0x69, data, 0x520, data->outbuf, 0x293);

    free(data);
    return ret;
}

static int32_t FSA_Unmount(int fsaFd, const char* path, uint32_t flags) {
    FSAIpcData* data = (FSAIpcData*)memalign(0x40, sizeof(FSAIpcData));
    if (!data) return -1;
    memset(data, 0, sizeof(FSAIpcData));

    data->handle = (uint32_t)fsaFd;
    data->command = 0x02;

    strncpy(data->unmount.path, path, sizeof(data->unmount.path) - 1);
    data->unmount.flags = flags;

    int32_t ret = IOS_Ioctl(fsaFd, 0x02, data, 0x520, data->outbuf, 0x293);

    free(data);
    return ret;
}

static int32_t FSA_Mount(int fsaFd, const char* device, const char* path, uint32_t flags, const char* arg, uint32_t arg_len) {
    FSAIpcData* data = (FSAIpcData*)memalign(0x40, sizeof(FSAIpcData));
    if (!data) return -1;
    memset(data, 0, sizeof(FSAIpcData));

    data->handle = (uint32_t)fsaFd;
    data->command = 0x01;

    strncpy(data->mount.device, device, sizeof(data->mount.device) - 1);
    strncpy(data->mount.path, path, sizeof(data->mount.path) - 1);

    data->mount.flags = flags;
    data->mount.arg_len = arg_len;

    IOSVec iov[3];
    iov[0].ptr = data;
    iov[0].len = 0x520;
    iov[1].ptr = (void*)arg;
    iov[1].len = arg_len;
    iov[2].ptr = data->outbuf;
    iov[2].len = 0x293;

    int32_t ret = IOS_Ioctlv(fsaFd, 0x01, 2, 1, iov);

    free(data);
    return ret;
}

void formatSdAndDownloadAromaMenu() {
    uint8_t choice = showDialogPrompt(L"WARNING: This will format the SD card and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No");
    if (choice != 0) return;

    WHBLogPrint("Opening /dev/fsa...");
    WHBLogFreetypeDraw();
    int fsaFd = FSAOpen();
    if (fsaFd < 0) {
        setErrorPrompt(L"Failed to open /dev/fsa!");
        showErrorPrompt(L"OK");
        return;
    }

    WHBLogPrint("Unmounting SD card...");
    WHBLogFreetypeDraw();
    int32_t status = FSA_Unmount(fsaFd, "/vol/external01", 0x80000002);
    if (status != 0) {
        WHBLogPrintf("Unmount failed (status: 0x%08X), ignoring...", status);
        WHBLogFreetypeDraw();
    }

    WHBLogPrint("Formatting SD card...");
    WHBLogFreetypeDraw();
    status = FSA_Format(fsaFd, "/dev/sdcard01", "fat", 0, 0, 0);
    if (status != 0) {
        WHBLogPrintf("Format failed (status: 0x%08X)!", status);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to format SD card!");
        showErrorPrompt(L"OK");
        FSAClose(fsaFd);
        return;
    }

    WHBLogPrint("Mounting SD card...");
    WHBLogFreetypeDraw();
    status = FSA_Mount(fsaFd, "/dev/sdcard01", "/vol/external01", 2, NULL, 0);
    if (status != 0) {
         WHBLogPrintf("Mount failed (status: 0x%08X)!", status);
         WHBLogFreetypeDraw();
         setErrorPrompt(L"Failed to mount SD card after format!");
         showErrorPrompt(L"OK");
         FSAClose(fsaFd);
         return;
    }

    WHBLogPrint("SD card formatted and mounted. Downloading Aroma...");
    WHBLogFreetypeDraw();

    if (downloadAroma("fs:/vol/external01/")) {
        showDialogPrompt(L"SD card formatted and Aroma downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }

    FSAClose(fsaFd);
}
