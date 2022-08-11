#include "RF24.h"
#include "RF24Network.h"
#include "RF24Mesh.h"
#include <SPI.h>
#include "types.h"
#include "printf.h"
#include <EEPROM.h>

#define DEVICE_ID 4
#define DEVICE_TYPE IRRIGATION_STATION

#define RUN_OFFLINE false

#define RESET_PIN 16

#define DEBUG true
#define ACTIVATE_BLINK true
#define STATUS_LED 18

#define PUMP_PIN 5
#define LAMP_PIN 10
#define BUTTON_PIN 21
#define WATER_OUTPUT_1_PIN 6
#define WATER_OUTPUT_2_PIN 7
#define WATER_OUTPUT_3_PIN 8
#define WATER_OUTPUT_4_PIN 9
#define TANK_LEVEL_PIN A6
#define BATTERY_VOLTAGE_PIN A5
#define SHOW_INSTRUMENTS_PIN 17

#define DATA_SAMPLE_INTERVAL 5000
#define NO_WATERING_TIME 21600000
#define SHOW_INSTRUMENTS_TIME 1000

#define DISABLE_OUTPUT_1 false
#define DISABLE_OUTPUT_2 false
#define DISABLE_OUTPUT_3 false
#define DISABLE_OUTPUT_4 false

unsigned long WATER_OUTPUT_1_TIME = 10000;
unsigned long WATER_OUTPUT_2_TIME = 7000;
unsigned long WATER_OUTPUT_3_TIME = 18000;
unsigned long WATER_OUTPUT_4_TIME = 12000;

short SOIL_MOISTURE_DATA = 0;
short BATTERY_VOLTAGE_DATA = 0;
short TANK_LEVEL_DATA = 10;
short WATERING_RUNNING = 0;

unsigned long watering_done = 0;
unsigned long button_pressed_timeout = 0;
unsigned long last_data_sample = 0;
unsigned long last_watering = 0;
unsigned long last_led_blink = 0;
bool led_state = false;
bool show_instruments = false;
bool button_pressed = false;

unsigned long output_1_done = 0;
unsigned long output_2_done = 0;
unsigned long output_3_done = 0;
unsigned long button_pressed_time = 0;

bool output_1_ran = false;
bool output_2_ran = true;
bool output_3_ran = true;

bool connected_to_network = false;
bool auto_start = false;

bool sendPayload(uint16_t node, const EventType event_type, DataPacket state_data_var[] = {}, size_t num_packets = 0, byte retry = 0);

void setup() {
    if (ACTIVATE_BLINK) {
        pinMode(STATUS_LED, OUTPUT);
    }

    if (DEBUG) {
        Serial.begin(9600);
        while (!Serial) {
            digitalWrite(STATUS_LED, HIGH);
            delay(100);
            digitalWrite(STATUS_LED, LOW);
            delay(100);
        }
        Serial.println("[INFO] Starting irrigation station!");
    }

    pinMode(PUMP_PIN, OUTPUT);
    pinMode(LAMP_PIN, OUTPUT);

    pinMode(WATER_OUTPUT_1_PIN, OUTPUT);
    pinMode(WATER_OUTPUT_2_PIN, OUTPUT);
    pinMode(WATER_OUTPUT_3_PIN, OUTPUT);
    pinMode(WATER_OUTPUT_4_PIN, OUTPUT);

    pinMode(BUTTON_PIN, INPUT);
    pinMode(SHOW_INSTRUMENTS_PIN, OUTPUT);
    pinMode(TANK_LEVEL_PIN, INPUT);
    pinMode(BATTERY_VOLTAGE_PIN, INPUT);

    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(WATER_OUTPUT_1_PIN, LOW);
    digitalWrite(WATER_OUTPUT_2_PIN, LOW);
    digitalWrite(WATER_OUTPUT_3_PIN, LOW);
    digitalWrite(WATER_OUTPUT_4_PIN, LOW);
    digitalWrite(LAMP_PIN, HIGH);

    delay(100);
    if (!RUN_OFFLINE) {
        radio_setup();
        connected_to_network = true;
    } else {
        if (DEBUG) {
            Serial.println("[INFO] Running offline!");
        }
    }

    if (DISABLE_OUTPUT_1) {
        WATER_OUTPUT_1_TIME = 0;
    }
    if (DISABLE_OUTPUT_2) {
        WATER_OUTPUT_2_TIME = 0;
    }
    if (DISABLE_OUTPUT_3) {
        WATER_OUTPUT_3_TIME = 0;
    }
    if (DISABLE_OUTPUT_4) {
        WATER_OUTPUT_4_TIME = 0;
    }

    digitalWrite(LAMP_PIN, LOW);

    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), on_button_changed, CHANGE);
}

size_t handleGetState(DataPacket* state_data) {
    state_data[0] = {ON_OFF, WATERING_RUNNING};
    state_data[1] = {TANK_LEVEL, TANK_LEVEL_DATA};
    state_data[2] = {SOIL_MOISTURE, SOIL_MOISTURE_DATA};
    return 1;
}

void start_watering(bool send_to_master = true) {
    if (DEBUG) {
        Serial.println("[INFO] Watering started!");
    }

    WATERING_RUNNING = 1;

    if (send_to_master && !RUN_OFFLINE) {
        DataPacket state_data[NUM_STATE_DATA_PACKETS];
        size_t num_packets = handleGetState(state_data);
        sendPayload(0, GET_STATE, state_data, num_packets);
    }

    watering_done = millis() + WATER_OUTPUT_1_TIME + WATER_OUTPUT_2_TIME + WATER_OUTPUT_3_TIME + WATER_OUTPUT_4_TIME;
    last_watering = millis();
    output_1_ran = false;

    digitalWrite(PUMP_PIN, HIGH);
    digitalWrite(LAMP_PIN, HIGH);
    if (!DISABLE_OUTPUT_1) {
        digitalWrite(WATER_OUTPUT_1_PIN, HIGH);
    }
    digitalWrite(WATER_OUTPUT_1_PIN, HIGH);
    output_1_done = millis() + WATER_OUTPUT_1_TIME;
}

void stop_watering(bool send_to_master = true) {
    if (DEBUG) {
        Serial.println("[INFO] Watering stopped!");
    }
    WATERING_RUNNING = 0;

    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(LAMP_PIN, LOW);
    digitalWrite(WATER_OUTPUT_1_PIN, LOW);
    digitalWrite(WATER_OUTPUT_2_PIN, LOW);
    digitalWrite(WATER_OUTPUT_3_PIN, LOW);
    digitalWrite(WATER_OUTPUT_4_PIN, LOW);

    if (send_to_master && !RUN_OFFLINE) {
        DataPacket state_data[NUM_STATE_DATA_PACKETS];
        size_t num_packets = handleGetState(state_data);
        sendPayload(0, GET_STATE, state_data, num_packets);
    }
}

void on_button_changed() {
    const unsigned long now = millis();
    if (now - button_pressed_timeout > 100) {
        button_pressed_timeout = now;

        if (digitalRead(BUTTON_PIN) == LOW) {
            if (DEBUG) {
                Serial.println("[INFO] Button released!");
            }
            button_pressed = false;
            digitalWrite(SHOW_INSTRUMENTS_PIN, LOW);
            if (millis() - button_pressed_time < SHOW_INSTRUMENTS_TIME) {
                auto_start = false;
                if (WATERING_RUNNING) {
                    stop_watering(connected_to_network);
                } else {
                    start_watering(connected_to_network);
                }
            }
        } else {
            if (DEBUG) {
                Serial.println("[INFO] Button pressed!");
            }
            button_pressed_time = now;
            button_pressed = true;
        }
    }
}

void handleSetState(DataPayload result, short i) {
    if (result.data[i].type == ON_OFF) {
        short watering = (short) result.data[i].data;
        if (watering) {
            start_watering(false);
        } else {
            stop_watering(false);
        }
    }
}

void loop() {
    if (!RUN_OFFLINE) {
        radio_loop();
    }

    if (button_pressed_time + SHOW_INSTRUMENTS_TIME <= millis() && button_pressed) {
        digitalWrite(SHOW_INSTRUMENTS_PIN, HIGH);
    }

    if(millis() >= watering_done && WATERING_RUNNING){
        stop_watering(!auto_start);
    }

    if(millis() >= output_1_done && WATERING_RUNNING && !output_1_ran){
        output_1_ran = true;
        digitalWrite(WATER_OUTPUT_1_PIN, LOW);
        if (!DISABLE_OUTPUT_2) {
            digitalWrite(WATER_OUTPUT_2_PIN, HIGH);
        }
        digitalWrite(WATER_OUTPUT_2_PIN, HIGH);

        output_2_done = millis() + WATER_OUTPUT_2_TIME;
        output_2_ran = false;

        if (DEBUG) {
            Serial.println("[INFO] Watering output 1 done!");
        }
    }

    if(millis() >= output_2_done && WATERING_RUNNING && !output_2_ran){
        output_2_ran = true;
        digitalWrite(WATER_OUTPUT_2_PIN, LOW);
        if (!DISABLE_OUTPUT_3) {
            digitalWrite(WATER_OUTPUT_3_PIN, HIGH);
        }

        output_3_done = millis() + WATER_OUTPUT_3_TIME;
        output_3_ran = false;

        if (DEBUG) {
            Serial.println("[INFO] Watering output 2 done!");
        }
    }

    if(millis() >= output_3_done && WATERING_RUNNING && !output_3_ran){
        output_3_ran = true;
        digitalWrite(WATER_OUTPUT_3_PIN, LOW);
        if (!DISABLE_OUTPUT_4) {
            digitalWrite(WATER_OUTPUT_4_PIN, HIGH);
        }

        if (DEBUG) {
            Serial.println("[INFO] Watering output 3 done!");
        }
    }

    if (millis() - last_data_sample >= DATA_SAMPLE_INTERVAL) {
        last_data_sample = millis();
        TANK_LEVEL_DATA = analogRead(TANK_LEVEL_PIN);
        BATTERY_VOLTAGE_DATA = analogRead(BATTERY_VOLTAGE_PIN);
        if (DEBUG) {
            Serial.print("[INFO] Data sampled: ");
            Serial.print(TANK_LEVEL_DATA);
            Serial.print("|");
            Serial.print(BATTERY_VOLTAGE_DATA);
            Serial.print("|");
            Serial.println(SOIL_MOISTURE_DATA);
        }
    }

    if(millis() - last_watering >= NO_WATERING_TIME){
        if (DEBUG) {
            Serial.println("[INFO] Auto watering started.");
        }
        auto_start = true;
        start_watering(false);
    }

    if(millis() - last_led_blink >= 1000 && ACTIVATE_BLINK){
        last_led_blink = millis();
        led_state = !led_state;
        digitalWrite(STATUS_LED, led_state);
    }
}
