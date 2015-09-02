#include <stdlib.h>
#include <SPI.h> 
#include <Ethernet.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <Bounce2.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include "warpcore2.h"


const uint16_t debounceTimeoutMs = 100;
uint8_t currentMode = MODE_WARPCORE;
unsigned long mqttConnectNextTryMillis = 0;

char numberConversionBuffer[10] = {0};

CRGB leds[NUM_LEDS];
uint8_t speed = 2;

struct WatchConfig pinWatch[] = {
  {5, "sensor/rack/door", genericPinWatchCallback, NULL}
};

struct OneWireTemperatureSensor temperatureSensors[] = {
  { {0x10, 0x39, 0x2D, 0xCF, 0x02, 0x08, 0x00, 0x0E}, "sensor/temperature/misc/test" }
};

struct TimeSchedule schedule[] = {
  { TIME_SCHEDULE_LED_UPDATE, .previousMillis = 0, .intervalMillis = (1000 / UPDATES_PER_SECOND) },
  { TIME_SCHEDULE_EVERY_SECOND, .previousMillis = 0, .intervalMillis = 1000 },
  { TIME_SCHEDULE_EVERY_MINUTE, .previousMillis = 0, .intervalMillis = 1000 * 60 },
};

EthernetClient ethClient;
PubSubClient mqttClient("mqtt.core.bckspc.de", 1883, mqttMessageReceived, ethClient);


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


uint8_t offset = 0;

void setup() {

  Serial.begin(115200);
  Ethernet.begin(mac);

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
  FastLED.setBrightness(BRIGHTNESS);

  Serial.println("Okay!");
}

float map_range(float val, float min1, float max1, float min2, float max2) {
    return min2 + (max2 - min2) * ((val - min1) / (max1 - min1));
}


DeviceAddress deviceAddress;

volatile unsigned long currentMillis = millis();

void loop() {

  currentMillis = millis();
    
  if (!mqttClient.loop()) {
    mqttClientConnect();
  } 

  for (uint8_t i = 0; i < ARRAY_LEN(schedule); i++) {

    if ((currentMillis - schedule[i].previousMillis) >= schedule[i].intervalMillis) {
      
      switch(schedule[i].id) {
        case TIME_SCHEDULE_EVERY_SECOND:
          readOneWireTemperatures();
        break;

        case TIME_SCHEDULE_EVERY_MINUTE:
        break;

        case TIME_SCHEDULE_LED_UPDATE:
        
          if(currentMode == MODE_BRIGHT_WHITE_LIGHT) {
            ledRackOpened();
          } else {
            ledWarpcore();
          }
          
          FastLED.show();
          
        break;
      }
      
    }
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
  
  if(strcmp(t->mqttTopic, "sensor/rack/door") == 0) {
    currentMode = (state == HIGH) ? MODE_BRIGHT_WHITE_LIGHT : MODE_WARPCORE;
    mqttClient.publish(t->mqttTopic, (state == HIGH)? "OPEN" : "CLOSED");
  } else {
    mqttClient.publish(t->mqttTopic, (state == HIGH)? "HIGH" : "LOW");
  }  
}

void ledWarpcore() {
  
  offset += speed;

  for(uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t phase = quadwave8(i * 11 + offset);
    uint8_t hue = (uint8_t) map_range(phase, 0, 255, 120, 170);
    leds[i] = CHSV(hue, 255, 255);
  }
  
}

void ledRackOpened() {
  for(uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::White;
  }
}

void mqttClientConnect() {

  if(millis() < mqttConnectNextTryMillis) {
    return;
  }

  Serial.print("MQTT disconnected...");
  
  if (mqttClient.connect("foo")) {
    mqttConnectNextTryMillis = 0;
    mqttClient.subscribe("tools/warpcore/speed");
  } else {
    mqttConnectNextTryMillis = millis() + 30000;
  }
}

void mqttMessageReceived(char* topic, byte* payload, unsigned int length) {
  // handle message arrived

  Serial.print("MQTT topic: ");
  Serial.print(topic);
  Serial.print(" - ");

  String mssg = String();
  
  for(uint8_t i = 0; i < length; i++) {
    mssg += (char) payload[i];
  }

  Serial.println(mssg);

  if(length > 3) {
    // No numbers higher than 999 allowed.
    return;
  }
  
  speed = mssg.toInt();
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
    
    sensors.requestTemperatures();
    for (uint8_t i = 0; i < ARRAY_LEN(temperatureSensors); i++) {
      float temperature = sensors.getTempC(temperatureSensors[i].deviceAddress);
      dtostrf(temperature, 2, 2, numberConversionBuffer);
      mqttClient.publish(temperatureSensors[i].mqttTopic, numberConversionBuffer);
    }

    Serial.println("Published temperatures");
}
