#include <Bounce2.h>

static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

typedef struct WatchConfig WatchConfig;

struct WatchConfig {
  uint8_t pin;
  const char* mqttTopic;
  void (*callback)(WatchConfig*, uint8_t);
  Bounce *debouncer;
};

struct OneWireTemperatureSensor {
  DeviceAddress deviceAddress;
  const char* mqttTopic;
};

struct TimeSchedule {
  uint8_t id;
  unsigned long previousMillis;
  unsigned long intervalMillis;
};


#define TIME_SCHEDULE_EVERY_SECOND 0
#define TIME_SCHEDULE_EVERY_MINUTE 1
#define TIME_SCHEDULE_LED_UPDATE   2

#define ARRAY_LEN(a) sizeof(a)/sizeof(a[0])

#define LED_PIN     3
#define NUM_LEDS    216 //108
#define LEDS_PER_SIDE 108
#define BRIGHTNESS  200
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 90

#define ONE_WIRE_BUS 2

#define MODE_WARPCORE 0
#define MODE_BRIGHT_WHITE_LIGHT 1
#define MODE_ALARM 2

const uint16_t SECOND = 1000;
const uint16_t MINUTE = (60 * SECOND);
const uint16_t HOUR   = (60 * MINUTE);

const char MQTT_TOPIC_WARPCORE_SPEED = "tools/warpcore/speed";
const char MQTT_TOPIC_MEMBER_COUNT = "sensor/space/member/count";
const char MQTT_TOPIC_ALARM = "psa/alarm";
const char MQTT_TOPIC_RACK_CONTACT_SENSOR = "sensor/rack/door";
const char MQTT_TOPIC_TEMPERATURE_TEST = "sensor/temperature/misc/test";

