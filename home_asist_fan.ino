#include <NimBLEDevice.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define PWM1_UUID           "12345678-1234-1234-1234-1234567890ac"
#define PWM2_UUID           "12345678-1234-1234-1234-1234567890ad"
#define PWM3_UUID           "12345678-1234-1234-1234-1234567890ae"
#define PWM4_UUID           "12345678-1234-1234-1234-1234567890af"
#define RPM1_UUID           "12345678-1234-1234-1234-1234567890b0"
#define RPM2_UUID           "12345678-1234-1234-1234-1234567890b1"
#define RPM3_UUID           "12345678-1234-1234-1234-1234567890b2"
#define RPM4_UUID           "12345678-1234-1234-1234-1234567890b3"

const int pwmPins[4] = {0, 1, 2, 3};
const int rpmPins[4] = {5, 6, 7, 8};

volatile uint32_t rpmCounts[4] = {0, 0, 0, 0};
uint32_t lastRpm[4] = {0, 0, 0, 0};
uint8_t pwmValues[4] = {128, 128, 128, 128}; // 0-255

NimBLECharacteristic* pwmChar[4];
NimBLECharacteristic* rpmChar[4];

void IRAM_ATTR rpm_isr0() {
  rpmCounts[0]++;
  Serial.println("RPM0 ISR triggered"); // Отладка
}
void IRAM_ATTR rpm_isr1() { rpmCounts[1]++; }
void IRAM_ATTR rpm_isr2() { rpmCounts[2]++; }
void IRAM_ATTR rpm_isr3() { rpmCounts[3]++; }

class PWMCallback : public NimBLECharacteristicCallbacks {
  int idx;
public:
  PWMCallback(int i) : idx(i) {}
  void onWrite(NimBLECharacteristic* pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    int pwm = atoi(value.c_str());
    if (pwm < 0) pwm = 0;
    if (pwm > 255) pwm = 255;
    pwmValues[idx] = pwm;
    ledcWrite(idx, pwm);
    Serial.printf("Fan %d PWM set to %d\n", idx+1, pwm);

    String val = String(pwmValues[idx]);
    pCharacteristic->setValue(val.c_str());
  }
};

void setup() {
  Serial.begin(115200);

  // PWM setup
  for (int i = 0; i < 4; i++) {
    ledcAttachPin(pwmPins[i], i);
    ledcSetup(i, 25000, 8);
    ledcWrite(i, pwmValues[i]);
  }

  // RPM input setup
  attachInterrupt(digitalPinToInterrupt(rpmPins[0]), rpm_isr0, RISING);
  attachInterrupt(digitalPinToInterrupt(rpmPins[1]), rpm_isr1, RISING);
  attachInterrupt(digitalPinToInterrupt(rpmPins[2]), rpm_isr2, RISING);
  attachInterrupt(digitalPinToInterrupt(rpmPins[3]), rpm_isr3, RISING);

  // BLE
  NimBLEDevice::init("FanController");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // PWM characteristics (write, string)
  pwmChar[0] = pService->createCharacteristic(PWM1_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
  pwmChar[1] = pService->createCharacteristic(PWM2_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
  pwmChar[2] = pService->createCharacteristic(PWM3_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
  pwmChar[3] = pService->createCharacteristic(PWM4_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
  for (int i = 0; i < 4; i++) {
    pwmChar[i]->setCallbacks(new PWMCallback(i));
    String val = String(pwmValues[i]);
    pwmChar[i]->setValue(val.c_str());
  }

  // RPM characteristics (read, string)
  rpmChar[0] = pService->createCharacteristic(RPM1_UUID, NIMBLE_PROPERTY::READ);
  rpmChar[1] = pService->createCharacteristic(RPM2_UUID, NIMBLE_PROPERTY::READ);
  rpmChar[2] = pService->createCharacteristic(RPM3_UUID, NIMBLE_PROPERTY::READ);
  rpmChar[3] = pService->createCharacteristic(RPM4_UUID, NIMBLE_PROPERTY::READ);
  for (int i = 0; i < 4; i++) {
    rpmChar[i]->setValue("0");
  }

  pService->start();
  NimBLEDevice::getAdvertising()->start();
  Serial.println("BLE Fan Controller started");
}

void loop() {
  static uint32_t lastMillis = 0;
  if (millis() - lastMillis > 1000) {
    lastMillis = millis();

    for (int i = 0; i < 4; i++) {
      lastRpm[i] = rpmCounts[i] * 60; // если 1 импульс = 1 оборот
      rpmCounts[i] = 0;

      char buffer[10];
      sprintf(buffer, "%u", lastRpm[i]); // безопасное преобразование
      rpmChar[i]->setValue(buffer);     // отправляем значение по BLE
    }
  }
}