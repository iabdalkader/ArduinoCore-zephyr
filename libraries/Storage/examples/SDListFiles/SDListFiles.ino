/*
  SD List Files

  This sketch demonstrates how to list files on an SD card
  on Arduino boards with Zephyr RTOS.

  The SD card is auto-mounted at boot as a FAT filesystem
  at "/SD:". If no card is inserted, the mount is skipped.

  Compatible with boards that have an SD card slot
  (e.g. Portenta H7 with a Portenta Breakout Board).

  This example code is in the public domain.
*/

#include <Arduino.h>
#include <zephyr/fs/fs.h>

#define SD_MOUNT_PT "/SD:"

void listDir(const char *path) {
    struct fs_dir_t dir = {0};
    struct fs_dirent entry;

    fs_dir_t_init(&dir);
    int res = fs_opendir(&dir, path);

    if (res) {
        Serial.print("Error opening directory ");
        Serial.print(path);
        Serial.print(" [error code: ");
        Serial.print(res);
        Serial.println("]");
        return;
    }

    Serial.print("\nContents of ");
    Serial.print(path);
    Serial.println(":");

    bool empty = true;
    while (true) {
        res = fs_readdir(&dir, &entry);
        if (res || entry.name[0] == '\0') {
            break;
        }

        empty = false;
        if (entry.type == FS_DIR_ENTRY_FILE) {
            Serial.print("  [FILE] ");
            Serial.print(entry.name);
            Serial.print(" (");
            Serial.print((size_t) entry.size);
            Serial.println(" bytes)");
        } else if (entry.type == FS_DIR_ENTRY_DIR) {
            Serial.print("  [DIR ] ");
            Serial.println(entry.name);
        }
    }

    if (empty) {
        Serial.println("  <empty>");
    }

    fs_closedir(&dir);
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n=== SD Card File Listing ===\n");

    // Check if the SD card is mounted
    struct fs_statvfs stat;
    int res = fs_statvfs(SD_MOUNT_PT, &stat);
    if (res) {
        Serial.println("SD card not mounted. Is one inserted?");
        return;
    }

    unsigned long total_kb = (unsigned long)stat.f_frsize * stat.f_blocks / 1024;
    unsigned long free_kb = (unsigned long)stat.f_frsize * stat.f_bfree / 1024;
    Serial.print("SD card: ");
    Serial.print(free_kb / 1024);
    Serial.print(" MB free / ");
    Serial.print(total_kb / 1024);
    Serial.println(" MB total");

    listDir(SD_MOUNT_PT);

    Serial.println("\nDone!");
}

void loop() {
    delay(1000);
}
