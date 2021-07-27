void setup() {
    // put your setup code here, to run once:
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(9600);
    Serial1.begin(9600);
    while(!Serial) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
}

void loop() {
    String msg = "HELLO WORLD HELLO WORLD HELLO WORLD HELLO WORLD";
    Serial1.println(msg);
    delay(1000);
}
