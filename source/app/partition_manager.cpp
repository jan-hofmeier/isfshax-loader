#include "partition_manager.h"
#include "menu.h"
#include "gui.h"
#include "filesystem.h"
#include "download.h"
#include "common.h"
#include <malloc.h>
#include <cstring>
#include <coreinit/ios.h>

typedef struct __attribute__((aligned(0x40))) {
    uint32_t handle;
    uint32_t command;
    uint8_t inbuf[0x520 - 8];
    uint8_t outbuf[0x293];
} FSAIpcData;

static int32_t FSA_Format(int fsaFd, const char* device, const char* filesystem, uint32_t flags, uint32_t param_5, uint32_t param_6) {
    FSAIpcData* data = (FSAIpcData*)memalign(0x40, sizeof(FSAIpcData));
    if (!data) return -1;
    memset(data, 0, sizeof(FSAIpcData));

    data->handle = (uint32_t)fsaFd;
    data->command = 0x69;

    strncpy((char*)data->inbuf + 4, device, 0x27F);
    strncpy((char*)data->inbuf + 0x284, filesystem, 8);

    // Manual BE conversion as in snippet
    data->inbuf[0x28C] = (uint8_t)(flags >> 24);
    data->inbuf[0x28D] = (uint8_t)(flags >> 16);
    data->inbuf[0x28E] = (uint8_t)(flags >> 8);
    data->inbuf[0x28F] = (uint8_t)flags;

    data->inbuf[0x290] = (uint8_t)(param_5 >> 24);
    data->inbuf[0x291] = (uint8_t)(param_5 >> 16);
    data->inbuf[0x292] = (uint8_t)(param_5 >> 8);
    data->inbuf[0x293] = (uint8_t)param_5;

    data->inbuf[0x294] = (uint8_t)(param_6 >> 24);
    data->inbuf[0x295] = (uint8_t)(param_6 >> 16);
    data->inbuf[0x296] = (uint8_t)(param_6 >> 8);
    data->inbuf[0x297] = (uint8_t)param_6;

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

    strncpy((char*)data->inbuf + 4, path, 0x27F);

    data->inbuf[0x284] = (uint8_t)(flags >> 24);
    data->inbuf[0x285] = (uint8_t)(flags >> 16);
    data->inbuf[0x286] = (uint8_t)(flags >> 8);
    data->inbuf[0x287] = (uint8_t)flags;

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

    strncpy((char*)data->inbuf + 4, device, 0x27F);
    strncpy((char*)data->inbuf + 0x284, path, 0x27F);

    data->inbuf[0x504] = (uint8_t)(flags >> 24);
    data->inbuf[0x505] = (uint8_t)(flags >> 16);
    data->inbuf[0x506] = (uint8_t)(flags >> 8);
    data->inbuf[0x507] = (uint8_t)flags;

    data->inbuf[0x508] = (uint8_t)(arg_len >> 24);
    data->inbuf[0x509] = (uint8_t)(arg_len >> 16);
    data->inbuf[0x50A] = (uint8_t)(arg_len >> 8);
    data->inbuf[0x50B] = (uint8_t)arg_len;

    iovec_s iov[3];
    iov[0].ptr = data;
    iov[0].length = 0x520;
    iov[1].ptr = (void*)arg;
    iov[1].length = arg_len;
    iov[2].ptr = data->outbuf;
    iov[2].length = 0x293;

    int32_t ret = IOS_Ioctlv(fsaFd, 0x01, 2, 1, iov);

    free(data);
    return ret;
}

void formatSdAndDownloadAromaMenu() {
    uint8_t choice = showDialogPrompt(L"WARNING: This will format the SD card and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No");
    if (choice != 0) return;

    WHBLogPrint("Opening /dev/fsa...");
    WHBLogFreetypeDraw();
    int fsaFd = IOS_Open("/dev/fsa", (IOSOpenMode)0);
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
        IOS_Close(fsaFd);
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
         IOS_Close(fsaFd);
         return;
    }

    WHBLogPrint("SD card formatted and mounted. Downloading Aroma...");
    WHBLogFreetypeDraw();

    if (downloadAroma("fs:/vol/external01/")) {
        showDialogPrompt(L"SD card formatted and Aroma downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }

    IOS_Close(fsaFd);
}
