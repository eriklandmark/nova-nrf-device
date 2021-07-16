#include "types.h"

#define DEBUG true

#define DEVICE_ID 2
#define DEVICE_TYPE OUTLET

#define OUTPUT_PIN LED_BUILTIN

uint8_t tx_address[6] = "txadd";
uint8_t rx_address[6] = "rxadd";

void setup() {
  pinMode(9, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  if (DEBUG) {
    Serial.begin(115200);
    while (!Serial) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
    Serial.println("Starting nrf-radio in debug");
  }

  radioSetup();

  pinMode(OUTPUT_PIN, OUTPUT);
}

void handleSetStateEvent(StateData data[8]) {
    for (short i = 0; i < 8; i++) {
        if (data[i].state_type != NO_STATE) {
            if (DEBUG) {
                Serial.print("STATE: ");
                Serial.print(data[i].state_type);
                Serial.print(":");
                Serial.println(data[i].state_data);
            }
            if (data[i].state_type == ON_OFF) {
                digitalWrite(OUTPUT_PIN, data[i].state_data);
            }
        }
    }
}

StateData *handleGetStateRequestEvent() {
    short state = digitalRead(OUTPUT_PIN);
    StateData state_data[1];
    state_data[0] = {1, state};
    return state_data;
}

void handleGetStateResponseEvent(StateData data[]) {}
void loop() {}
