#include "download.h"
#include "gui.h"
#include "filesystem.h"
#include "common.h"
#include <curl/curl.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <mocha/mocha.h>
#include "cacert_pem.h"
#include "unzip.h"
#include "ioapi.h"
#include <cstring>
#include <algorithm>
#include <new>

static size_t write_data_posix(void *ptr, size_t size, size_t nmemb, void *stream) {
    int fd = *(int*)stream;
    ssize_t written = write(fd, ptr, size * nmemb);
    if (written == -1) {
        return 0; // Signal error to curl
    }
    return written;
}

static bool downloadFile(const std::string& url, const std::string& path) {
    WHBLogFreetypePrintf(L"Downloading %S...", toWstring(url).c_str());
    WHBLogFreetypeDrawScreen();

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        WHBLogFreetypePrintf(L"Failed to initialize curl!");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        WHBLogFreetypePrintf(L"Failed to open %S for writing! Errno: %d", toWstring(path).c_str(), errno);
        WHBLogFreetypeDrawScreen();
        curl_easy_cleanup(curl_handle);
        return false;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_posix);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &fd);
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ISFShaxLoader/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    // set cert
    curl_blob blob;
    blob.data  = (void *) cacert_pem;
    blob.len   = cacert_pem_size;
    blob.flags = CURL_BLOB_COPY;
    curl_easy_setopt(curl_handle, CURLOPT_CAINFO_BLOB, &blob);

    CURLcode res = curl_easy_perform(curl_handle);
    close(fd);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        WHBLogFreetypePrintf(L"Curl failed: %S", toWstring(curl_easy_strerror(res)).c_str());
        WHBLogFreetypeDrawScreen();
        return false;
    }

    WHBLogFreetypePrintf(L"Successfully downloaded %S", toWstring(url).c_str());
    WHBLogFreetypeDrawScreen();
    return true;
}

static bool createHaxDirectories() {
    if (!isSlcMounted()) {
        WHBLogFreetypePrintf(L"Failed to mount SLC! FTP system file access enabled?");
        WHBLogFreetypeDrawScreen();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return false;
    }

    std::vector<std::string> dirs = {"/vol/storage_slc/sys/hax", "/vol/storage_slc/sys/hax/installer", "/vol/storage_slc/sys/hax/ios_plugins"};
    for(const auto& dir : dirs) {
        std::string posix_path = convertToPosixPath(dir.c_str());
        WHBLogFreetypePrintf(L"Create directory %S.", toWstring(posix_path).c_str());
        if (mkdir(posix_path.c_str(), 0755) != 0 && errno != EEXIST) {
            WHBLogFreetypePrintf(L"Failed to create directory %S. Errno: %d", toWstring(posix_path).c_str(), errno);
            WHBLogFreetypeDrawScreen();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            return false;
        }
    }
    return true;
}

bool downloadHaxFiles() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of hax files...");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!createHaxDirectories()) return false;

    // Stroopwafel
    if (!downloadFile("https://github.com/StroopwafelCFW/stroopwafel/releases/latest/download/00core.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/00core.ipx")) ||
        !downloadFile("https://github.com/isfshax/wafel_isfshax_patch/releases/latest/download/5isfshax.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5payldr.ipx")) ||
        !downloadFile("https://github.com/StroopwafelCFW/wafel_usb_partition/releases/latest/download/5upartsd.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5upartsd.ipx")) ||
        !downloadFile("https://github.com/StroopwafelCFW/wafel_payloader/releases/latest/download/5payldr.ipx", convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins/5isfshax.ipx")) ||
        // minute
        !downloadFile("https://github.com/StroopwafelCFW/minute_minute/releases/latest/download/fw_fastboot.img", convertToPosixPath("/vol/storage_slc/sys/hax/fw.img")) ||
        // ISFShax
        !downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.img")) ||
        !downloadFile("https://github.com/isfshax/isfshax/releases/latest/download/superblock.img.sha", convertToPosixPath("/vol/storage_slc/sys/hax/installer/sblock.sha")) ||
        !downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img"))) 
    {
        WHBLogFreetypePrint(L"\nDownload failed. Please check your internet connection.");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    return true;
}

bool downloadInstallerOnly() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Starting download of installer...");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!createHaxDirectories()) return false;

    if (!downloadFile("https://github.com/isfshax/isfshax_installer/releases/latest/download/ios.img", convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img")))
    {
        WHBLogFreetypePrint(L"\nDownload failed. Please check your internet connection.");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    return true;
}

// Memory-based IOAPI for minizip
typedef struct {
    const uint8_t* buffer;
    size_t size;
    size_t pos;
} mem_zip_buffer;

static void* ZCALLBACK mem_open(void* opaque, const char* filename, int mode) {
    return opaque;
}

static uLong ZCALLBACK mem_read(void* opaque, void* stream, void* buf, uLong size) {
    mem_zip_buffer* mem = (mem_zip_buffer*)stream;
    if (mem->pos + size > mem->size) {
        size = mem->size - mem->pos;
    }
    memcpy(buf, mem->buffer + mem->pos, size);
    mem->pos += size;
    return size;
}

static uLong ZCALLBACK mem_write(void* opaque, void* stream, const void* buf, uLong size) {
    return 0; // Not supported
}

static long ZCALLBACK mem_tell(void* opaque, void* stream) {
    mem_zip_buffer* mem = (mem_zip_buffer*)stream;
    return (long)mem->pos;
}

static long ZCALLBACK mem_seek(void* opaque, void* stream, uLong offset, int origin) {
    mem_zip_buffer* mem = (mem_zip_buffer*)stream;
    long long new_pos;
    switch (origin) {
        case ZLIB_FILEFUNC_SEEK_CUR:
            new_pos = (long long)mem->pos + (long)(offset);
            break;
        case ZLIB_FILEFUNC_SEEK_END:
            new_pos = (long long)mem->size + (long)(offset);
            break;
        case ZLIB_FILEFUNC_SEEK_SET:
            new_pos = (long long)offset;
            break;
        default:
            return -1;
    }
    if (new_pos < 0 || (size_t)new_pos > mem->size) return -1;
    mem->pos = (size_t)new_pos;
    return 0;
}

static int ZCALLBACK mem_close(void* opaque, void* stream) {
    return 0;
}

static int ZCALLBACK mem_error(void* opaque, void* stream) {
    return 0;
}

static void fill_memory_filefunc(zlib_filefunc_def* pzlib_filefunc_def, mem_zip_buffer* mem) {
    pzlib_filefunc_def->zopen_file = mem_open;
    pzlib_filefunc_def->zread_file = mem_read;
    pzlib_filefunc_def->zwrite_file = mem_write;
    pzlib_filefunc_def->ztell_file = mem_tell;
    pzlib_filefunc_def->zseek_file = mem_seek;
    pzlib_filefunc_def->zclose_file = mem_close;
    pzlib_filefunc_def->zerror_file = mem_error;
    pzlib_filefunc_def->opaque = mem;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    std::vector<uint8_t> *mem = (std::vector<uint8_t> *)userp;
    try {
        mem->insert(mem->end(), (uint8_t*)contents, (uint8_t*)contents + realsize);
    } catch (const std::bad_alloc&) {
        return 0;
    }
    return realsize;
}

static std::string findAromaDownloadUrl(const std::string& json) {
    size_t assets_pos = json.find("\"assets\":");
    if (assets_pos == std::string::npos) return "";

    size_t current_pos = assets_pos;
    while (true) {
        size_t name_pos = json.find("\"name\":", current_pos);
        if (name_pos == std::string::npos) break;

        size_t start_quote = json.find("\"", name_pos + 7);
        if (start_quote == std::string::npos) break;
        size_t end_quote = json.find("\"", start_quote + 1);
        if (end_quote == std::string::npos) break;
        std::string name = json.substr(start_quote + 1, end_quote - start_quote - 1);

        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

        if (lower_name.find("aroma") == 0 && lower_name.find(".zip") != std::string::npos) {
            size_t url_pos = json.find("\"browser_download_url\":", name_pos);
            if (url_pos != std::string::npos) {
                size_t u_start = json.find("\"", url_pos + 23);
                if (u_start == std::string::npos) break;
                size_t u_end = json.find("\"", u_start + 1);
                if (u_end == std::string::npos) break;
                return json.substr(u_start + 1, u_end - u_start - 1);
            }
        }
        current_pos = end_quote;
    }
    return "";
}

static bool recursive_mkdir(const std::string& path) {
    // We only want to create directories on the SD card
    std::string sd_prefix = convertToPosixPath("/vol/external01/");
    if (path.find(sd_prefix) != 0) return false;

    size_t pos = sd_prefix.length();
    // Skip trailing slash of prefix if it exists
    if (pos > 0 && sd_prefix[pos-1] == '/') pos--;

    while ((pos = path.find_first_of('/', pos + 1)) != std::string::npos) {
        std::string dir = path.substr(0, pos);
        if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }
    return true;
}

static bool extractZip(const std::vector<uint8_t>& zip_data, const std::string& dest_root) {
    mem_zip_buffer mem = { zip_data.data(), zip_data.size(), 0 };
    zlib_filefunc_def filefunc;
    fill_memory_filefunc(&filefunc, &mem);

    unzFile uf = unzOpen2(NULL, &filefunc);
    if (uf == NULL) return false;

    unz_global_info gi;
    if (unzGetGlobalInfo(uf, &gi) != UNZ_OK) {
        unzClose(uf);
        return false;
    }

    for (uLong i = 0; i < gi.number_entry; i++) {
        char filename[256];
        unz_file_info fi;
        if (unzGetCurrentFileInfo(uf, &fi, filename, sizeof(filename), NULL, 0, NULL, 0) != UNZ_OK) break;

        std::string full_path = dest_root;
        if (full_path.back() != '/' && filename[0] != '/') full_path += "/";
        full_path += filename;

        if (filename[strlen(filename) - 1] == '/') {
            recursive_mkdir(full_path);
            mkdir(full_path.c_str(), 0755);
        } else {
            recursive_mkdir(full_path);
            if (unzOpenCurrentFile(uf) == UNZ_OK) {
                int fd = open(full_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd >= 0) {
                    char buffer[8192];
                    int read;
                    while ((read = unzReadCurrentFile(uf, buffer, sizeof(buffer))) > 0) {
                        write(fd, buffer, read);
                    }
                    close(fd);
                }
                unzCloseCurrentFile(uf);
            }
        }

        if (i < gi.number_entry - 1) {
            if (unzGoToNextFile(uf) != UNZ_OK) break;
        }
    }

    unzClose(uf);
    return true;
}

bool downloadAroma() {
    WHBLogFreetypeStartScreen();
    WHBLogFreetypePrint(L"Fetching latest Aroma release info...");
    WHBLogFreetypeDrawScreen();

    std::vector<uint8_t> json_data;
    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) {
        WHBLogFreetypePrint(L"Failed to initialize curl!");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, "https://api.github.com/repos/wiiu-env/Aroma/releases/latest");
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &json_data);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ISFShaxLoader/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);

    // set cert
    curl_blob blob;
    blob.data  = (void *) cacert_pem;
    blob.len   = cacert_pem_size;
    blob.flags = CURL_BLOB_COPY;
    curl_easy_setopt(curl_handle, CURLOPT_CAINFO_BLOB, &blob);

    CURLcode res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        WHBLogFreetypePrintf(L"Failed to fetch release info: %S", toWstring(curl_easy_strerror(res)).c_str());
        WHBLogFreetypeDrawScreen();
        curl_easy_cleanup(curl_handle);
        return false;
    }

    std::string json(json_data.begin(), json_data.end());
    std::string downloadUrl = findAromaDownloadUrl(json);

    if (downloadUrl.empty()) {
        WHBLogFreetypePrint(L"Failed to find Aroma download URL in release info.");
        WHBLogFreetypeDrawScreen();
        curl_easy_cleanup(curl_handle);
        return false;
    }

    WHBLogFreetypePrintf(L"Downloading Aroma from: %S", toWstring(downloadUrl).c_str());
    WHBLogFreetypeDrawScreen();

    std::vector<uint8_t> zip_data;
    curl_easy_setopt(curl_handle, CURLOPT_URL, downloadUrl.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &zip_data);

    res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        WHBLogFreetypePrintf(L"Failed to download zip: %S", toWstring(curl_easy_strerror(res)).c_str());
        WHBLogFreetypeDrawScreen();
        return false;
    }

    WHBLogFreetypePrint(L"Extracting Aroma to SD card...");
    WHBLogFreetypeDrawScreen();

    std::string sdPath = convertToPosixPath("/vol/external01/");
    if (sdPath.empty()) {
        WHBLogFreetypePrint(L"Failed to get SD card path!");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    if (!extractZip(zip_data, sdPath)) {
        WHBLogFreetypePrint(L"Failed to extract zip!");
        WHBLogFreetypeDrawScreen();
        return false;
    }

    WHBLogFreetypePrint(L"Aroma successfully installed!");
    WHBLogFreetypeDrawScreen();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}
