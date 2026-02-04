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

typedef struct __attribute__((aligned(0x40))) {
    uint32_t handle;
    uint32_t command;
    union {
        uint8_t inbuf[0x520 - 8];
        FSAFormatRequest format;
    };
    uint8_t padding[0x20]; // Align outbuf to 0x40 boundary (0x520 + 0x20 = 0x540)
    uint8_t outbuf[0x293];
} FSAIpcData;

static int32_t FSA_Format(FSAClientHandle handle, const char* device, const char* filesystem, uint32_t flags, uint32_t param_5, uint32_t param_6) {
    FSAIpcData* data = (FSAIpcData*)memalign(0x40, sizeof(FSAIpcData));
    if (!data) return -1;
    memset(data, 0, sizeof(FSAIpcData));

    data->handle = (uint32_t)handle;
    data->command = 0x69;

    strncpy(data->format.device, device, sizeof(data->format.device) - 1);
    strncpy(data->format.filesystem, filesystem, sizeof(data->format.filesystem) - 1);

    data->format.flags = flags;
    data->format.param_5 = param_5;
    data->format.param_6 = param_6;

    int32_t ret = IOS_Ioctl((int)handle, 0x69, data, 0x520, data->outbuf, 0x293);

    free(data);
    return ret;
}

void formatSdAndDownloadAromaMenu() {
    uint8_t choice = showDialogPrompt(L"WARNING: This will format the SD card and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No");
    if (choice != 0) return;

    WHBLogPrint("Opening /dev/fsa...");
    WHBLogFreetypeDraw();
    FSAClientHandle fsaHandle;
    FSStatus status = FSAOpen(&fsaHandle);
    if (status != FS_STATUS_OK) {
        WHBLogPrintf("Failed to open /dev/fsa! Status: 0x%08X", status);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to open /dev/fsa!");
        showErrorPrompt(L"OK");
        return;
    }

    WHBLogPrint("Unmounting SD card...");
    WHBLogFreetypeDraw();
    status = FSAUnmount(fsaHandle, "/vol/external01", (FSFlags)0x80000002);
    if (status != FS_STATUS_OK) {
        WHBLogPrintf("Unmount failed (status: 0x%08X), ignoring...", status);
        WHBLogFreetypeDraw();
    }

    WHBLogPrint("Formatting SD card...");
    WHBLogFreetypeDraw();
    status = (FSStatus)FSA_Format(fsaHandle, "/dev/sdcard01", "fat", 0, 0, 0);
    if (status != FS_STATUS_OK) {
        WHBLogPrintf("Format failed (status: 0x%08X)!", status);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to format SD card!");
        showErrorPrompt(L"OK");
        FSAClose(fsaHandle);
        return;
    }

    WHBLogPrint("Mounting SD card...");
    WHBLogFreetypeDraw();
    status = FSAMount(fsaHandle, "/dev/sdcard01", "/vol/external01", (FSFlags)2, NULL, 0);
    if (status != FS_STATUS_OK) {
         WHBLogPrintf("Mount failed (status: 0x%08X)!", status);
         WHBLogFreetypeDraw();
         setErrorPrompt(L"Failed to mount SD card after format!");
         showErrorPrompt(L"OK");
         FSAClose(fsaHandle);
         return;
    }

    WHBLogPrint("SD card formatted and mounted. Downloading Aroma...");
    WHBLogFreetypeDraw();

    if (downloadAroma("fs:/vol/external01/")) {
        showDialogPrompt(L"SD card formatted and Aroma downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }

    FSAClose(fsaHandle);
}
