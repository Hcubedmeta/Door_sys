#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

// ---------- Relay + LCD ----------
#define RELAY_PIN 13
#define LED_PIN 23
#define buzzer_PIN 5
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ LCD có thể là 0x3F tùy module

HardwareSerial mySerial(1);  // UART1: RX = GPIO18, TX = GPIO19
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t nextID = 2;
bool deleteMode = false;
bool masterDeleted = false;

// ---------- Keypad ----------
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};  // chỉnh theo thực tế
byte colPins[COLS] = {27, 14, 12};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------- Biến điều khiển ----------
String inputPassword = "";
String correctPassword = "1234";
unsigned long unlockTime = 0;
const unsigned long unlockDuration = 5000;

bool changeMode = false;
unsigned long starPressTime = 0;
unsigned long hashPressTime = 0;
const unsigned long comboThreshold = 300;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(buzzer_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  mySerial.begin(57600, SERIAL_8N1, 18, 19);
  finger.begin(57600);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  if (!finger.verifyPassword()) {
     lcd.print("no connection");
    while (1);
  }
  lcd.clear();
  finger.getTemplateCount();
  lcd.setCursor(0, 0);
  lcd.print("fingers saved: ");
  lcd.setCursor(10, 0);
  lcd.print(finger.templateCount);
  lcd.clear();
  lcd.setCursor(0, 0);
  if (finger.templateCount == 0) {
    lcd.println("Chưa có Master..");
    while (getFingerprintEnroll(1) != FINGERPRINT_OK) {
      lcd.setCursor(0, 1);
      Serial.println("master that bai");
      delay(2000);
    }
    finger.getTemplateCount();
    nextID = finger.templateCount + 1;
  } else {
    nextID = finger.templateCount + 1;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Nhap mat khau:");
}

void loop() {
  char key = keypad.getKey();  // Đọc key trước tiên

  // ======= Chế độ đổi mật khẩu =======
  if (changeMode) {
    if (key == '#') {
      if (inputPassword.length() >= 4) {
        correctPassword = inputPassword;
        Serial.println("✅ Doi mat khau OK.");
        lcd.clear();
        lcd.print("Da doi mat khau!");
      } else {
        Serial.println("❌ Qua ngan!");
        lcd.clear();
        lcd.print("Mat khau qua ngan");
      }
      inputPassword = "";
      changeMode = false;
      delay(2000);
      lcd.clear();
      lcd.print("Nhap mat khau:");
    }
    else if (key == '*') {
      inputPassword = "";
      Serial.println("🔄 Xoa mat khau moi.");
      lcd.clear();
      lcd.print("Nhap lai mat khau:");
    }
    else if (key) {
      inputPassword += key;
      Serial.print("MK moi: ");
      Serial.println(inputPassword);
      lcd.setCursor(0, 1);
      lcd.print(inputPassword);
    }
    return; // Ngăn không xử lý các phần còn lại
  }

  // ======= Quét vân tay =======
  if (masterDeleted || finger.templateCount == 0) {
    Serial.println("🔐 Không còn Master. Đăng ký lại ID #1...");
    while (getFingerprintEnroll(1) != FINGERPRINT_OK) {
      Serial.println("❌ Đăng ký lại master thất bại, thử lại...");
      delay(2000);
    }
    finger.getTemplateCount();
    nextID = finger.templateCount + 1;
    masterDeleted = false;
    return;
  }

  uint8_t result = getFingerprintID();
  if (result == FINGERPRINT_OK) {
    if (finger.fingerID == 1) {
      Serial.println("🔓 Master xác thực!");
      Serial.println("➕ Thêm vân tay mới...");
      while (getFingerprintEnroll(nextID) != FINGERPRINT_OK) {
        Serial.println("❌ Thử lại đăng ký...");
        delay(2000);
      }
      finger.getTemplateCount();
      nextID = finger.templateCount + 1;
      Serial.print("📦 Tổng số vân tay đã lưu: ");
      Serial.println(finger.templateCount);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("van tay ID#"); 
      lcd.setCursor(13, 0);
      lcd.print(finger.fingerID);
      lcd.setCursor(0, 1);
      lcd.print("duoc nhan dang"); 
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(buzzer_PIN, LOW);
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("nhap mat khau"); 
      digitalWrite(RELAY_PIN, LOW);
      digitalWrite(LED_PIN, LOW);
    }
  } else if (result == FINGERPRINT_NOTFOUND) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ko nhan dang"); 
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(buzzer_PIN, HIGH);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("nhap mat khau"); 
    digitalWrite(buzzer_PIN, LOW);
  }

  // ======= Xử lý bàn phím =======
  if (key) {
    Serial.print("Nhấn: "); Serial.println(key);
    if (key == '#') {
      if (inputPassword == "0000") {
        // Xóa vân tay
        Serial.println("🧹 Kích hoạt chế độ xóa vân tay...");
        lcd.clear();
        lcd.print("Xoa van tay...");
        deleteMode = true;
        delay(3000);  // Cho thời gian rút tay ra

        while (deleteMode) {
          uint8_t delResult = getFingerprintID();
          if (delResult == FINGERPRINT_OK) {
            int delID = finger.fingerID;

            if (delID == 1) {
              Serial.println("⚠️ Bạn đang xóa Master...");
              if (finger.deleteModel(1) == FINGERPRINT_OK) {
                Serial.println("✅ Đã xóa Master.");
                lcd.clear(); lcd.print("Da xoa Master");
                masterDeleted = true;
              } else {
                Serial.println("❌ Xóa Master thất bại!");
                lcd.clear(); lcd.print("Xoa that bai!");
              }
              deleteMode = false;
            } else {
              if (finger.deleteModel(delID) == FINGERPRINT_OK) {
                Serial.print("✅ Đã xóa vân tay ID #");
                Serial.println(delID);
                lcd.clear(); lcd.print("Xoa ID #");
                lcd.print(delID);
              } else {
                Serial.println("❌ Xóa thất bại!");
                lcd.clear(); lcd.print("Xoa that bai!");
              }
              deleteMode = false;
            }

            finger.getTemplateCount();
            nextID = finger.templateCount + 1;
          }
          delay(200);
        }

        inputPassword = "";
        delay(2000);
        lcd.clear();
        lcd.print("Nhap mat khau:");
      } 
      else if (inputPassword == correctPassword + "0") {
        changeMode = true;
        inputPassword = "";
        Serial.println("🔐 Vào chế độ đổi mật khẩu (qua mật khẩu+0).");
        lcd.clear();
        lcd.print("Doi mat khau moi:");
        return;
      }
      else if (inputPassword == correctPassword) {
        Serial.println("✅ Đúng mật khẩu.");
        lcd.clear();
        lcd.print("dung mat khau");
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(buzzer_PIN, LOW);
        unlockTime = millis();
        inputPassword = "";
        delay(2000);
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        lcd.clear();
        lcd.print("Nhap mat khau:");
      } else {
        Serial.println("❌ Sai mật khẩu.");
        lcd.clear();
        lcd.print("Sai mat khau!");
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        digitalWrite(buzzer_PIN, HIGH);
        inputPassword = "";
        delay(1000);
        lcd.clear();
        lcd.print("Nhap mat khau:");
        digitalWrite(buzzer_PIN, LOW);
      }
    } else if (key == '*') {
      inputPassword = "";
      Serial.println("🔄 Xóa MK đang nhập.");
      lcd.setCursor(0, 1);
      lcd.print("                ");
    } else {
      inputPassword += key;
      Serial.print("Đăng nhập: "); Serial.println(inputPassword);
      lcd.setCursor(0, 1);
      lcd.print(inputPassword);
    }
  }


// ================= Nhận dạng vân tay =================
uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;

  p = finger.fingerSearch();
  return p;
}

// ================= Đăng ký vân tay =================
uint8_t getFingerprintEnroll(uint8_t id) {
  int p = -1;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("de ngon tay vao");
  lcd.setCursor(0, 1);
  lcd.print(id);

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      Serial.print(".");
      delay(100);
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return p;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ngon tay lan 1");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ngon tay lan 2");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      Serial.print(".");
      delay(100);
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) return p;

  p = finger.createModel();
  if (p != FINGERPRINT_OK) return p;

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("thanh cong");
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("nhap mat khau");   
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("that bai");
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("nhap mat khau");   
  }

  return p;
}
