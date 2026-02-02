#include "menu.h"
#include "navigation.h"
#include "filesystem.h"
#include "gui.h"
#include "cfw.h"
#include "fw_img_loader.h"
#include "download.h"
#include <dirent.h>
#include <algorithm>
#include <vector>
#include <string>
#include <stroopwafel/stroopwafel.h>

// Menu screens

void showLoadingScreen() {
    WHBLogFreetypeSetBackgroundColor(0x0b5d5e00);
    WHBLogFreetypeSetFontColor(0xFFFFFFFF);
    WHBLogFreetypeSetFontSize(22);
    WHBLogPrint("ISFShax Loader");
    WHBLogPrint("-- Made by Crementif, Emiyl and Jan Hofmeier --");
    WHBLogPrint("");
    WHBLogFreetypeDraw();
}

#define OPTION(n) (selectedOption == (n) ? L'>' : L' ')

void installISFShax() {
    if (downloadHaxFiles()) {
        showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue");
        loadFwImg();
    } else {
        showErrorPrompt(L"OK");
    }
}

void redownloadFiles() {
    if (downloadHaxFiles()) {
        showDialogPrompt(L"All hax files downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }
}

void bootInstaller() {
    std::string fwImgPath = convertToPosixPath("/vol/storage_slc/sys/hax/installer/fw.img");
    bool downloaded = true;
    if (!fileExist(fwImgPath.c_str())) {
        uint8_t choice = showDialogPrompt(L"The ISFShax installer (fw.img) is missing.\nDo you want to download everything or just the installer?", L"Everything", L"Just installer");
        if (choice == 0) {
            downloaded = downloadHaxFiles();
        } else {
            downloaded = downloadInstallerOnly();
        }

        if (!downloaded) {
             showErrorPrompt(L"OK");
             return;
        }
    }

    if (downloaded) {
        showDialogPrompt(L"The ISFShax installer is controlled with the buttons on the main console.\nPOWER: moves the curser\nEJECT: confirm\nPress A to launch into the ISFShax Installer", L"Continue");
        loadFwImg();
    }
}

void installAromaMenu() {
    if (downloadAroma()) {
        showDialogPrompt(L"Aroma and tools downloaded and extracted successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }
}

void formatUsbAndDownloadAromaMenu() {
    uint8_t choice = showDialogPrompt(L"WARNING: This will format the USB drive and DELETE ALL DATA on it.\nDo you want to continue?", L"Yes", L"No");
    if (choice != 0) return;

    if (!formatUsbFat()) {
        setErrorPrompt(L"Failed to format USB drive!");
        showErrorPrompt(L"OK");
        return;
    }

    if (!mountUsbFat()) {
        setErrorPrompt(L"Failed to mount USB drive after formatting!");
        showErrorPrompt(L"OK");
        return;
    }

    if (downloadAroma("usb:/")) {
        showDialogPrompt(L"USB drive formatted and Aroma downloaded successfully!", L"OK");
    } else {
        showErrorPrompt(L"OK");
    }

    unmountUsbFat();
}

void managePlugins(std::string posixPath) {
    uint8_t selectedOption = 0;
    bool refreshList = true;
    std::vector<std::string> plugins;

    while(true) {
        if (refreshList) {
            plugins.clear();
            DIR* dir = opendir(posixPath.c_str());
            if (dir) {
                struct dirent* ent;
                while ((ent = readdir(dir)) != nullptr) {
                    if (ent->d_type == DT_REG) {
                        plugins.push_back(ent->d_name);
                    }
                }
                closedir(dir);
            }
            std::sort(plugins.begin(), plugins.end());
            refreshList = false;
        }

        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrintf(L"Managing: %S", toWstring(posixPath).c_str());
        WHBLogFreetypePrint(L"===============================");

        if (plugins.empty()) {
            WHBLogFreetypePrint(L"No plugins found.");
        } else {
            for (size_t i = 0; i < plugins.size(); i++) {
                WHBLogFreetypePrintf(L"%C %S", OPTION(i), toWstring(plugins[i]).c_str());
            }
        }

        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Select \uE001 Back \uE003 Delete Plugin");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while(true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && !plugins.empty() && selectedOption < plugins.size() - 1) {
                selectedOption++;
                break;
            }
            if (pressedBack()) {
                return;
            }
            if (pressedX() && !plugins.empty()) {
                std::string pluginName = plugins[selectedOption];
                if (pluginName == "00core.ipx" || pluginName == "5isfshax.ipx") {
                    showDialogPrompt(L"This plugin is protected and cannot be deleted!", L"OK");
                } else {
                    std::wstring msg = L"Do you really want to delete " + toWstring(pluginName) + L"?";
                    if (showDialogPrompt(msg.c_str(), L"Yes", L"No") == 0) {
                        std::string fullPath = posixPath;
                        if (fullPath.back() != '/') fullPath += "/";
                        fullPath += pluginName;
                        if (remove(fullPath.c_str()) == 0) {
                            showDialogPrompt(L"Plugin deleted successfully!", L"OK");
                            if (selectedOption > 0 && selectedOption == plugins.size() - 1) {
                                selectedOption--;
                            }
                            refreshList = true;
                        } else {
                            setErrorPrompt(L"Failed to delete plugin!");
                            showErrorPrompt(L"OK");
                        }
                    }
                }
                if (refreshList) break;
            }
            sleep_for(50ms);
        }
    }
}

void showPluginManager() {
    StroopwafelMinutePath currentPath = {0};
    bool stroopAvailable = isStroopwafelAvailable();
    if (stroopAvailable) {
        Stroopwafel_GetPluginPath(&currentPath);
    }

    std::string slcDefaultPath = "/sys/hax/ios_plugins";
    std::string sdDefaultPath = "/wiiu/ios_plugins";

    std::string slcPosix = convertToPosixPath("/vol/storage_slc/sys/hax/ios_plugins");
    std::string sdPosix = convertToPosixPath("/vol/external01/wiiu/ios_plugins");

    std::string currentPosix;
    if (stroopAvailable) {
        if (currentPath.device == STROOPWAFEL_MIN_DEV_SLC) {
            currentPosix = convertToPosixPath(("/vol/storage_slc" + std::string(currentPath.path)).c_str());
        } else if (currentPath.device == STROOPWAFEL_MIN_DEV_SD) {
            currentPosix = convertToPosixPath(("/vol/external01" + std::string(currentPath.path)).c_str());
        }
    }

    std::vector<std::pair<std::wstring, std::string>> options;
    options.push_back({L"SLC Plugins (/sys/hax/ios_plugins)", slcPosix});
    options.push_back({L"SD Plugins (/wiiu/ios_plugins)", sdPosix});

    uint8_t selectedOption = 0;
    if (stroopAvailable && !currentPosix.empty()) {
        bool isSLCDefault = (currentPath.device == STROOPWAFEL_MIN_DEV_SLC && std::string(currentPath.path) == slcDefaultPath);
        bool isSDDefault = (currentPath.device == STROOPWAFEL_MIN_DEV_SD && std::string(currentPath.path) == sdDefaultPath);

        if (isSLCDefault) {
            selectedOption = 0;
        } else if (isSDDefault) {
            selectedOption = 1;
        } else {
            std::wstring devName = (currentPath.device == STROOPWAFEL_MIN_DEV_SLC ? L"SLC" : L"SD");
            options.push_back({L"Current Plugins (" + devName + L": " + toWstring(currentPath.path) + L")", currentPosix});
            selectedOption = 2;
        }
    }

    while(true) {
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"Stroopwafel Plugin Manager");
        WHBLogFreetypePrint(L"===============================");
        for (size_t i = 0; i < options.size(); i++) {
            WHBLogFreetypePrintf(L"%C %S", OPTION(i), options[i].first.c_str());
        }
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Back");
        WHBLogFreetypeDrawScreen();

        sleep_for(100ms);
        updateInputs();
        while(true) {
            updateInputs();
            if (navigatedUp() && selectedOption > 0) {
                selectedOption--;
                break;
            }
            if (navigatedDown() && selectedOption < options.size() - 1) {
                selectedOption++;
                break;
            }
            if (pressedOk()) {
                managePlugins(options[selectedOption].second);
                break;
            }
            if (pressedBack()) {
                return;
            }
            sleep_for(50ms);
        }
    }
}

// Can get recursively called
void showMainMenu() {
    uint8_t selectedOption = 0;
    bool startSelectedOption = false;
    while(!startSelectedOption) {
        // Print menu text
        WHBLogFreetypeStartScreen();
        WHBLogFreetypePrint(L"ISFShax Loader");
        WHBLogFreetypePrint(L"===============================");
        WHBLogFreetypePrintf(L"%C Install ISFShax + sd emulation + payloadler", OPTION(0));
        WHBLogFreetypePrintf(L"%C Redownload files", OPTION(1));
        WHBLogFreetypePrintf(L"%C Boot Installer", OPTION(2));
        WHBLogFreetypePrintf(L"%C Download Aroma", OPTION(3));
        WHBLogFreetypePrintf(L"%C Format USB and Download Aroma", OPTION(4));
        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrintf(L"%C Stroopwafel Plugin Manager", OPTION(6));
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option \uE001 Button = Exit ISFShax Loader");
        WHBLogFreetypeScreenPrintBottom(L"");
        WHBLogFreetypeDrawScreen();

        // Loop until there's new input
        sleep_for(200ms); // Cooldown between each button press
        updateInputs();
        while(!startSelectedOption) {
            updateInputs();
            // Check each button state
            if (navigatedUp()) {
                if (selectedOption == 6) {
                    selectedOption = 4;
                    break;
                } else if (selectedOption > 0) {
                    selectedOption--;
                    break;
                }
            }
            if (navigatedDown()) {
                if (selectedOption == 4) {
                    selectedOption = 6;
                    break;
                } else if (selectedOption < 4) {
                    selectedOption++;
                    break;
                }
            }
            if (pressedOk()) {
                startSelectedOption = true;
                break;
            }
            if (pressedBack()) {
                uint8_t exitSelectedOption = showDialogPrompt(getCFWVersion() == MOCHA_FSCLIENT ? L"Do you really want to exit ISFShax Loader?" : L"Do you really want to exit ISFShax Loader?\nYour console will reboot to prevent compatibility issues!", L"Yes", L"No");
                if (exitSelectedOption == 0) {
                    WHBLogFreetypeClear();
                    return;
                }
                else break;
            }
            sleep_for(50ms);
        }
    }

    // Go to the selected menu
    switch(selectedOption) {
        case 0:
            installISFShax();
            break;
        case 1:
            redownloadFiles();
            break;
        case 2:
            bootInstaller();
            break;
        case 3:
            installAromaMenu();
            break;
        case 4:
            formatUsbAndDownloadAromaMenu();
            break;
        case 6:
            showPluginManager();
            break;
        default:
            break;
    }

    sleep_for(500ms);
    showMainMenu();
}


// Helper functions

uint8_t showDialogPrompt(const wchar_t* message, const wchar_t* button1, const wchar_t* button2) {
    sleep_for(100ms);
    uint8_t selectedOption = 0;
    while(true) {
        WHBLogFreetypeStartScreen();

        // Print each line
        std::wistringstream messageStream(message);
        std::wstring line;

        while(std::getline(messageStream, line)) {
            WHBLogFreetypePrint(line.c_str());
        }

        WHBLogFreetypePrint(L"");
        WHBLogFreetypePrintf(L"%C [%S]", OPTION(0), button1);
        if (button2 != nullptr) WHBLogFreetypePrintf(L"%C [%S]", OPTION(1), button2);
        WHBLogFreetypePrint(L"");
        WHBLogFreetypeScreenPrintBottom(L"===============================");
        WHBLogFreetypeScreenPrintBottom(L"\uE000 Button = Select Option");
        WHBLogFreetypeDrawScreen();

        // Input loop
        sleep_for(400ms);
        updateInputs();
        while (true) {
            updateInputs();
            // Handle navigation between the buttons
            if (button2 != nullptr) {
                if (navigatedUp() && selectedOption == 1) {
                    selectedOption = 0;
                    break;
                }
                else if (navigatedDown() && selectedOption == 0) {
                    selectedOption = 1;
                    break;
                }
            }

            if (pressedOk()) {
                return selectedOption;
            }

            sleep_for(50ms);
        }
    }
}

void showDialogPrompt(const wchar_t* message, const wchar_t* button) {
    showDialogPrompt(message, button, nullptr);
}

const wchar_t* errorMessage = nullptr;
void setErrorPrompt(const wchar_t* message) {
    errorMessage = message;
}

std::wstring messageCopy;
void setErrorPrompt(std::wstring message) {
    messageCopy = std::move(message);
    setErrorPrompt(messageCopy.c_str());
}

void showErrorPrompt(const wchar_t* button) {
    std::wstring promptMessage(L"An error occurred:\n");
    if (errorMessage) promptMessage += errorMessage;
    else promptMessage += L"No error was specified!";
    showDialogPrompt(promptMessage.c_str(), button);
}
