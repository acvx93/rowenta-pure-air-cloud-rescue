// Transparent bridge between the PC's USB and the purifier board's UART (9600 8N1).
//
// Everything arriving over USB is forwarded to the board and vice versa, with one exception:
// a byte other than 0xFF arriving after >=100 ms without data from the PC is a command for the
// bridge itself (Gizwits frames always start with 0xFF):
//   'T'  reports the level of the test pins (with pull-up): "#PINS 4=x 5=y#"
//   'A'  measures their voltage in mV with the ADC and the internal pull-up: "#MV 4=x 5=y#"
//        (real ground ~0 mV; a resistor to ground reads hundreds of mV)
#include <Arduino.h>
#include <driver/rtc_io.h>

constexpr int PIN_TX = 17;  // to CN6 RXD (board input), through the level shifter
constexpr int PIN_RX = 18;  // from CN6 TXD (board output), through the level shifter
constexpr int TEST_PINS[] = {4, 5};
constexpr uint32_t COMMAND_GAP_MS = 100;

HardwareSerial &board = Serial1;
uint32_t lastHostByte = 0;

void reportTestPins() {
  Serial.print("#PINS");
  for (int pin : TEST_PINS) Serial.printf(" %d=%d", pin, digitalRead(pin));
  Serial.print("#\n");
}

void measureTestPins() {
  Serial.print("#MV");
  for (int pin : TEST_PINS) {
    analogReadMilliVolts(pin);  // sets the pin up as analog (removes the digital pull-up)
    rtc_gpio_pullup_en((gpio_num_t)pin);
    rtc_gpio_pulldown_dis((gpio_num_t)pin);
    delay(5);
    uint32_t total = 0;
    for (int i = 0; i < 16; i++) total += analogReadMilliVolts(pin);
    Serial.printf(" %d=%lu", pin, (unsigned long)(total / 16));
    rtc_gpio_pullup_dis((gpio_num_t)pin);
    pinMode(pin, INPUT_PULLUP);
  }
  Serial.print("#\n");
}

void setup() {
  Serial.begin(115200);
  board.begin(9600, SERIAL_8N1, PIN_RX, PIN_TX);
  for (int pin : TEST_PINS) pinMode(pin, INPUT_PULLUP);
}

void loop() {
  while (Serial.available()) {
    int b = Serial.read();
    uint32_t now = millis();
    bool afterGap = now - lastHostByte >= COMMAND_GAP_MS;
    lastHostByte = now;
    if (afterGap && b == 'T') {
      reportTestPins();
    } else if (afterGap && b == 'A') {
      measureTestPins();
    } else if (!afterGap || b == 0xFF) {
      board.write(b);
    }
  }
  while (board.available()) Serial.write(board.read());
}
