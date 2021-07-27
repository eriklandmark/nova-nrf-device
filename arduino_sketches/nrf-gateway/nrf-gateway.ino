#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "types.h"
#include <EEPROM.h>

#define DEBUG false

#define DEVICE_ID 1
#define DEVICE_TYPE GATEWAY
#define PING_INTERVAL 15 // Seconds

#define NETWORK_CHANNEL 100

RF24 radio(2, 3);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial1.begin(9600);

    if (DEBUG) {
        Serial.begin(9600);
        while (!Serial) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }

        Serial.println("Starting nrf-proxy-radio");
    }

    if (DEBUG) {
        Serial.println("Setting up radio.");
    }

    Serial1.println("Starting up!");

    mesh.setNodeID(0);
    mesh.begin(NETWORK_CHANNEL, RF24_250KBPS);

    while (!Serial) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
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
        Serial1.print("Error sending to node: ");
        Serial1.println(node);
    }

    return result;
}

void sendEventToPi(DataPayload payload) {
    Serial1.print(payload.uid);
    Serial1.print(":");
    Serial1.print(payload.event);
    Serial1.print(":");
    for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
        Serial1.print(payload.state_data[i].type);
        Serial1.print("|");
        Serial1.print(payload.state_data[i].data);
        Serial1.print(",");
    }
    Serial1.println("");
}

unsigned long last_ping = millis();
unsigned long last_state_test = millis();
unsigned long last_display_nodes = millis();
bool last_state = true;

String msg = "";
bool msg_done = false;

void loop() {
    mesh.update();
    mesh.DHCP();

    while (network.available()){
        RF24NetworkHeader header;
        network.peek(header);

        if (header.type == 'M') {
            DataPayload result;
            network.read(header, &result, sizeof(result));

            if (result.event == PONG) {
                if (DEBUG) {
                    Serial.print("[EVENT] - PONG - UID: ");
                    Serial.println(result.uid);
                }
                sendEventToPi(result);
            } else if (result.event == GET_STATE) {
                if (DEBUG) {
                    Serial.print("[EVENT] - GET_STATE - UID: ");
                    Serial.println(result.uid);
                }

                bool has_data = false;
                for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                    if (result.state_data[i].type != NO_STATE) {
                        has_data = true;
                        break;
                    }
                }

                if (has_data) {
                    // GET_STATE Response:
                    sendEventToPi(result);
                }
            }
        } else {
            network.read(header, 0, 0);

            if (DEBUG) {
                Serial.print("[EVENT] - UNKNOWN - ");
                Serial.println(header.type);
            }
        }
    }

    while (Serial1.available() > 0) {
        char c = Serial1.read();

        if (c == '<') {
            msg_done = false;
        } else if (c == '>') {
            msg_done = true;
        } else {
            msg += c;
        }
    }

    if (msg.length() > 0 && msg_done) {
        Serial1.print("Got from serial: ");
        Serial1.println(msg);
        msg = "";
    }

    unsigned long current_millis = millis();

    if(current_millis - last_display_nodes >= 10000){
        last_display_nodes = current_millis;

        Serial1.println(F("**NODES**"));
        for(int i = 0; i< mesh.addrListTop; i++){
            Serial1.print("Node: ");
            Serial1.print(mesh.addrList[i].nodeID);
            Serial1.print(" : ");
            Serial1.println(mesh.addrList[i].address, OCT);
        }
        Serial1.println(F("*********"));
    }

    if (current_millis - last_ping >= 1000 * PING_INTERVAL) {
        last_ping = current_millis;

        if (mesh.addrListTop > 0) {
            if (DEBUG) {
                Serial.println("Sending ping command!");
            }

            Serial1.println("Sending ping command!");
            for(int i = 0; i< mesh.addrListTop; i++){
                sendPayload((uint16_t) mesh.addrList[i].nodeID, PING);
            }
        }
    }
}
