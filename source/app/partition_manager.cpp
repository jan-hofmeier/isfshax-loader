#include "partition_manager.h"
#include "menu.h"
#include "navigation.h"
#include "filesystem.h"
#include "../utils/fatfs/diskio.h"
#include "gui.h"
#include "download.h"

void formatUsbAndDownloadAromaMenu() {
    uint8_t choice = showDialogPrompt(L"WARNING: This will format the USB drive's first partition and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No");
    if (choice != 0) return;

    if (!formatUsbFat(true)) {
        setErrorPrompt(L"Failed to format USB drive partition! Make sure it is partitioned.");
        showErrorPrompt(L"OK");
        return;
    }

    if (!mountUsbFat()) {
        setErrorPrompt(L"Failed to mount USB drive after formatting!");
        showErrorPrompt(L"OK");
        return;
    }

    if (downloadAroma("usb:/")) {
        showDialogPrompt(L"USB drive partition formatted and Aroma downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }

    unmountUsbFat();
}

void partitionUsbAndDownloadAromaMenu() {
    uint8_t choice = showDialogPrompt(L"WARNING: This will RE-PARTITION the USB drive and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No");
    if (choice != 0) return;

    if (disk_initialize((void*)1) != 0) {
        setErrorPrompt(L"Failed to initialize USB drive!");
        showErrorPrompt(L"OK");
        return;
    }

    uint64_t info[2];
    if (disk_ioctl((void*)1, WIIU_GET_RAW_DEVICE_INFO, info) != RES_OK) {
        setErrorPrompt(L"Failed to get USB drive info!");
        showErrorPrompt(L"OK");
        return;
    }
    uint64_t totalSectors = info[0];
    uint32_t sectorSize = ((uint32_t*)info)[2];
    double totalGB = (double)totalSectors * sectorSize / (1024.0 * 1024.0 * 1024.0);

    int fatPercent = 80;
    if (totalGB < 1.0) fatPercent = 100;

    while(true) {
        double fatGB = totalGB * fatPercent / 100.0;
        double ntfsGB = totalGB * (100 - fatPercent) / 100.0;

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Partition USB Drive");
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"FAT32: [ %d%% ] (%.2f GB)", fatPercent, fatGB);
        WHBLogFreetypePrintf(L"NTFS:  [ %d%% ] (%.2f GB)", 100 - fatPercent, ntfsGB);
        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrint(L"Use Left/Right to adjust (10% increments)");
        WHBLogFreetypePrint(L"Press A to confirm, B to cancel");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        bool confirmed = false;
        bool cancelled = false;
        while(true) {
            updateInputs();
            if (navigatedLeft() && fatPercent > 10) {
                if (totalGB >= 1.0) {
                    if (totalGB * (fatPercent - 10) / 100.0 >= 1.0) {
                        fatPercent -= 10;
                    }
                }
                break;
            }
            if (navigatedRight() && fatPercent < 100) {
                fatPercent += 10;
                break;
            }
            if (pressedOk()) {
                confirmed = true;
                break;
            }
            if (pressedBack()) {
                cancelled = true;
                break;
            }
            sleep_for(50ms);
        }
        if (confirmed) break;
        if (cancelled) return;
    }

    if (!partitionUsb(fatPercent)) {
        showErrorPrompt(L"OK");
        return;
    }

    if (!mountUsbFat()) {
        setErrorPrompt(L"Failed to mount USB drive after partitioning!");
        showErrorPrompt(L"OK");
        return;
    }

    if (showDialogPrompt(L"USB drive partitioned successfully!\nDo you want to download Aroma now?", L"Yes", L"No") == 0) {
        if (downloadAroma("usb:/")) {
            showDialogPrompt(L"Aroma downloaded successfully!", L"OK");
        } else {
            showErrorPrompt(L"OK");
        }
    }

    unmountUsbFat();
}
