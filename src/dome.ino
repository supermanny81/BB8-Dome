#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "BluefruitConfig.h"
#include <Adafruit_NeoPixel_ZeroDMA.h>

#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
  // Required for Serial on Zero based boards
  #define Serial SERIAL_PORT_USBVIRTUAL
#endif

#define PIN A5
#define N_LEDS 9
#define VBATPIN A7
#define DOME_ID "BB8 Dome (3efdd90)"

Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);
Adafruit_NeoPixel_ZeroDMA strip(N_LEDS, PIN, NEO_GRB);

enum class RenderMethod {flicker, fade};

struct COLORS {
  uint32_t RED = strip.Color(0, 255, 0);
  uint32_t WHITE = strip.Color(255, 255, 255);
  uint32_t LIGHT_BLUE =  strip.Color(110, 110, 200);
  uint32_t BLUE = strip.Color(35, 35, 245);
  uint32_t DEEP_BLUE = strip.Color(0, 0, 255);
  uint32_t OFF = 0;
} colors;

struct LEDS {
  byte position;
  byte size;
  uint32_t color;
  bool enabled;
  uint32_t last_update;
  RenderMethod method;
};

struct DOME_LIGHTS {
  LEDS HP = (LEDS){.position=0, .size=1,.color=colors.WHITE, .enabled=false};
  LEDS EYE = (LEDS){.position=1, .size=1, .color=colors.RED, .enabled=true};
  LEDS PSI = (LEDS){.position=2, .size=1, .color=colors.WHITE, .enabled=false};
  LEDS SMALL_LOGIC = (LEDS){.position=7, .size=2, .color=colors.BLUE, .enabled=true};
  LEDS LARGE_LOGIC_TOP = (LEDS){.position=5, .size=2, .color=colors.LIGHT_BLUE, .enabled=true};
  LEDS LARGE_LOGIC_BOTTOM = (LEDS){.position=3, .size=2, .color=colors.LIGHT_BLUE, .enabled=false};
} dome_lights;

struct BATTERY {
  float level = 0.0;
  byte percent = 0;
} battery;

void checkBattery(){
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  battery.level = measuredvbat;
  measuredvbat = constrain(measuredvbat * 100, 370, 420);
  battery.percent = map(measuredvbat, 370, 420, 0, 100);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F(DOME_ID));
  Serial.println(F("---------------------------------------"));

  strip.begin();
  strip.setBrightness(255);
  strip.show(); // Initialize all pixels to 'off'

  ble.begin(false);
  ble.echo(false);
  ble.println("AT+GAPDEVNAME=" DOME_ID); // set peripheral name
  ble.println("ATZ");
}

void render() {
  setLEDs(dome_lights.HP);
  setLEDs(dome_lights.EYE);
  setLEDs(dome_lights.PSI);
  setLEDs(dome_lights.SMALL_LOGIC);
  setLEDs(dome_lights.LARGE_LOGIC_TOP);
  setLEDs(dome_lights.LARGE_LOGIC_BOTTOM);
}

/** Overload the method here, default arguments don't work in the *.ino
*/
void setLEDs(LEDS leds) {
  setLEDs(leds, true);
}

void setLEDs(LEDS leds, bool enabled) {
  for (byte i=0; i < leds.size; i++) {
    byte pos = leds.position + i;
    if (leds.enabled) {
      strip.setPixelColor(pos, leds.color);
    } else {
      strip.setPixelColor(pos, colors.OFF);
    }
  }
  strip.show();
}

void handleBLE() {
  ble.print("AT+BLEUARTTX=");
  ble.print(battery.level);ble.print(',');ble.print(battery.percent);
  ble.println("\\n");
  // check response stastus
  if (! ble.waitForOK() ) {
    Serial.println(F("Failed to send?"));
  }

  // Check for incoming characters from Bluefruit
  ble.println("AT+BLEUARTRX");
  ble.readline();
  if (strcmp(ble.buffer, "OK") == 0) {
    // no data
    return;
  }
  // Some data was found, its in the buffer
  Serial.print(F("[Recv] ")); Serial.println(ble.buffer);
  ble.waitForOK();
}

void loop() {
  if (ble.isConnected()) {
    dome_lights.EYE.enabled = true;
    flicker();
    checkBattery();
    handleBLE();
  } else {
    delay(500);
    dome_lights.EYE.enabled = !dome_lights.EYE.enabled;
    dome_lights.PSI.enabled = false;
  }
  render();
}

void flicker() {
  dome_lights.PSI.enabled = !dome_lights.PSI.enabled;
  delay(random(200));
}
