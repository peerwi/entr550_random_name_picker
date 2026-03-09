#include <debounce.h>
#include <LiquidCrystal.h>
#include <string.h>

#if defined(ARDUINO_ARCH_MBED)

// ===== Nano 33 BLE Flash Storage =====
#include <FlashIAPBlockDevice.h>
#include <mbed.h>
using namespace mbed;

#define USE_FLASH_STORAGE

#else

// ===== AVR / UNO EEPROM Storage =====
#include <EEPROM.h>

#define USE_EEPROM_STORAGE

#endif


#define MAX_NAMES 20
#define MAX_NAME_LENGTH 32

#if defined(USE_FLASH_STORAGE)
#define STORAGE_SIZE 4096
#define FLASH_MAGIC 0x4E414D45
FlashIAPBlockDevice flash(0xE0000, STORAGE_SIZE);
#endif

#define STORAGE_MAGIC 0x4E414D45

#define BUTTON 2


struct NameStorage {
  uint32_t magic;
  size_t count;
  char names[MAX_NAMES][MAX_NAME_LENGTH + 1];
};

NameStorage storage;
String inputBuffer = "";

LiquidCrystal lcd(4, 6, 10, 11, 12, 13);




uint8_t order[MAX_NAMES];
size_t pick_pos = 0;

void shuffleOrder() {
  for (size_t i = 0; i < storage.count; i++) order[i] = (uint8_t)i;

  // Fisher–Yates shuffle
  for (size_t i = storage.count - 1; i > 0; i--) {
    size_t j = (size_t)random(i + 1);   // 0..i
    uint8_t t = order[i];
    order[i] = order[j];
    order[j] = t;
  }

  pick_pos = 0;
}

bool pickNext(const char*& out) {
  if (pick_pos >= storage.count) return false;           // all used up
  out = storage.names[order[pick_pos++]];
  return true;
}

void buttonPressed(uint8_t btnId, uint8_t btnState) {
  if (btnState == BTN_PRESSED) {
    const char* name;
    if (!pickNext(name)) {
      shuffleOrder();
      pickNext(name);
    }

    lcd.clear();
    lcd.print(name);
  }
}

static Button myButton(0, buttonPressed);

void setup() {

  /************ SERIAL SETUP ***************/
  Serial.begin(9600);
  while (!Serial) {}

  Serial.println("Name Manager Ready");
  Serial.println("Commands: READ, CLEAR, COUNT:n, NAME:i:name, SAVE");

  /*************  LCD SETUP ****************/
  lcd.begin(16, 2);
  lcd.print("Push button");
  lcd.setCursor(0, 1);
  lcd.print("to start");

  /***********  BUTTON SETUP *************/
  pinMode(BUTTON, INPUT_PULLUP);

  /************ FLASH SETUP ***************/

#ifdef USE_FLASH_STORAGE
  flash.init();
#endif

  readStorage();

  /*********** RANDOM SETUP   ************/

  unsigned long seed = micros();
  for (int k = 0; k < 32; k++) {
    seed = (seed << 1) ^ (unsigned long)analogRead(A0);
    delay(1);
  }
  randomSeed(seed);

  shuffleOrder();
}

void pollButtons() {
  myButton.update(digitalRead(BUTTON));
}

void loop() {

  pollButtons();

  if (Serial.available()) {

    char c = Serial.read();

    if (c == '\n') {
      processCommand(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }
}


void processCommand(String command) {

  command.trim();

  if (command == "READ") {

    readStorage();
    sendNames();

  } else if (command == "CLEAR") {

    clearNames();
    Serial.println("OK: Names cleared");

  } else if (command == "SAVE") {

    saveStorage();
    Serial.println("OK: Names saved");

  } else if (command.startsWith("COUNT:")) {

    int count = command.substring(6).toInt();

    if (count >= 0 && count <= MAX_NAMES) {
      storage.count = count;
      Serial.print("OK: Count set to ");
      Serial.println(count);
    } else {
      Serial.println("ERROR: Invalid count");
    }

  } else if (command.startsWith("NAME:")) {

    int first = command.indexOf(':');
    int second = command.indexOf(':', first + 1);

    if (second > 0) {

      int index = command.substring(first + 1, second).toInt();
      String name = command.substring(second + 1);

      if (index >= 0 && index < MAX_NAMES) {

        name = name.substring(0, MAX_NAME_LENGTH);
        name.toCharArray(storage.names[index], MAX_NAME_LENGTH + 1);

        Serial.print("OK: Name ");
        Serial.print(index);
        Serial.print(" set to ");
        Serial.println(storage.names[index]);

      } else {
        Serial.println("ERROR: Invalid index");
      }
    }

  } else {

    Serial.print("ERROR: Unknown command: ");
    Serial.println(command);
  }
}


void clearNames() {

  storage.magic = STORAGE_MAGIC;
  storage.count = 0;

  for (int i = 0; i < MAX_NAMES; i++)
    storage.names[i][0] = '\0';
}


void saveStorage() {

  storage.magic = STORAGE_MAGIC;

#ifdef USE_FLASH_STORAGE

  if (flash.erase(0, STORAGE_SIZE) != 0) {
    Serial.println("ERROR: Flash erase failed");
    return;
  }

  if (flash.program(&storage, 0, sizeof(NameStorage)) != 0) {
    Serial.println("ERROR: Flash write failed");
    return;
  }

#else

  EEPROM.put(0, storage);

#endif

  Serial.print("Saved ");
  Serial.print(storage.count);
  Serial.println(" names");
}


void readStorage() {

#ifdef USE_FLASH_STORAGE

  if (flash.read(&storage, 0, sizeof(NameStorage)) != 0) {
    Serial.println("ERROR: Flash read failed");
    clearNames();
    return;
  }

#else

  EEPROM.get(0, storage);

#endif

  if (storage.magic != STORAGE_MAGIC) {

    Serial.println("No valid storage data");
    clearNames();
    return;
  }

  if (storage.count < 0 || storage.count > MAX_NAMES) {

    Serial.println("Storage corrupted");
    clearNames();
    return;
  }

  Serial.print("Loaded ");
  Serial.print(storage.count);
  Serial.println(" names");
}


void sendNames() {

  Serial.println("=== Names ===");

  Serial.print("Count: ");
  Serial.println(storage.count);

  for (int i = 0; i < storage.count; i++) {

    Serial.print(i);
    Serial.print(": ");
    Serial.println(storage.names[i]);
  }

  Serial.println("=============");
}