#pragma once
#include <string>
#include "ff.h"

bool fatfs_mount(const std::string& name, int pdrv);
bool fatfs_unmount(const std::string& name);
FATFS* fatfs_get_fs(const std::string& name);
