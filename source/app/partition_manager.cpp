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
#include <mocha/mocha.h>
#include <whb/sdcard.h>

typedef struct __attribute__((packed)) {
    uint32_t unused;
    char device[0x280];
    char filesystem[8];
    uint32_t flags;
    uint32_t param_5;
    uint32_t param_6;
} FSAFormatRequest;

typedef struct __attribute__((aligned(0x40))) {
    union {
        uint8_t inbuf[0x520];
        FSAFormatRequest format;
    };
    uint8_t outbuf[0x293];
    uint8_t padding[0x30];
    uint32_t handle;
    uint32_t command;
    uint8_t unknown[0x3d];
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

    // set partition start to 16MiB
    Mocha_IOSUKernelWrite32(0x1078e4a4, 0xe3a05902);
    // skip cylinder alignment
    Mocha_IOSUKernelWrite32(0x1078e4fc, 0xe1530003);

    // Remove 32GB check
    //Mocha_IOSUKernelWrite32(0x1078e354, 0xe1540004);

    // Return something for big cards
    Mocha_IOSUKernelWrite32(0x1078de88, 0xe3a00000); // mov r0, #0

    // Increase cluster size for >32GB
    Mocha_IOSUKernelWrite32(0x1078e35c, 0xe3a07080); // mov r7, #128
    Mocha_IOSUKernelWrite32(0x1078e360, 0xE5CD7270); // strb r7, [sp, #local_3c.sectors_per_cluster]
    Mocha_IOSUKernelWrite32(0x1078e364, 0xEA000071); // b 0x1078e530

    Mocha_IOSUKernelWrite32(0x1078e550, 0xe3a00000); // mov r0, #0 - patch out memset
    //Mocha_IOSUKernelWrite32(0x1078e55c, 0xe3a00000); // mov r0, #0 - patch out  FAT32_GetGeometry

    Mocha_IOSUKernelWrite32(0x1078e6c8, 0xe3a00000); // mov r0, #0 - patch out memset
    Mocha_IOSUKernelWrite32(0x1078e6d4, 0xe3a00000); // mov r0, #0 - patch out  FAT32_GetGeometry

    Mocha_IOSUKernelWrite32(0x1078ee74, 12345678);

    WHBLogPrint("Opening /dev/fsa...");
    WHBLogFreetypeDraw();
    FSAClientHandle fsaHandle = FSAAddClient(NULL);
    if (fsaHandle < 0) {
        WHBLogPrintf("Failed to open /dev/fsa! Status: 0x%08X", fsaHandle);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to open /dev/fsa!");
        showErrorPrompt(L"OK");
        return;
    }
    int status;
    // int status = Mocha_UnlockFSClientEx(fsaHandle);
    // WHBLogPrintf("Unlocking FSClientEX Status: -0x%08X", -status);

    WHBLogPrint("Unmounting SD card...");
    WHBLogFreetypeDraw();
    status = WHBUnmountSdCard(); //FSAUnmount(fsaHandle, "/vol/external01", (FSAUnmountFlags) 0x80000002);
    if (status != 1) {
        WHBLogPrintf("Unmount failed (status: %d), ignoring...", status);
        WHBLogFreetypeDraw();
    }

    WHBLogPrint("Formatting SD card...");
    WHBLogFreetypeDraw();
    status = (FSStatus)FSA_Format(fsaHandle, "/dev/sdcard01", "fat", 0, 0, 0);
    if (status != FS_STATUS_OK) {
        WHBLogPrintf("Format failed (status: -0x%08X)!\n", -status);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to format SD card!");
        showErrorPrompt(L"OK");
        FSADelClient(fsaHandle);
        return;
    }

    WHBLogPrint("Mounting SD card...");
    WHBLogFreetypeDraw();
    status = WHBMountSdCard(); //FSAMount(fsaHandle, "/dev/sdcard01", "/vol/external01", (FSAMountFlags)2, NULL, 0);
    if (status != 1) {
        WHBLogPrintf("Mount failed (status: %d)!", status);
        WHBLogFreetypeDraw();
        setErrorPrompt(L"Failed to mount SD card after format!");
        showErrorPrompt(L"OK");
        FSADelClient(fsaHandle);
        return;
    }

    FSADelClient(fsaHandle);

    WHBLogPrint("SD card formatted and mounted. Downloading Aroma...");
    WHBLogFreetypeDraw();

    if (downloadAroma("fs:/vol/external01/")) {
        showDialogPrompt(L"SD card formatted and Aroma downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }
}
