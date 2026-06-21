#include <EEPROM.h>

#define COM_SPEED 57600
const int VOLT_PIN = A1;
const int AC_PIN = 2;
const int CAL_BTN_PIN = 3;

const float V_REF = 3.3;
const int ADC_RES = 1024;
const int AC_DEBOUNCE_MS = 500;
const float LINEV_NOMINAL = 230.0;

struct CalibrationData {
  float factor;
  bool isValid;
};

float calibrationFactor = 9.2;
bool acPowerOn = false;
bool lastAcState = false;
unsigned long acDebounceStart = 0;
float lastBatteryVoltage = 0.0;

void setup() {
  Serial.begin(COM_SPEED);
  
  pinMode(AC_PIN, INPUT_PULLUP);
  pinMode(CAL_BTN_PIN, INPUT_PULLUP);
  
  loadCalibration();
  
  if (digitalRead(CAL_BTN_PIN) == LOW || !isCalibrationValid()) {
    Serial.println("=== CALIBRATION MODE ===");
    Serial.println("Apply known voltage to VOLT_PIN (e.g. 12.00V)");
    Serial.println("Enter the voltage value (format: 12.00)");
    calibrate();
  }
  
  Serial.println("APC Smart Protocol Ready");
  Serial.print("Divider factor: ");
  Serial.println(calibrationFactor, 4);
  Serial.println();
}

void loop() {
  updateAcStatus();
  
  int adcValue = analogRead(VOLT_PIN);
  float voltageAtPin = (adcValue * V_REF) / ADC_RES;
  lastBatteryVoltage = voltageAtPin * calibrationFactor;
  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "STATUS" || cmd == "?" || cmd == "S") {
      sendApcStatus();
    }
    else if (cmd == "SHORT" || cmd == "Q") {
      sendShortStatus();
    }
    else if (cmd == "FACTOR") {
      sendCalibrationFactor();
    }
  }
  
  delay(100);
}

void updateAcStatus() {
  bool rawState = (digitalRead(AC_PIN) == LOW);
  
  if (rawState != lastAcState) {
    if (acDebounceStart == 0) {
      acDebounceStart = millis();
    } else if (millis() - acDebounceStart >= AC_DEBOUNCE_MS) {
      acPowerOn = rawState;
      lastAcState = rawState;
      acDebounceStart = 0;
    }
  } else {
    acDebounceStart = 0;
  }
}

void sendApcStatus() {
  if (acPowerOn) {
    Serial.println("STATUS : ONLINE");
  } else {
    Serial.println("STATUS : ONBATT");
  }
  
  Serial.print("BATTV : ");
  Serial.print(lastBatteryVoltage, 1);
  Serial.println(" Volts");
  
  Serial.print("LINEV : ");
  if (acPowerOn) {
    Serial.print(LINEV_NOMINAL, 1);
  } else {
    Serial.print("0.0");
  }
  Serial.println(" Volts");
  
  Serial.print("LOADPCT : ");
  int loadPercent = calculateLoadPercent();
  Serial.print(loadPercent);
  Serial.println(" Percent");
  
  Serial.println();
}

void sendShortStatus() {
  Serial.print(acPowerOn ? "ONLINE" : "ONBATT");
  Serial.print(" BATTV=");
  Serial.print(lastBatteryVoltage, 1);
  Serial.print(" LINEV=");
  if (acPowerOn) {
    Serial.print(LINEV_NOMINAL, 0);
  } else {
    Serial.print("0");
  }
  Serial.println();
}

void sendCalibrationFactor() {
  Serial.print("FACTOR=");
  Serial.println(calibrationFactor, 4);
}

int calculateLoadPercent() {
  float minVolt = 10.5;
  float maxVolt = 13.8;
  
  float percent = (maxVolt - lastBatteryVoltage) / (maxVolt - minVolt) * 100.0;
  
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  
  return (int)percent;
}

void loadCalibration() {
  CalibrationData data;
  EEPROM.get(0, data);
  
  if (data.isValid) {
    calibrationFactor = data.factor;
    Serial.println("Loaded calibration from EEPROM");
    Serial.print("Divider factor: ");
    Serial.println(calibrationFactor, 4);
  } else {
    Serial.println("Using default divider factor: 9.2");
  }
}

void saveCalibration(float factor) {
  CalibrationData data;
  data.factor = factor;
  data.isValid = true;
  EEPROM.put(0, data);
  Serial.print("Divider factor saved: ");
  Serial.println(factor, 4);
}

bool isCalibrationValid() {
  CalibrationData data;
  EEPROM.get(0, data);
  return data.isValid;
}

void calibrate() {
  while (!Serial.available()) {
    delay(100);
  }
  
  float realVoltage = Serial.parseFloat();
  if (realVoltage <= 0) {
    Serial.println("ERROR: Invalid voltage");
    return;
  }
  
  int adcValue = analogRead(VOLT_PIN);
  float voltageAtPin = (adcValue * V_REF) / ADC_RES;
  calibrationFactor = realVoltage / voltageAtPin;
  
  Serial.print("ADC raw: ");
  Serial.println(adcValue);
  Serial.print("Voltage at pin: ");
  Serial.print(voltageAtPin, 4);
  Serial.println("V");
  Serial.print("Calculated factor: ");
  Serial.println(calibrationFactor, 4);
  
  saveCalibration(calibrationFactor);
  Serial.println("Calibration complete. Restart the board.");
  while(1);
}