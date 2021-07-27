#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "types.h"
#include "printf.h"
#include <EEPROM.h>

#define DEBUG false

#define DEVICE_ID 2
#define DEVICE_TYPE IRRIGATION_STATION
#define OUTPUT_PIN LED_BUILTIN

#define NETWORK_CHANNEL 100
RF24 radio(2, 3);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

void setup() {
    pinMode(9, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);

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

    pinMode(OUTPUT_PIN, OUTPUT);

    mesh.setNodeID(1);
    mesh.begin(NETWORK_CHANNEL, RF24_250KBPS);
    radio.setPALevel(RF24_PA_MIN);

    if (!radio.isChipConnected()) {
        if (DEBUG) {
            Serial.println("Radio is not connected!");
        }
    } else {
        if (DEBUG) {
            printf_begin();
            radio.printPrettyDetails();
        }
    }
}

bool sendPayload(uint16_t node, const EventType event_type, StateData state_data_var[] = {}) {
    DataPayload payload = {DEVICE_ID, DEVICE_TYPE, event_type};
    byte state_packets = int(sizeof(*state_data_var) / sizeof(StateData));

    for (short i = 0; i < state_packets; i++) {
        payload.state_data[i] = state_data_var[i];
    }

    for (short i = state_packets; i < NUM_STATE_DATA_PACKETS; i++) {
        payload.state_data[i] = {0,0};
    }

    bool result = mesh.write(&payload, 'M', sizeof(payload), node);
    if (!result) {
        if (!mesh.checkConnection()) {
            if (DEBUG) {
                Serial.println("Renewing Address");
            }

            if(!mesh.renewAddress()){
                mesh.begin();
            }
        } else {
            if (DEBUG) {
                Serial.println("Send fail, Connection OK");
            }
        }
    }

    return result;
}

unsigned long last_radio_check = millis();
unsigned long last_led_blink = millis();
bool led_state = false;

void loop() {
    mesh.update();

    unsigned long current_millis = millis();

    if(current_millis - last_radio_check >= 5000){
        last_radio_check = current_millis;

        if (!radio.isChipConnected()) {
            if (DEBUG) {
                Serial.println("Radio is not connected!");
            }
        }
    }

    if(current_millis - last_led_blink >= 1000){
        last_led_blink = current_millis;
        led_state = !led_state;
        digitalWrite(LED_BUILTIN, led_state);
    }

    while (network.available()) {
        if (DEBUG) {
            Serial.println("Recieved data!");
        }

        RF24NetworkHeader header;
        DataPayload result;
        network.read(header, &result, sizeof(result));

        if (result.event == PING) {
            if (DEBUG) {
                Serial.print("[EVENT] - PING - UID: ");
                Serial.println(result.uid);
            }
            sendPayload(0, PONG);
        } else if (result.event == SET_STATE) {
            for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                if (result.state_data[i].type != NO_STATE) {
                    if (DEBUG) {
                        Serial.print("STATE: ");
                        Serial.print(result.state_data[i].type);
                        Serial.print(":");
                        Serial.println(result.state_data[i].data);
                    }
                    if (result.state_data[i].type == ON_OFF) {
                        digitalWrite(OUTPUT_PIN, result.state_data[i].data);
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
                if (result.state_data[i].type != NO_STATE) {
                    has_data = true;
                    break;
                }
            }
            if (!has_data) {
                // GET_STATE Request:
                short state = digitalRead(OUTPUT_PIN);
                StateData state_data[1];
                state_data[0] = {1, state};
                sendPayload(0, GET_STATE, state_data);
                if (DEBUG) {
                    Serial.print("[EVENT] - GET_STATE_REQUEST - UID: ");
                    Serial.println(result.uid);
                }
            }
        }
    }
}
