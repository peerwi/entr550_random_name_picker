// For Arduino Nano 33 BLE - uses built-in mbed FlashIAP
// No additional libraries needed!

#include <FlashIAPBlockDevice.h>
#include <mbed.h>

using namespace mbed;

#define MAX_NAMES 50
#define MAX_NAME_LENGTH 32
#define FLASH_STORAGE_SIZE (4096) // 4KB flash sector

// Structure to store in flash memory
struct NameStorage {
  uint32_t magic;  // Magic number to verify valid data
  int count;
  char names[MAX_NAMES][MAX_NAME_LENGTH + 1];
};

// Flash storage address (last sector of flash to avoid conflicts)
#define FLASH_MAGIC 0x4E414D45  // "NAME" in hex
FlashIAPBlockDevice flash(0xE0000, FLASH_STORAGE_SIZE);

String inputBuffer = "";
NameStorage storage;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
  Serial.println("Arduino Nano 33 BLE - Flash Storage Name Manager Ready");
  Serial.println("Commands: READ, CLEAR, COUNT:n, NAME:i:name, SAVE");
  
  // Initialize flash
  flash.init();
  
  // Read existing data from flash storage on startup
  readFromFlash();
}

void loop() {
  if (Serial.available() > 0) {
    char inChar = Serial.read();
    
    if (inChar == '\n') {
      processCommand(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += inChar;
    }
  }
}

void processCommand(String command) {
  command.trim();
  
  if (command == "READ") {
    readFromFlash();
    sendNamesToSerial();
  }
  else if (command == "CLEAR") {
    clearNames();
    Serial.println("OK: Names cleared from memory");
  }
  else if (command == "SAVE") {
    saveToFlash();
    Serial.println("OK: Names saved to flash storage");
  }
  else if (command.startsWith("COUNT:")) {
    int count = command.substring(6).toInt();
    if (count >= 0 && count <= MAX_NAMES) {
      storage.count = count;
      Serial.print("OK: Count set to ");
      Serial.println(count);
    } else {
      Serial.print("ERROR: Invalid count. Max is ");
      Serial.println(MAX_NAMES);
    }
  }
  else if (command.startsWith("NAME:")) {
    // Format: NAME:index:name
    int firstColon = command.indexOf(':');
    int secondColon = command.indexOf(':', firstColon + 1);
    
    if (secondColon > 0) {
      int index = command.substring(firstColon + 1, secondColon).toInt();
      String name = command.substring(secondColon + 1);
      
      if (index >= 0 && index < MAX_NAMES) {
        name = name.substring(0, MAX_NAME_LENGTH);
        name.toCharArray(storage.names[index], MAX_NAME_LENGTH + 1);
        Serial.print("OK: Name ");
        Serial.print(index);
        Serial.print(" set to: ");
        Serial.println(storage.names[index]);
      } else {
        Serial.println("ERROR: Invalid index");
      }
    }
  }
  else {
    Serial.print("ERROR: Unknown command: ");
    Serial.println(command);
  }
}

void clearNames() {
  storage.count = 0;
  storage.magic = FLASH_MAGIC;
  for (int i = 0; i < MAX_NAMES; i++) {
    storage.names[i][0] = '\0';
  }
}

void saveToFlash() {
  storage.magic = FLASH_MAGIC;
  
  // Erase the flash sector first
  int err = flash.erase(0, FLASH_STORAGE_SIZE);
  if (err != 0) {
    Serial.println("ERROR: Flash erase failed");
    return;
  }
  
  // Write data to flash
  err = flash.program(&storage, 0, sizeof(NameStorage));
  if (err != 0) {
    Serial.println("ERROR: Flash write failed");
    return;
  }
  
  Serial.print("Saved ");
  Serial.print(storage.count);
  Serial.println(" names to flash storage");
}

void readFromFlash() {
  // Read data from flash
  int err = flash.read(&storage, 0, sizeof(NameStorage));
  
  if (err != 0) {
    Serial.println("ERROR: Flash read failed");
    clearNames();
    return;
  }
  
  // Check magic number to verify valid data
  if (storage.magic != FLASH_MAGIC) {
    Serial.println("No valid data in flash storage (first time use)");
    clearNames();
    return;
  }
  
  // Validate count
  if (storage.count < 0 || storage.count > MAX_NAMES) {
    Serial.println("Corrupted data in flash storage");
    clearNames();
    return;
  }
  
  Serial.print("Read ");
  Serial.print(storage.count);
  Serial.println(" names from flash storage");
}

void sendNamesToSerial() {
  Serial.println("=== Names in Flash Storage ===");
  Serial.print("Count: ");
  Serial.println(storage.count);
  
  for (int i = 0; i < storage.count; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(storage.names[i]);
  }
  Serial.println("==============================");
}
