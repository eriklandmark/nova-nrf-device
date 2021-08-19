size_t NUM_STATE_DATA_PACKETS = 4;

enum DeviceType {
    GATEWAY,
    IRRIGATION_STATION,
    OUTLET,
    REPEATER
};

enum EventType {
    PING,
    PONG,
    OK,
    ERROR,
    SET_STATE,
    GET_STATE,
    DEVICES
};

enum DataType {
    NO_STATE,
    ERROR_CODE,
    DEVICE,
    ON_OFF,
    SOIL_MOISTURE
};

enum ErrorCodes {
    RADIO_ERROR,
    NODE_NOT_CONNECTED,
    NODE_NOT_RESPONDING,
    GATEWAY_WRONG_RESPONSE
};

struct DataPacket {
    DataType type;
    short data;
};

struct DataPayload {
    byte uid;
    byte device_type;
    EventType event;
    DataPacket data[4];
};