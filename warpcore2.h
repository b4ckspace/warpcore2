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
#define NUM_LEDS    108
#define BRIGHTNESS  40
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 100

#define ONE_WIRE_BUS 2

#define MODE_WARPCORE 0
#define MODE_BRIGHT_WHITE_LIGHT 1

