#include <stdlib.h>
#include <SPI.h> 
#include <Ethernet.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <Bounce2.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <SimpleTimer.h>

#include "warpcore2.h"


const uint16_t debounceTimeoutMs = 100;
uint8_t currentMode = MODE_WARPCORE;
unsigned long mqttConnectNextTryMillis = 0;

char numberConversionBuffer[10] = {0};

boolean doorStatusOpen = false;
uint8_t lastMemberCount = 0;

CRGB leds[NUM_LEDS];
uint8_t speed = 2;

struct WatchConfig pinWatch[] = {
  {7, MQTT_TOPIC_RACK_CONTACT_SENSOR, genericPinWatchCallback, NULL}
};

struct OneWireTemperatureSensor temperatureSensors[] = {
  { {0x10, 0x39, 0x2D, 0xCF, 0x02, 0x08, 0x00, 0x0E}, MQTT_TOPIC_TEMPERATURE_TEST }
};


EthernetClient ethClient;
PubSubClient mqttClient("mqtt.core.bckspc.de", 1883, mqttMessageReceived, ethClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

SimpleTimer timer;
  
uint8_t offset = 0;

void setup() {

  Serial.begin(115200);
  Ethernet.begin(mac);

  timer.setInterval(1000 / UPDATES_PER_SECOND, updateLeds);
  timer.setInterval(300000, readOneWireTemperatures);
  
  sensors.begin();
  mqttClientConnect();
  
  for (uint8_t i = 0; i < ARRAY_LEN(pinWatch); i++) {
     pinMode(pinWatch[i].pin, INPUT);
     digitalWrite(pinWatch[i].pin, HIGH);
     
     pinWatch[i].debouncer = new Bounce();
     pinWatch[i].debouncer->attach(pinWatch[i].pin);
     pinWatch[i].debouncer->interval(debounceTimeoutMs);
  }

  delay(500); // power-up safety delay
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS_FULL);

  Serial.println("Okay!");
}

float map_range(float val, float min1, float max1, float min2, float max2) {
    return min2 + (max2 - min2) * ((val - min1) / (max1 - min1));
}


DeviceAddress deviceAddress;

volatile unsigned long currentMillis = millis();

void loop() {

  timer.run();
  
  currentMillis = millis();
    
  if (!mqttClient.loop()) {
    mqttClientConnect();
  }

  for (uint8_t i = 0; i < ARRAY_LEN(pinWatch); i++) {
    pinWatch[i].debouncer->update();
    
    if(pinWatch[i].callback != NULL && (pinWatch[i].debouncer->fell() || pinWatch[i].debouncer->rose())) {
      pinWatch[i].callback(&pinWatch[i], pinWatch[i].debouncer->read());
    }
  }

  //FastLED.delay(1000 / UPDATES_PER_SECOND);
}


void genericPinWatchCallback(WatchConfig* t, uint8_t state) {
  Serial.println(t->mqttTopic);
  Serial.println(t->pin);
  Serial.println((state == HIGH)? "HIGH" : "LOW");
  
  if(strcmp(t->mqttTopic, MQTT_TOPIC_RACK_CONTACT_SENSOR) == 0) {
    currentMode = (state == HIGH) ? MODE_BRIGHT_WHITE_LIGHT : MODE_WARPCORE;
    mqttPublish(t->mqttTopic, (state == HIGH)? SENSOR_VALUE_OPEN : SENSOR_VALUE_CLOSED, true);
  } else {
    mqttPublish(t->mqttTopic, (state == HIGH)? "HIGH" : "LOW", true);
  }  
}

void ledWarpcore() {
  
  offset += speed;

  CHSV color;

  for(uint16_t i = 0; i < LEDS_PER_SIDE; i++) {
    uint8_t phase = quadwave8(i * 8 + offset);
    uint8_t hue = (uint8_t) map_range(phase, 0, 255, 105, 170);
    uint8_t volume = (uint8_t) map_range(hue, 105, 170, 210, 50);

    color = CHSV(hue, 255, volume);
    
    leds[i] = color;
    leds[i + LEDS_PER_SIDE] = color;
  }
  
}

void ledRackOpened() {
  for(uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::White;
  }
}

void ledAlarm() {
  offset += 1;
  
  uint8_t phase = quadwave8(offset);
  phase = map_range(phase, 0, 255, 60, 255);

  CHSV color = CHSV(5, 255, phase);
  for(uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
}


void mqttClientConnect() {

  if(millis() < mqttConnectNextTryMillis) {
    return;
  }

  Serial.print("MQTT disconnected...");
  
  if (mqttClient.connect("WarpCore2")) {
    
    mqttConnectNextTryMillis = 0;
    mqttClient.subscribe(MQTT_TOPIC_WARPCORE_SPEED);
    mqttClient.subscribe(MQTT_TOPIC_MEMBER_COUNT);
    mqttClient.subscribe(MQTT_TOPIC_DOOR_LOCK);
    mqttClient.subscribe(MQTT_TOPIC_ALARM);
  } else {
    mqttConnectNextTryMillis = millis() + 30000;
  }
}

void mqttPublish(const char* topic, const char* payload, boolean retain) {
  mqttClient.publish(topic, (const uint8_t*) payload, strlen(payload), retain);
}

void mqttMessageReceived(char* topic, byte* payload, unsigned int length) {

  Serial.print("MQTT topic: ");
  Serial.print(topic);
  Serial.print(" - ");

  String mssg = String();
  
  for(uint8_t i = 0; i < length; i++) {
    mssg += (char) payload[i];
  }

  Serial.println(mssg);

  if(str_equals(topic, MQTT_TOPIC_ALARM)) {
    currentMode = MODE_ALARM;
    timer.setTimeout(10 * 1000, restoreLedAnimation);
      
  } else if(str_equals(topic, MQTT_TOPIC_DOOR_LOCK)) {
    if(mssg == SENSOR_VALUE_OPEN) {
      doorStatusOpen = true;
      FastLED.setBrightness(BRIGHTNESS_FULL);
    } else {
      doorStatusOpen = false;
      FastLED.setBrightness((lastMemberCount == 0)? BRIGHTNESS_OFF : BRIGHTNESS_DIMMED);
    }

  } else if(str_equals(topic, MQTT_TOPIC_MEMBER_COUNT)) {

    lastMemberCount = mssg.toInt();
    if(lastMemberCount == 0) {
      FastLED.setBrightness(BRIGHTNESS_OFF);
    } else {
      FastLED.setBrightness((doorStatusOpen)? BRIGHTNESS_FULL : BRIGHTNESS_DIMMED);
    }
    
  } else if(str_equals(topic, MQTT_TOPIC_WARPCORE_SPEED)) {
    speed = (uint8_t) mssg.toInt();
  }
}

void readOneWireTemperatures() {
  /*
    uint8_t numberOfDevices = sensors.getDeviceCount();

    Serial.print("# of OneWire devices: ");
    Serial.println(numberOfDevices);
    
    for(uint8_t i = 0 ;i < numberOfDevices; i++) {
      Serial.print("Querying device ");
      Serial.print(i);
      Serial.print(" for address ... ");
      
      if(sensors.getAddress(deviceAddress, i)){
          for (uint8_t j = 0; j < 8; j++) {
            if (deviceAddress[j] < 16) Serial.print("0");
            Serial.print(deviceAddress[j], HEX);
          }
      } else {
        Serial.print("FAIL!");
      }

      Serial.println();
      
    }
*/

    Serial.println("Reading onewire");
    
    sensors.requestTemperatures();
    for (uint8_t i = 0; i < ARRAY_LEN(temperatureSensors); i++) {
      float temperature = sensors.getTempC(temperatureSensors[i].deviceAddress);
      dtostrf(temperature, 2, 2, numberConversionBuffer);
      mqttPublish(temperatureSensors[i].mqttTopic, numberConversionBuffer, true);
    }

    Serial.print("Published temperatures ");
    Serial.println(millis());
}

void updateLeds() {
  if(currentMode == MODE_BRIGHT_WHITE_LIGHT) {
    ledRackOpened();
  } else if(currentMode == MODE_ALARM) {
    ledAlarm();
  } else {
    ledWarpcore();
  }
  
  FastLED.show();
}

void restoreLedAnimation() {
  currentMode = MODE_WARPCORE;
}

