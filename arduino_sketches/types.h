int NUM_STATE_DATA_PACKETS = 4;

enum DeviceType {
    GATEWAY,
    IRRIGATION_STATION,
    OUTLET
};

enum EventType {
    PING,
    PONG,
    OK,
    ERROR,
    SCAN,
    SET_STATE,
    GET_STATE
};

enum StateType {
    NO_STATE,
    ON_OFF,
    SOIL_MOISTURE
};

struct StateData {
    byte type;
    short data;
};

struct DataPayload {
    byte uid;
    byte device_type;
    EventType event;
    StateData state_data[4];
};