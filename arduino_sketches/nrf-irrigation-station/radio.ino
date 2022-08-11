#define PING_INTERVAL 60000 // Seconds
#define MAX_FAIL_BEFORE_RESET 5
#define NETWORK_CHANNEL 1
#define NETWORK_SPEED RF24_250KBPS
#define RADIO_POWER RF24_PA_MAX

RF24 radio(2, 3); // CE, CSN
RF24Network network(radio);
RF24Mesh mesh(radio, network);

byte ping_fail_count = 0;

//bool connected_to_network = false;

unsigned long last_rejoin = 0;

bool radio_setup() {
    mesh.setNodeID(DEVICE_ID);
    mesh.begin(NETWORK_CHANNEL, NETWORK_SPEED);
    radio.setPALevel(RADIO_POWER);

    if (!radio.isChipConnected()) {
        if (DEBUG) {
            Serial.println("[ERROR] Radio is not connected!");
        }
        while (!Serial && ACTIVATE_BLINK) {
            digitalWrite(STATUS_LED, HIGH);
            delay(100);
            digitalWrite(STATUS_LED, LOW);
            delay(100);
        }
    } else {
        if (DEBUG) {
            //printf_begin();
            //radio.printPrettyDetails();
            Serial.println("[INFO] Radio is connected!");
        }

        connected_to_network = mesh.checkConnection();
        if (DEBUG) {
            Serial.print("[INFO] Connected to network: ");
            Serial.println(connected_to_network);
        }
    }
    return true;
}

bool rejoinNetwork(const bool full) {
    if (!mesh.checkConnection()) {
        if (DEBUG) {
            Serial.println("[ERROR] No connection to network. Renewing Address.");
        }

        if(!mesh.renewAddress()){
            if (DEBUG) {
                Serial.println("[ERROR] Failed to renew address");
            }

            if (full) {
                mesh.begin();
                return true;
            } else {
                connected_to_network = false;
                return false;
            }
        } else {
            if (DEBUG) {
                Serial.println("[ERROR] Succeeded to renew address!");
            }
            connected_to_network = true;
            return true;
        }
    } else {
        if (DEBUG) {
            Serial.println("[INFO] Connection to network ok!");
        }
        connected_to_network = true;
        return true;
    }
}

bool sendPayload(uint16_t node, const EventType event_type, DataPacket state_data_var[] = {}, size_t num_packets = 0, byte retry = 0) {
    if (DEBUG) {
        Serial.print("[INFO] Sending event ");
        Serial.print(event_type);
        Serial.print(" Try: ");
        Serial.println(retry);
    }

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
            Serial.print("[ERROR] Send event ");
            Serial.print(event_type);
            Serial.println(" failed!");
        }

        if (retry >= 0 && retry < 5) {
            delay(1);
            return sendPayload(node, event_type, state_data_var, num_packets, retry + 1);
        } else {
            rejoinNetwork(true);
        }
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

void radio_loop() {
    mesh.update();

    if(millis() - last_ping > PING_INTERVAL && received_pong){
        last_ping = millis();

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
                Serial.println("[INFO] Sending ping!");
            }
            received_pong = !sendPayload(0, PING);
            last_pong = millis();
        }
    }

    if(millis() - last_pong >= 1000 && !received_pong) {
        last_pong = millis();

        if (ping_fail_count >= MAX_FAIL_BEFORE_RESET) {
            if (DEBUG) {
                Serial.println("[ERROR] PONG TIMEOUT! Max fails reached, resetting!");
            }
            //softwareReset::standard();
        } else {
            if (DEBUG) {
                Serial.println("[ERROR] PONG TIMEOUT! Trying to renew address!");
            }
            ping_fail_count += 1;
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
           ping_fail_count = 0;
           received_pong = true;
        } else if (result.event == SET_STATE) {
            if (DEBUG) {
                Serial.print("[EVENT] - SET_STATE - UID: ");
                Serial.println(result.uid);
            }

            for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                if (result.data[i].type != NO_STATE) {
                    if (DEBUG) {
                        Serial.print("[EVENT] STATE: ");
                        Serial.print(result.data[i].type);
                        Serial.print(":");
                        Serial.println(result.data[i].data);
                    }

                    handleSetState(result, i);
                }
            }

            DataPacket state_data[NUM_STATE_DATA_PACKETS];
            size_t num_packets = handleGetState(state_data);

            sendPayload(0, GET_STATE, state_data, num_packets);
        } else if (result.event == GET_STATE) {
            bool has_data = false;
            for (short i = 0; i < NUM_STATE_DATA_PACKETS; i++) {
                if (result.data[i].type != NO_STATE) {
                    has_data = true;
                    break;
                }
            }

            if (!has_data) {
                DataPacket state_data[NUM_STATE_DATA_PACKETS];
                size_t num_packets = handleGetState(state_data);
                sendPayload(0, GET_STATE, state_data, num_packets);
            }

            if (DEBUG) {
                Serial.print("[EVENT] - GET_STATE_REQUEST - UID: ");
                Serial.println(result.uid);
            }
        }
    }

    if (!connected_to_network && millis() - last_rejoin > 5000) {
        last_rejoin = millis();
        if (DEBUG) {
            Serial.println("[INFO] Trying to rejoining network!");
        }

        if (rejoinNetwork(true) && DEBUG) {
            Serial.println("[INFO] Rejoined network!");
        }
    }
}
