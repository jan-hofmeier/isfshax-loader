#include "filesystem.h"
#include "cfw.h"
#include "gui.h"
#include "menu.h"
#include "progress.h"
#include "../utils/fatfs/fatfs_devoptab.h"
#include "../utils/fatfs/ff.h"
#include "../utils/fatfs/diskio.h"

#include <dirent.h>
#include <sys/unistd.h>
#include <malloc.h>
#include <regex>

static bool systemSLCMounted = false;
static bool usbFatMounted = false;
static bool systemMLCMounted = false;
static bool systemUSBMounted = false;
static bool discMounted = false;

extern "C" const PARTITION VolToPart[FF_VOLUMES] = {
    {0, 0},     // 0: SD Card (Physical 0, Auto)
    {1, 1},     // 1: USB Partition 1 (Physical 1, Partition 1)
    {1, 2},     // 2: USB Partition 2 (Physical 1, Partition 2)
    {1, 0}      // 3: USB Auto (Physical 1, Auto)
};

// unmount wii hook
extern "C" FSClient* __wut_devoptab_fs_client;
bool unmountDefaultDevoptab() {
    // Get FS mount path using current directory
    char mountPath[PATH_MAX];
    getcwd(mountPath, sizeof(mountPath));
    memmove(mountPath, mountPath + 3, PATH_MAX - 3);

    FSCmdBlock cmd;
    FSInitCmdBlock(&cmd);
    if (FSStatus res = FSUnmount(__wut_devoptab_fs_client, &cmd, "/vol/external01", FS_ERROR_FLAG_ALL); res != FS_STATUS_OK) {
        WHBLogPrintf("Couldn't unmount default devoptab with path %s! Error = %X", mountPath, res);
        WHBLogFreetypeDraw();
        return false;
    }

    if (FSStatus res = FSDelClient(__wut_devoptab_fs_client, FS_ERROR_FLAG_ALL); res != FS_STATUS_OK) {
        WHBLogPrintf("Couldn't delete wut devoptab with path %s! Error = %X", mountPath, res);
        WHBLogFreetypeDraw();
        return false;
    }
    return true;
}


bool mountSystemDrives() {
    WHBLogPrint("Mounting system drives...");
    WHBLogFreetypeDraw();
    if (USE_LIBMOCHA()) {
        //unmountDefaultDevoptab();
        if (Mocha_MountFS("storage_slc", "/dev/slc01", "/vol/storage_slc01") == MOCHA_RESULT_SUCCESS) systemSLCMounted = true;
        // if (Mocha_MountFS("storage_mlc01", nullptr, "/vol/storage_mlc01") == MOCHA_RESULT_SUCCESS) systemMLCMounted = true;
        // if (Mocha_MountFS("storage_usb01", nullptr, "/vol/storage_usb01") == MOCHA_RESULT_SUCCESS) systemUSBMounted = true;
    }
    else {
        systemMLCMounted = true;
        systemUSBMounted = false;
    }

    if (systemSLCMounted) WHBLogPrint("Successfully mounted the SLC storage!");
    if (systemMLCMounted) WHBLogPrint("Successfully mounted the internal Wii U storage!");
    if (systemUSBMounted) WHBLogPrint("Successfully mounted the external Wii U storage!");
    WHBLogFreetypeDraw();
    return systemSLCMounted; // Require only the MLC to be mounted for this function to be successful
}

bool isSlcMounted() {
    return systemSLCMounted;
}

bool mountDisc() {
    if (USE_LIBMOCHA()) {
        if (Mocha_MountFS("storage_odd01", "/dev/odd01", "/vol/storage_odd_tickets") == MOCHA_RESULT_SUCCESS) discMounted = true;
        if (Mocha_MountFS("storage_odd02", "/dev/odd02", "/vol/storage_odd_updates") == MOCHA_RESULT_SUCCESS) discMounted = true;
        if (Mocha_MountFS("storage_odd03", "/dev/odd03", "/vol/storage_odd_content") == MOCHA_RESULT_SUCCESS) discMounted = true;
        if (Mocha_MountFS("storage_odd04", "/dev/odd04", "/vol/storage_odd_content2") == MOCHA_RESULT_SUCCESS) discMounted = true;
    }
    if (discMounted) WHBLogPrint("Successfully mounted the disc!");
    WHBLogFreetypeDraw();
    return discMounted;
}

bool unmountSystemDrives() {
    if (USE_LIBMOCHA()) {
        if (systemSLCMounted && Mocha_UnmountFS("storage_slc") == MOCHA_RESULT_SUCCESS) systemSLCMounted = false;
        if (systemMLCMounted && Mocha_UnmountFS("storage_mlc01") == MOCHA_RESULT_SUCCESS) systemMLCMounted = false;
        if (systemUSBMounted && Mocha_UnmountFS("storage_usb01") == MOCHA_RESULT_SUCCESS) systemUSBMounted = false;
    }
    else {
        systemMLCMounted = false;
        systemUSBMounted = false;
    }
    return (!systemMLCMounted && !systemUSBMounted);
}

bool unmountDisc() {
    if (!discMounted) return false;
    if (USE_LIBMOCHA()) {
        if (Mocha_UnmountFS("storage_odd01") == MOCHA_RESULT_SUCCESS) discMounted = false;
        if (Mocha_UnmountFS("storage_odd02") == MOCHA_RESULT_SUCCESS) discMounted = false;
        if (Mocha_UnmountFS("storage_odd03") == MOCHA_RESULT_SUCCESS) discMounted = false;
        if (Mocha_UnmountFS("storage_odd04") == MOCHA_RESULT_SUCCESS) discMounted = false;
    }
    return !discMounted;
}

bool isDiscMounted() {
    return discMounted;
}

bool mountUsbFat() {
    if (usbFatMounted) return true;
    if (fatfs_mount("usb", 1)) {
        usbFatMounted = true;
        return true;
    }
    return false;
}

void unmountUsbFat() {
    if (usbFatMounted) {
        fatfs_unmount("usb");
        usbFatMounted = false;
    }
}

static void write32LE(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

bool partitionUsb(int fatPercent) {
    unmountUsbFat();
    if (disk_initialize((void*)1) != 0) {
        setErrorPrompt(L"Failed to initialize USB drive!");
        return false;
    }

    uint64_t info[2];
    if (disk_ioctl((void*)1, WIIU_GET_RAW_DEVICE_INFO, info) != RES_OK) {
        setErrorPrompt(L"Failed to get USB drive info!");
        return false;
    }
    uint64_t totalSectors = info[0];
    uint32_t sectorSize = ((uint32_t*)info)[2];

    uint64_t driveSizeInBytes = totalSectors * sectorSize;
    uint64_t oneGB = 1024ULL * 1024 * 1024;
    uint64_t oneGBInSectors = oneGB / sectorSize;
    uint32_t maxSectors = 0xFFFFFFFF;

    uint32_t alignSectors;
    if (driveSizeInBytes >= oneGB) {
        alignSectors = (16 * 1024 * 1024) / sectorSize;
    } else {
        alignSectors = (1 * 1024 * 1024) / sectorSize;
    }

    // Partition 1
    uint64_t p1_start = alignSectors;
    uint64_t p1_size = (totalSectors * fatPercent) / 100;
    if (p1_size < oneGBInSectors && totalSectors > oneGBInSectors + alignSectors) {
        p1_size = oneGBInSectors;
    }
    if (p1_size > maxSectors) p1_size = maxSectors;
    if (p1_start + p1_size > totalSectors) p1_size = totalSectors - p1_start;

    // Partition 2
    uint64_t p2_start = 0;
    uint64_t p2_size = 0;
    if (fatPercent < 100) {
        p2_start = (p1_start + p1_size + alignSectors - 1) / alignSectors * alignSectors;
        if (p2_start < totalSectors && p2_start <= maxSectors) {
             p2_size = totalSectors - p2_start;
             if (p2_size > maxSectors) p2_size = maxSectors;
        } else {
            p2_start = 0;
            p2_size = 0;
        }
    }

    // Prepare MBR
    uint8_t* mbr = (uint8_t*)memalign(0x40, sectorSize);
    if (!mbr) return false;
    memset(mbr, 0, sectorSize);
    uint8_t* pte1 = &mbr[446];
    uint8_t* pte2 = &mbr[446 + 16];

    // Partition 1 (FAT32)
    pte1[4] = 0x0C; // FAT32 with LBA
    write32LE(&pte1[8], (uint32_t)p1_start);
    write32LE(&pte1[12], (uint32_t)p1_size);

    // Partition 2 (NTFS)
    if (p2_size > 0) {
        pte2[4] = 0x07; // NTFS/exFAT
        write32LE(&pte2[8], (uint32_t)p2_start);
        write32LE(&pte2[12], (uint32_t)p2_size);
    }

    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    WHBLogPrint("Writing MBR...");
    WHBLogFreetypeDraw();
    if (disk_write((void*)1, mbr, 0, 1) != RES_OK) {
        free(mbr);
        setErrorPrompt(L"Failed to write MBR!");
        return false;
    }
    free(mbr);

    // Now format the first partition
    return formatUsbFat(true);
}

bool formatUsbFat(bool partitionTableExists) {
    unmountUsbFat(); // Make sure it's not mounted via FatFS devoptab

    // Initialize the drive
    if (disk_initialize((void*)1) != 0) {
        setErrorPrompt(L"Failed to initialize USB drive!");
        return false;
    }

    BYTE* work = (BYTE*)memalign(0x40, FF_MAX_SS);
    if (!work) return false;

    WHBLogPrint("Formatting FAT32 partition...");
    WHBLogFreetypeDraw();

    // Using logical drive "1:", which maps to Physical 1, Partition 1 via VolToPart
    MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
    FRESULT res = f_mkfs("1:", &opt, work, FF_MAX_SS);
    if (res != FR_OK) {
        WHBLogPrintf("f_mkfs failed: %d", res);
        WHBLogFreetypeDraw();
        free(work);
        setErrorPrompt(L"Failed to format FAT32 partition!");
        return false;
    }
    free(work);

    // Mount it briefly to set the label
    if (mountUsbFat()) {
        FATFS* fs = fatfs_get_fs("usb");
        if (fs) {
            f_setlabel(fs, "aroma");
        }
    }

    WHBLogPrint("USB drive formatted successfully!");
    WHBLogFreetypeDraw();
    return true;
}

bool testStorage(TITLE_LOCATION location) {
    if (location == TITLE_LOCATION::NAND) return dirExist(convertToPosixPath("/vol/storage_mlc01/usr/").c_str());
    if (location == TITLE_LOCATION::USB) return dirExist(convertToPosixPath("/vol/storage_usb01/usr/").c_str());
    //if (location == TITLE_LOCATION::Disc) return dirExist("storage_odd01:/usr/");
    return false;
}

bool isDiscInserted() {
    return false;
}

// Filesystem Helper Functions

// Wii U libraries will give us paths that use /vol/storage_mlc01/file.txt, but libiosuhax uses the mounted drive paths like storage_mlc01:/file.txt (and wut uses fs:/vol/sys_mlc01/file.txt)
// Converts a Wii U device path to a posix path
std::string convertToPosixPath(const char* volPath) {
    std::string posixPath;

    // volPath has to start with /vol/
    if (strncmp("/vol/", volPath, 5) != 0) return "";

    if (USE_LIBMOCHA()) {
        // Get and append the mount path
        const char *drivePathEnd = strchr(volPath + 5, '/');
        if (drivePathEnd == nullptr) {
            // Return just the mount path
            posixPath.append(volPath + 5);
            posixPath.append(":");
        } else {
            // Return mount path + the path after it
            posixPath.append(volPath, 5, drivePathEnd - (volPath + 5));
            posixPath.append(":/");
            posixPath.append(drivePathEnd + 1);
        }
        return posixPath;
    }
    else return std::string("fs:") + volPath;
}


struct stat existStat;
const std::regex rootFilesystem(R"(^fs:\/vol\/[^\/:]+\/?$)");
bool isRoot(const char* path) {
    std::string newPath(path);
    if (newPath.size() >= 2 && newPath.rbegin()[0] == ':') return true;
    if (newPath.size() >= 3 && newPath.rbegin()[1] == ':' && newPath.rbegin()[0] == '/') return true;
    if (true/*!IS_CEMU_PRESENT()*/) {
        if (std::regex_match(newPath, rootFilesystem)) return true;
    }
    return false;
}

bool fileExist(const char* path) {
    if (isRoot(path)) return true;
    if (lstat(path, &existStat) == 0 && S_ISREG(existStat.st_mode)) return true;
    return false;
}

bool dirExist(const char* path) {
    if (isRoot(path)) return true;
    if (lstat(path, &existStat) == 0 && S_ISDIR(existStat.st_mode)) return true;
    return false;
}

bool isDirEmpty(const char* path) {
    DIR* dirHandle;
    if ((dirHandle = opendir(path)) == nullptr) return true;

    struct dirent *dirEntry;
    while((dirEntry = readdir(dirHandle)) != nullptr) {
        if ((dirEntry->d_type & DT_DIR) == DT_DIR && (strcmp(dirEntry->d_name, ".") == 0 || strcmp(dirEntry->d_name, "..") == 0)) continue;
        
        // An entry other than the root and parent directory was found
        closedir(dirHandle);
        return false;
    }
    
    closedir(dirHandle);
    return true;
}
