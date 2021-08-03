#include "ArduinoJson.h"
#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "types.h"
#include <EEPROM.h>

#define DEBUG false

#define DEVICE_ID 0
#define DEVICE_TYPE GATEWAY

#define NETWORK_CHANNEL 100
#define NETWORK_SPEED RF24_250KBPS

#define OUTPUT_SERIAL Serial //Serial1

RF24 radio(2, 3);
RF24Network network(radio);
RF24Mesh mesh(radio, network);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    OUTPUT_SERIAL.begin(9600);

    if (DEBUG) {
        Serial.begin(9600);
        while (!Serial) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }

        Serial.println("Starting nrf-gateway");
    }

    if (DEBUG) {
        Serial.println("Setting up radio.");
    }

    mesh.setNodeID(DEVICE_ID);
    mesh.begin(NETWORK_CHANNEL, NETWORK_SPEED);

    if (DEBUG) {
        if (radio.isChipConnected()) {
            Serial.println("Radio is connected!");
        } else {
            Serial.println("Radio is not connected!");
            sendErrorToPi(RADIO_ERROR);
        }
    }
}

void sendEventToPi(DataPayload payload) {
    StaticJsonDocument<256> doc;
    doc["event"] = payload.event;
    doc["uid"] = payload.uid;
    JsonArray data = doc["data"].to<JsonArray>();
    if (payload.event == DEVICES) {
        for (short i = 0; i < mesh.addrListTop; i++) {
            JsonObject nested = data.createNestedObject();
            nested["type"] = DEVICE;
            nested["data"] = mesh.addrList[i].nodeID;
        }
    } else if (payload.event != PONG && payload.event != OK && payload.event != PING) {
        for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
            if (payload.data[i].type) {
                JsonObject nested = data.createNestedObject();
                nested["type"] = payload.data[i].type;
                nested["data"] = payload.data[i].data;
            }
        }
    } else if (payload.event == ERROR) {
        JsonObject nested = data.createNestedObject();
        nested["type"] = ERROR_CODE;
        nested["data"] = payload.data[0].data;
    }

    char serialized_data[256];
    serializeJson(doc, serialized_data);
    OUTPUT_SERIAL.println(serialized_data);
}

void sendErrorToPi(const ErrorCodes error_code) {
    DataPayload payload = {DEVICE_ID, DEVICE_TYPE, ERROR};
    payload.data[0] = {ERROR_CODE, (short) error_code};
    sendEventToPi(payload);
}

bool sendPayload(uint16_t node, const EventType event, DataPacket data_var[], const size_t data_length = NUM_STATE_DATA_PACKETS) {
    bool exists_in_list = false;
    for (byte i = 0; i < mesh.addrListTop; i++) {
        if (mesh.addrList[i].nodeID == node) {
            exists_in_list = true;
            break;
        }
    }

    if (exists_in_list) {
        DataPayload payload = {DEVICE_ID, DEVICE_TYPE, event};

        if (data_length == 0) {
            for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                payload.data[i] = {NO_STATE,0};
            }
        } else {
            for (short i = 0; i < data_length; i++) {
                payload.data[i] = data_var[i];
            }

            for (short i = data_length; i < NUM_STATE_DATA_PACKETS; i++) {
                payload.data[i] = {NO_STATE,0};
            }
        }


        bool result = mesh.write(&payload, 'M', sizeof(payload), node);

        if (!result) {
            if (DEBUG) {
                Serial.print("Error sending to node: ");
                Serial.println(node);
            }

            sendErrorToPi(NODE_NOT_RESPONDING);
        }
        return result;
    } else {
        if (DEBUG) {
            Serial.print("[RADIO] Node ");
            Serial.print(node);
            Serial.println(" is not connected!");
        }
        sendErrorToPi(NODE_NOT_CONNECTED);
        return false;
    }
}

char raw_serial_data[53];
size_t raw_serial_data_length = 0;
StaticJsonDocument<256> serial_data;
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

            if (result.event == PING) {
                if (DEBUG) {
                    Serial.print("[RADIO EVENT] - PING - UID: ");
                    Serial.println(result.uid);
                }

                bool exists_in_list = false;
                for (short i = 0; i < mesh.addrListTop; i++) {
                    if (mesh.addrList[i].nodeID == result.uid) {
                        exists_in_list = true;
                        break;
                    }
                }

                if (exists_in_list) {
                    result.event = PONG;
                    sendEventToPi(result);
                    sendPayload(result.uid, PONG, {}, 0);
                }
            } else if (result.event == PONG) {
                if (DEBUG) {
                    Serial.print("[RADIO EVENT] - PONG - UID: ");
                    Serial.println(result.uid);
                }
                sendEventToPi(result);
            } else if (result.event == GET_STATE) {
                if (DEBUG) {
                    Serial.print("[RADIO EVENT] - GET_STATE - UID: ");
                    Serial.println(result.uid);
                }
                sendEventToPi(result);
            }
        } else {
            network.read(header, 0, 0);

            if (DEBUG) {
                Serial.print("[RADIO EVENT] - UNKNOWN - ");
                Serial.println(header.type);
            }
        }
    }

    while (OUTPUT_SERIAL.available() > 0) {
        char c = OUTPUT_SERIAL.read();

        if (c == '<') {
            msg_done = false;
        } else if (c == '>') {
            msg_done = true;
        } else {
            raw_serial_data[raw_serial_data_length] = c;
            raw_serial_data_length++;
        }
    }

    if (raw_serial_data_length > 0 && msg_done) {
        deserializeJson(serial_data, raw_serial_data, raw_serial_data_length);

        if ((byte) serial_data["event"] == PING) {
            DataPayload pong_payload = {0,0,PONG};

            sendEventToPi(pong_payload);
            if (DEBUG) {
                Serial.println("[SERIAL EVENT] - PING");
            }
        } else if ((byte) serial_data["event"] == DEVICES) {
            DataPayload devices_payload = {0,0,DEVICES};
            sendEventToPi(devices_payload);

            if (DEBUG) {
                Serial.println("[SERIAL EVENT] - DEVICES");
            }
        } else if ((byte) serial_data["event"] == GET_STATE) {
            sendPayload((uint16_t) serial_data["uid"], GET_STATE, {}, 0);

            if (DEBUG) {
                Serial.print("[SERIAL EVENT] - GET_STATE - ");
                Serial.println((byte) serial_data["uid"]);
            }
        } else if ((byte) serial_data["event"] == SET_STATE) {
            if (DEBUG) {
                Serial.print("[SERIAL EVENT] - SET_STATE - ");
                Serial.println((byte) serial_data["uid"]);
            }

            size_t num_packets = serial_data["data"].size();
            DataPacket state_data[num_packets];

            for (byte i = 0; i < num_packets; i++) {
                if ((byte) serial_data["data"][i]["type"] > DEVICE) {
                    state_data[i].type = (byte) serial_data["data"][i]["type"];
                    state_data[i].data = (short) serial_data["data"][i]["data"];
                }
            }

            sendPayload((uint16_t) serial_data["uid"], SET_STATE, state_data, num_packets);
        }

        raw_serial_data_length = 0;
    }
}
