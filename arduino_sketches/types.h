enum DeviceType {
    NRF_PROXY,
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
    byte state_type;
    short state_data;
};

struct DataPayload {
    byte uid;
    byte device_type;
    EventType event;
    StateData state_data[8];
};