#include <SPI.h>
#include "printf.h"
#include "RF24.h"

#define RADIO_CHANNEL 100
#define CE_PIN 2
#define CSN_PIN 3
#define IRQ_PIN 7
#define RADIO_POWER RF24_PA_MIN

RF24 radio(CE_PIN, CSN_PIN);

unsigned long latest_time = millis();


void radioSetup() {
  if (!radio.begin()) {
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }

  pinMode(IRQ_PIN, INPUT);
  pinMode(CSN_PIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), radioEvent, FALLING);

  radio.setPALevel(RADIO_POWER);
  radio.setChannel(100);
  radio.setAutoAck(true);
  radio.setPayloadSize(28);
  radio.maskIRQ(true, false, false);

  radio.openWritingPipe(tx_address);
  radio.openReadingPipe(1, rx_address);
  radio.startListening();

  radio.flush_tx();
  radio.flush_rx();

  if (DEBUG) {
    printf_begin();
    radio.printPrettyDetails();
  }
}

bool sendPayload(const EventType event_type, StateData state_data_var[] = {}) {
    if (radio.isChipConnected()) {
        radio.stopListening();

        DataPayload payload = {DEVICE_ID, DEVICE_TYPE, event_type};
        byte state_packets = int(sizeof(*state_data_var) / sizeof(StateData));

        for (short i = 0; i < state_packets; i++) {
            payload.state_data[i] = state_data_var[i];
        }

        for (short i = state_packets; i < 8; i++) {
            payload.state_data[i] = {0,0};
        }

        bool result = radio.write(&payload, sizeof(payload));

        radio.startListening();
        return result;
    } else {
        if (DEBUG) {
            Serial.println("[ERROR] - RADIO - Radio is not connected!!");
        }
        return false;
    }
}

void radioEvent() {
    bool tx_ds, tx_df, rx_dr;
    radio.whatHappened(tx_ds, tx_df, rx_dr);

    if (tx_df) {
        if (DEBUG) {
            Serial.println("[ERROR] - RADIO - Error sending payload!!");
        }
        radio.flush_tx();
    } else if (rx_dr) {
        if (radio.available()) {
            DataPayload result;
            radio.read(&result, sizeof(result));

            if (result.event == PING) {
                sendPayload(PONG);
                if (DEBUG) {
                    Serial.print("[EVENT] - PING - UID: ");
                    Serial.println(result.uid);
                }
            } else if (result.event == PONG) {
                if (DEBUG) {
                    Serial.print("[EVENT] - PONG - UID: ");
                    Serial.println(result.uid);
                }
            } else if (result.event == SET_STATE) {
                handleSetStateEvent(result.state_data);
                if (DEBUG) {
                    Serial.print("[EVENT] - SET_STATE - UID: ");
                    Serial.println(result.uid);
                }
            } else if (result.event == GET_STATE) {
                bool has_data = false;
                for (short i = 0; i < 8; i++) {
                    if (result.state_data[i].state_type != NO_STATE) {
                        has_data = true;
                        break;
                    }
                }
                if (has_data) {
                    // GET_STATE Response:
                    handleGetStateResponseEvent(result.state_data);
                    if (DEBUG) {
                        Serial.print("[EVENT] - GET_STATE_RESPONSE - UID: ");
                        Serial.println(result.uid);
                    }
                } else {
                    // GET_STATE Request:
                    sendPayload(GET_STATE, handleGetStateRequestEvent());
                    if (DEBUG) {
                        Serial.print("[EVENT] - GET_STATE_REQUEST - UID: ");
                        Serial.println(result.uid);
                    }
                }
            }
        }
    }
}