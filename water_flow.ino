#include <EEPROM.h>


#define RESET_BUTTON_PIN 2
#define BIG_BOTTLE_BUTTON_PIN 3// set
#define MANUAL_BUTTON_PIN 4 // set
#define SOLENOID_VALVE_PIN 5 // set
#define PUMP_PIN 6 
#define ORANGE_LED 8 // set
#define MANUAL_LED 9 // set
#define BIG_BOTTLE_LED 10 // set

#define SHORT_MAX_BOUNCE_COUNT 3 
#define LONG_MAX_BOUNCE_COUNT 300

#define PUMP_INACTIVE_DURATION 60000 // 10 minuets
// #define PUMP_INACTIVE_DURATION 6000 // 1 minuets

#define NUMBER_OFF_WATER_FLOW 6000 // 1 minuets
#define NUMBER_OFF_WATER_PAUSE_DURATION 500 //dont change
#define NUMBER_OFF_FILL_CYCLES 22



int global_fill_cycles = 0;

unsigned long big_bottle_fill_counter = 0;
unsigned long tic;
unsigned long startTics; // Variable to hold the start time

unsigned long counter_for_inactive_pump_tic = 0;

bool activity_flag = false;

enum dispenserPumpStates
{
    PUMP_ACTIVE_STATE,
    BOUNCE_BEFORE_PUMP_INACTIVE_STATE,
    PUMP_INACTIVE_STATE,
    BOUNCE_BEFORE_PUMP_ACTIVE_STATE
};

enum dispenserBigBottleFillStates
{
    BIG_BOTTLE_STOP_STATE,
    BOUNCE_BEFORE_BIG_BOTTLE_FILLING_STATE,
    BIG_BOTTLE_FILLING_STATE,
    BIG_BOTTLE_WAITING_STATE,
    BIG_BOTTLE_COMPLE_STATET
};


enum bottleButtontStates
{
    BOTTLE_BUTTON_UNPRESED_STATE,
    BOTTLE_BUTTON_BOUNCE_BEFORE_PRESED_STATE,
    BOTTLE_BUTTON_PRESED_STATE,
    BOTTLE_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE
};

enum manualButtonStates
{
    MANUAL_BUTTON_UNPRESED_STATE,
    MANUAL_BUTTON_BOUNCE_BEFORE_PRESED_STATE,
    MANUAL_BUTTON_PRESED_STATE,
    MANUAL_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE
};

enum resetButtonStates
{
    RESET_BUTTON_UNPRESED_STATE,
    RESET_BUTTON_BOUNCE_BEFORE_PRESED_STATE,
    RESET_BUTTON_PRESED_STATE,
    RESET_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE
};




unsigned char dispenser_Pump_State = PUMP_ACTIVE_STATE;
unsigned char dispenser_bigbottle_fill_state = BIG_BOTTLE_STOP_STATE;
unsigned char bottle_button_state = BOTTLE_BUTTON_UNPRESED_STATE;
unsigned char manual_button_state = MANUAL_BUTTON_UNPRESED_STATE;
unsigned char reset_button_state = RESET_BUTTON_UNPRESED_STATE;

String pump_states_name[] = { "PUMP_ACTIVE_STATE", "PUMP_INACTIVE_STATE" };
String big_bottle_states_name[] = { "BIG_BOTTLE_STOP_STATE","BOUNCE_BEFORE_BIG_BOTTLE_FILLING_STATE","BIG_BOTTLE_FILLING_STATE","BIG_BOTTLE_WAITING_STATE","BIG_BOTTLE_COMPLE_STATET" };
String manual_states_name[] = { "MANUAL_STOP_STATE", "BOUNCE_BEFORE_MANUAL_MODE_STATE", "BOUNCE_BEFORE_STOP_STATE", "MANUAL_MOD_STATE" };


uint8_t computeCRC(uint8_t* data, size_t length) {
    uint8_t crc = 0;
    while (length--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            }
            else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
void writeIntToEEPROM(int address, int value) {
    uint8_t data[4];
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;

    uint8_t crc = computeCRC(data, 4);

    for (int i = 0; i < 4; i++) {
        EEPROM.write(address + i, data[i]);
    }
    EEPROM.write(address + 4, crc);
}
bool readIntFromEEPROM(int address, int& value) {
    uint8_t data[4];
    for (int i = 0; i < 4; i++) {
        data[i] = EEPROM.read(address + i);
    }
    uint8_t storedCRC = EEPROM.read(address + 4);
    uint8_t computedCRC = computeCRC(data, 4);

    if (storedCRC == computedCRC) {
        value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        return true;
    }
    else {
        return false;
    }
}

void setup() {

    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MANUAL_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BIG_BOTTLE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PUMP_PIN, INPUT_PULLUP);
    pinMode(SOLENOID_VALVE_PIN, OUTPUT);
    pinMode(MANUAL_LED, OUTPUT);
    pinMode(BIG_BOTTLE_LED, OUTPUT);
    pinMode(ORANGE_LED, OUTPUT);

    Serial.begin(115200);

    int eepromAddress = 0;
    int readValue = 0;



    if (readIntFromEEPROM(eepromAddress, readValue)) {
        Serial.print("Value read from EEPROM: ");
        Serial.println(readValue);
        global_fill_cycles = readValue;
    }
    else {
        Serial.println("CRC check failed. Data might be corrupted.");
    }
}

void loop() {
    bool pump_active = !digitalRead(PUMP_PIN);
    bool manual_button_pressed = !digitalRead(MANUAL_BUTTON_PIN);
    bool big_bottle_button_presed = !digitalRead(BIG_BOTTLE_BUTTON_PIN);
    bool reset_button_pressed = !digitalRead(RESET_BUTTON_PIN);

    digitalWrite(LED_BUILTIN, pump_active);
    bottleButtonProcessing(big_bottle_button_presed);
    resetButtonProcessing(reset_button_pressed);
    manualButtonProcessing(manual_button_pressed);

    bottleFillingProcess(pump_active);
    delay(10);

}


void bottleButtonProcessing(bool big_bottle_button_presed) {
    static int bottleButtonBounceCounter = 0;

    switch (bottle_button_state)
    {
    case BOTTLE_BUTTON_UNPRESED_STATE:
        if (big_bottle_button_presed) {
            bottle_button_state = BOTTLE_BUTTON_BOUNCE_BEFORE_PRESED_STATE;
            bottleButtonBounceCounter = 0;
            analogWrite(ORANGE_LED, 255);
        }

        break;
    case BOTTLE_BUTTON_BOUNCE_BEFORE_PRESED_STATE:
        if (big_bottle_button_presed) {
            if (bottleButtonBounceCounter >= LONG_MAX_BOUNCE_COUNT) {
                bottle_button_state = BOTTLE_BUTTON_PRESED_STATE;
                writeIntToEEPROM(0, int(NUMBER_OFF_FILL_CYCLES));
                global_fill_cycles = NUMBER_OFF_FILL_CYCLES;
                Serial.print("bottle button pressed global_fill_cycles:");
                Serial.println(global_fill_cycles);
                analogWrite(ORANGE_LED, 0);
                analogWrite(BIG_BOTTLE_LED, 50);
            }
            else {
                bottleButtonBounceCounter++;
            }
        }
        else {
            bottle_button_state = BOTTLE_BUTTON_UNPRESED_STATE;
            analogWrite(ORANGE_LED, 0);

        }
        break;
    case BOTTLE_BUTTON_PRESED_STATE:
        if (!big_bottle_button_presed) {
            bottle_button_state = BOTTLE_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE;
            bottleButtonBounceCounter = 0;
        }


        break;
    case BOTTLE_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE:
        if (!big_bottle_button_presed) {
            if (bottleButtonBounceCounter >= SHORT_MAX_BOUNCE_COUNT) {
                bottle_button_state = BOTTLE_BUTTON_UNPRESED_STATE;
                Serial.println("bottle button unpresed");
            }
            else {
                bottleButtonBounceCounter++;
            }
        }
        else {
            bottle_button_state = BOTTLE_BUTTON_PRESED_STATE;
        }
        break;
    }
}


void resetButtonProcessing(bool reset_button_pressed) {
    static int resetButtonBounceCounter = 0;

    switch (reset_button_state)
    {
    case RESET_BUTTON_UNPRESED_STATE:
        if (reset_button_pressed) {
            reset_button_state = RESET_BUTTON_BOUNCE_BEFORE_PRESED_STATE;
            resetButtonBounceCounter = 0;
        }
        break;
    case RESET_BUTTON_BOUNCE_BEFORE_PRESED_STATE:
        if (reset_button_pressed) {
            if (resetButtonBounceCounter >= LONG_MAX_BOUNCE_COUNT) {

                analogWrite(ORANGE_LED, 255);
                global_fill_cycles = 0;
                reset_button_state = RESET_BUTTON_PRESED_STATE;
                dispenser_bigbottle_fill_state = BIG_BOTTLE_WAITING_STATE;
                // Serial.println("\033[H\033[J");
                Serial.print("reset button pressed global_fill_cycles:");
                Serial.println(global_fill_cycles);

            }
            else {
                resetButtonBounceCounter++;
                analogWrite(ORANGE_LED, 100);
            }
        }
        else {
            reset_button_state = RESET_BUTTON_UNPRESED_STATE;
        }
        break;
    case RESET_BUTTON_PRESED_STATE:
        if (!reset_button_pressed) {
            reset_button_state = RESET_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE;
            resetButtonBounceCounter = 0;
        }

        break;
    case RESET_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE:
        if (!reset_button_pressed) {
            if (resetButtonBounceCounter >= SHORT_MAX_BOUNCE_COUNT) {

                analogWrite(ORANGE_LED, 0);

                reset_button_state = RESET_BUTTON_UNPRESED_STATE;
                Serial.println("reset button unpresed");
            }
            else {
                resetButtonBounceCounter++;
            }
        }
        else {
            reset_button_state = RESET_BUTTON_PRESED_STATE;
        }
        break;
    }
}

void manualButtonProcessing(bool manual_button_pressed) {
    static int manualButtonBounceCounter = 0;

    switch (manual_button_state)
    {
    case MANUAL_BUTTON_UNPRESED_STATE:
        if (manual_button_pressed) {
            manual_button_state = MANUAL_BUTTON_BOUNCE_BEFORE_PRESED_STATE;
            manualButtonBounceCounter = 0;
        }
        break;
    case MANUAL_BUTTON_BOUNCE_BEFORE_PRESED_STATE:
        if (manual_button_pressed) {
            if (manualButtonBounceCounter >= SHORT_MAX_BOUNCE_COUNT) {

                digitalWrite(SOLENOID_VALVE_PIN, HIGH);
                digitalWrite(MANUAL_LED, HIGH);
                manual_button_state = MANUAL_BUTTON_PRESED_STATE;
                Serial.println("manual button pressed");
            }
            else {
                manualButtonBounceCounter++;
            }
        }
        else {
            manual_button_state = MANUAL_BUTTON_UNPRESED_STATE;
        }

        break;
    case MANUAL_BUTTON_PRESED_STATE:
        if (!manual_button_pressed) {
            manual_button_state = MANUAL_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE;
            manualButtonBounceCounter = 0;

        }
        break;

        break;
    case MANUAL_BUTTON_BOUNCE_BEFORE_UNPRESED_STATE:
        if (!manual_button_pressed) {
            if (manualButtonBounceCounter >= SHORT_MAX_BOUNCE_COUNT) {

                digitalWrite(SOLENOID_VALVE_PIN, LOW);
                digitalWrite(MANUAL_LED, LOW);
                manual_button_state = MANUAL_BUTTON_UNPRESED_STATE;
                Serial.println("manual button unpresed");
            }
            else {
                manualButtonBounceCounter++;
            }
        }
        else {
            manual_button_state = MANUAL_BUTTON_PRESED_STATE;
        }
        break;
    }

}


void bottleFillingProcess(bool pump_active) { //bottle fiilng pro
    static int pumpInActiveCounter = 0;
    static int fillsCounter = 0;
    static int waitCounter = 0;


    switch (dispenser_Pump_State)
    {
    case PUMP_ACTIVE_STATE:
        if (!pump_active) {
            pumpInActiveCounter = 0;
            dispenser_Pump_State = BOUNCE_BEFORE_PUMP_INACTIVE_STATE;
        }
        break;

    case BOUNCE_BEFORE_PUMP_INACTIVE_STATE:
        if (!pump_active) {
            if (pumpInActiveCounter >= PUMP_INACTIVE_DURATION)
            {
                dispenser_Pump_State = PUMP_INACTIVE_STATE;
                pumpInActiveCounter = 0;
            }
            else {
                pumpInActiveCounter++;
                Serial.println(float(pumpInActiveCounter) / 100, 1);
            }
        }
        else {
            dispenser_Pump_State = PUMP_ACTIVE_STATE;
        }
        break;


    case PUMP_INACTIVE_STATE:
        if (pump_active) {
            dispenser_Pump_State = BOUNCE_BEFORE_PUMP_ACTIVE_STATE;
            pumpInActiveCounter = 0;
        }
        break;

    case BOUNCE_BEFORE_PUMP_ACTIVE_STATE:

        if (pump_active) {
            if (pumpInActiveCounter >= SHORT_MAX_BOUNCE_COUNT) {
                dispenser_Pump_State = PUMP_ACTIVE_STATE;
                Serial.println("PUMP_ACTIVE_STATE");
            }
            else {
                pumpInActiveCounter++;
            }
        }
        else {
            dispenser_Pump_State = PUMP_INACTIVE_STATE;
        }

        break;
    }





    switch (dispenser_bigbottle_fill_state)
    {
    case BIG_BOTTLE_STOP_STATE:
        if (dispenser_Pump_State == PUMP_INACTIVE_STATE) {
            if (global_fill_cycles > 0)
            {
                digitalWrite(SOLENOID_VALVE_PIN, HIGH);
                digitalWrite(BIG_BOTTLE_LED, HIGH);
                dispenser_bigbottle_fill_state = BIG_BOTTLE_FILLING_STATE;
                Serial.print(" BIG_BOTTLE_STOP_STATE global_fill_cycles: ");
                Serial.println(global_fill_cycles);
            }
        }

        break;
    case BIG_BOTTLE_FILLING_STATE:
        if (fillsCounter >= NUMBER_OFF_WATER_FLOW) {
            dispenser_bigbottle_fill_state = BIG_BOTTLE_WAITING_STATE;
            Serial.print("global_fill_cycles: ");
            Serial.println(global_fill_cycles);
            writeIntToEEPROM(0, global_fill_cycles--);
            digitalWrite(SOLENOID_VALVE_PIN, LOW);
            analogWrite(BIG_BOTTLE_LED, 50);

            fillsCounter = 0;
            waitCounter = 0;
        }
        else {
            fillsCounter++;
            analogWrite(BIG_BOTTLE_LED, 150);
        }
        break;
    case BIG_BOTTLE_WAITING_STATE:
        if (global_fill_cycles < 1) {
            dispenser_bigbottle_fill_state = BIG_BOTTLE_STOP_STATE;
            
            digitalWrite(SOLENOID_VALVE_PIN, LOW);
            analogWrite(BIG_BOTTLE_LED, 0);
            writeIntToEEPROM(0, global_fill_cycles);
            Serial.print("global_fill_cycles: ");
            Serial.println(global_fill_cycles);

        }
        else {
            if (waitCounter >= NUMBER_OFF_WATER_PAUSE_DURATION) {
                digitalWrite(SOLENOID_VALVE_PIN, LOW);
                analogWrite(BIG_BOTTLE_LED, 100);
                dispenser_bigbottle_fill_state = BIG_BOTTLE_STOP_STATE;
            }
            else {
                waitCounter++;
            }
        }
        break;
    }

}