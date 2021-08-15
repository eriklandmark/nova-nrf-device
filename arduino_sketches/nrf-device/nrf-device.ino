#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "types.h"
#include "printf.h"
#include <EEPROM.h>

#define DEBUG false
#define ACTIVATE_BLINK false

#define DEVICE_ID 1
#define DEVICE_TYPE IRRIGATION_STATION
#define OUTPUT_PIN 4

#define PING_INTERVAL 300000 // Seconds

#define NETWORK_CHANNEL 1
#define NETWORK_SPEED RF24_250KBPS
#define RADIO_POWER RF24_PA_MAX

RF24 radio(2, 3);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(OUTPUT_PIN, OUTPUT);

    if (DEBUG) {
        Serial.begin(9600);
        while (!Serial) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
        Serial.println("Starting nrf-device!");
    }

    mesh.setNodeID(DEVICE_ID);
    mesh.begin(NETWORK_CHANNEL, NETWORK_SPEED);
    radio.setPALevel(RADIO_POWER);

    if (!radio.isChipConnected()) {
        if (DEBUG) {
            Serial.println("Radio is not connected!");
        }
    } else {
        if (DEBUG) {
            //printf_begin();
            //radio.printPrettyDetails();
            Serial.println("Radio is connected!");
        }
    }
}

bool rejoinNetwork(const bool full) {
    if (!mesh.checkConnection()) {
        if (DEBUG) {
            Serial.println("[RADIO] Renewing Address");
        }

        if(!mesh.renewAddress()){
            if (DEBUG) {
                Serial.println("[RADIO] Failed to renew address");
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
            Serial.println("[RADIO] Connection to network ok!");
        }
        return true;
    }
}

bool sendPayload(uint16_t node, const EventType event_type, DataPacket state_data_var[] = {}, size_t num_packets = 0) {
    DataPayload payload = {DEVICE_ID, DEVICE_TYPE, event_type};

    for (short i = 0; i < num_packets; i++) {
        payload.data[i] = state_data_var[i];
    }

    for (short i = num_packets; i < NUM_STATE_DATA_PACKETS; i++) {
        payload.data[i] = {NO_STATE, 0};
    }

    bool result = mesh.write(&payload, 'M', sizeof(payload), node);
    if (!result) {
        if (DEBUG) {
            Serial.println("[RADIO] Renewing Address");
        }

        rejoinNetwork(true);
    } else {
        if (DEBUG) {
            Serial.println("Send OK!");
        }
    }

    return result;
}

size_t handleGetState(state_data) {
    if (DEVICE_TYPE == OUTLET) {
        state_data[0] = {ON_OFF, (short) digitalRead(OUTPUT_PIN)};
        return 1;
    } else if (DEVICE_TYPE == IRRIGATION_STATION) {
        return 0;
    } else {
        return 0;
    }
}


unsigned long last_ping = millis();
unsigned long last_pong = millis();
unsigned long last_led_blink = millis();
bool received_pong = true;
bool led_state = false;

void loop() {
    mesh.update();

    unsigned long current_millis = millis();

    if(current_millis - last_ping > PING_INTERVAL && received_pong){
        last_ping = current_millis;

        if (!radio.isChipConnected()) {
            if (DEBUG) {
                Serial.println("Radio is not connected!");
            }
        } else if (!mesh.checkConnection()) {
            if (DEBUG) {
                Serial.println("Connection to network not ok!");
            }
            rejoinNetwork(false);
        } else {
            if (DEBUG) {
                Serial.println("Sending ping!");
            }
            received_pong = !sendPayload(0, PING);
            last_pong = current_millis;
        }
    }

    if(current_millis - last_pong >= 1000 && !received_pong) {
        last_pong = current_millis;
        if (DEBUG) {
            Serial.println("PONG TIMEOUT! Trying to renew address!");
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
            sendPayload(0, PONG);
        } else if (result.event == PONG) {
           if (DEBUG) {
               Serial.print("[EVENT] - PONG - UID: ");
               Serial.println(result.uid);
           }
           received_pong = true;
        } else if (result.event == SET_STATE) {
            for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                if (result.data[i].type != NO_STATE) {
                    if (DEBUG) {
                        Serial.print("STATE: ");
                        Serial.print(result.data[i].type);
                        Serial.print(":");
                        Serial.println(result.data[i].data);
                    }
                    if (result.data[i].type == ON_OFF) {
                        digitalWrite(OUTPUT_PIN, result.data[i].data);
                        digitalWrite(LED_BUILTIN, result.data[i].data);

                        short state = digitalRead(OUTPUT_PIN);
                        DataPacket state_data[1];
                        state_data[0] = {ON_OFF, state};
                        sendPayload(0, GET_STATE, state_data, 1);
                    }
                }
            }

            if (DEBUG) {
                Serial.print("[EVENT] - SET_STATE - UID: ");
                Serial.println(result.uid);
            }
        } else if (result.event == GET_STATE) {
            bool has_data = false;
            for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                if (result.data[i].type != NO_STATE) {
                    has_data = true;
                    break;
                }
            }

            if (!has_data) {
                // GET_STATE Request:
                short state = digitalRead(OUTPUT_PIN);
                DataPacket state_data[NUM_STATE_DATA_PACKETS];
                size_t num_packets = handleGetState(state_data);
                sendPayload(0, GET_STATE, state_data, num_packets);
            }

            if (DEBUG) {
                Serial.print("[EVENT] - GET_STATE_REQUEST - UID: ");
                Serial.println(has_data);
            }
        }
    }

    if(current_millis - last_led_blink >= 1000 && ACTIVATE_BLINK){
        last_led_blink = current_millis;
        led_state = !led_state;
        digitalWrite(LED_BUILTIN, led_state);
    }
}
