#include "types.h"

#define DEBUG true

#define DEVICE_ID 1
#define DEVICE_TYPE NRF_PROXY

#define PING_INTERVAL 30 // Seconds

uint8_t tx_address[6] = "rxadd";
uint8_t rx_address[6] = "txadd";

bool sendPayload(const EventType event_type, StateData state_data_var[8] = {});

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  if (DEBUG) {
      Serial.begin(115200);
      while (!Serial) {
          digitalWrite(LED_BUILTIN, HIGH);
          delay(100);
          digitalWrite(LED_BUILTIN, LOW);
          delay(100);
        }

        Serial.println("Starting nrf-proxy-radio");
  }

  radioSetup();
}

void handleSetStateEvent(StateData data[]) {

}

void handleGetStateResponseEvent(StateData data[]) {

}

StateData *handleGetStateRequestEvent() {

}

unsigned long last_ping = millis();
unsigned long last_state_test = millis();
bool last_state = true;

void loop() {
    unsigned long current_millis = millis();
    if (current_millis - last_ping >= 1000*PING_INTERVAL) {
        if (DEBUG) {
            Serial.println("Sending ping command!");
        }

        sendPayload(PING);
        last_ping = current_millis;
    }

    if (current_millis - last_state_test >= 5000) {
        if (DEBUG) {
            Serial.println("Sending State test!");
        }

        last_state = !last_state;
        StateData state_payload[1];
        state_payload[0] = {ON_OFF, last_state};
        sendPayload(SET_STATE, state_payload);
        last_state_test = current_millis;
    }
}
