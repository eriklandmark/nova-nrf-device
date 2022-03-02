#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "types.h"
#include "printf.h"
#include <EEPROM.h>

#define DEBUG true

#define DEVICE_ID 2
#define DEVICE_TYPE REPEATER

#define PING_INTERVAL 60000 // Seconds

#define NETWORK_CHANNEL 1
#define NETWORK_SPEED RF24_250KBPS
#define RADIO_POWER RF24_PA_MAX

RF24 radio(2, 3);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    if (DEBUG) {
        Serial.begin(9600);
        while (!Serial) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
        Serial.println("[INFO] Starting nrf-device!");
    }

    mesh.setNodeID(DEVICE_ID);
    mesh.begin(NETWORK_CHANNEL, NETWORK_SPEED);
    radio.setPALevel(RADIO_POWER);

    if (!radio.isChipConnected()) {
        if (DEBUG) {
            Serial.println("[ERROR] Radio is not connected!");
        }
    } else {
        if (DEBUG) {
            //printf_begin();
            //radio.printPrettyDetails();
            Serial.println("[INFO] Radio is connected!");
        }
    }
}

bool rejoinNetwork(const bool full) {
    if (!mesh.checkConnection()) {
        if (DEBUG) {
            Serial.println("[INFO] Renewing address..");
        }

        if(!mesh.renewAddress()){
            if (DEBUG) {
                Serial.println("[ERROR] Failed to renew address");
            }

            if (full) {
                mesh.begin();
                return true;
            } else {
                return false;
            }
        } else {
            return true;
        }
    } else {
        if (DEBUG) {
            Serial.println("[INFO] Connection to network ok!");
        }
        return true;
    }
}

bool sendPayload(uint16_t node, const EventType event_type) {
    DataPayload payload = {(byte) DEVICE_ID, (byte) DEVICE_TYPE, (byte) event_type};

    for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
        payload.data[i] = {(byte) NO_STATE, 0};
    }

    bool result = mesh.write(&payload, 'M', sizeof(payload), node);
    if (!result) {
        if (DEBUG) {
            Serial.print("[ERROR] Error sending event: ");
            Serial.println(payload.event);
        }

        rejoinNetwork(false);
    } else {
        if (DEBUG) {
            Serial.println("[INFO] Send OK!");
        }
    }

    return result;
}

unsigned long last_ping = millis();
unsigned long last_pong = millis();
bool received_pong = true;

void loop() {
    mesh.update();

    unsigned long current_millis = millis();

    if(current_millis - last_ping > PING_INTERVAL && received_pong){
        last_ping = current_millis;

        if (!radio.isChipConnected()) {
            if (DEBUG) {
                Serial.println("[ERROR] Radio is not connected!");
            }
        } else if (!mesh.checkConnection()) {
            if (DEBUG) {
                Serial.println("[ERROR] Connection to network not ok!");
            }
            rejoinNetwork(false);
        } else {
            if (DEBUG) {
                Serial.println("[EVENT] Sending ping!");
            }
            received_pong = !sendPayload(0, PING);
            last_pong = current_millis;
        }
    }

    if(current_millis - last_pong >= 2000 && !received_pong) {
        last_pong = current_millis;
        if (DEBUG) {
            Serial.println("[ERROR] PONG TIMEOUT! Trying to renew address!");
        }

        received_pong = rejoinNetwork(false);
    }

    while (network.available()) {
        RF24NetworkHeader header;
        DataPayload result;
        network.read(header, &result, sizeof(result));

        if (result.event == PING) {
            if (DEBUG) {
                Serial.print("[EVENT] - PING - UID: ");
                Serial.println(result.uid);
            }
            sendPayload(result.uid, PONG);
        } else if (result.event == PONG) {
            if (DEBUG) {
                Serial.print("[EVENT] - PONG - UID: ");
                Serial.println(result.uid);
            }
            received_pong = true;
        } else if (result.event == GET_STATE) {
            if (DEBUG) {
                Serial.print("[EVENT] - GET_STATE_REQUEST - UID: ");
                Serial.println(result.uid);
            }
            sendPayload(result.uid, GET_STATE);
        }
    }
}
