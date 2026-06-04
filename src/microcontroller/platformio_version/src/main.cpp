#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// =============================================================================
// IC Pin → Arduino Pin Mapping
// Index i corresponds to IC physical pin (i+1), zero-based.
// -1 = power pin (VCC/GND), not connected to Arduino I/O.
// Edit this table to remap for a different socket/shield layout.
// =============================================================================
const int8_t IC_PINS[14] = {
     8,   // IC pin  1 → D8
     9,   // IC pin  2 → D9
    10,   // IC pin  3 → D10
    11,   // IC pin  4 → D11
    12,   // IC pin  5 → D12
    13,   // IC pin  6 → D13
    -1,   // IC pin  7  (GND) — always '0' in PINS message
     7,   // IC pin  8 → D7
     6,   // IC pin  9 → D6
     5,   // IC pin 10 → D5
     4,   // IC pin 11 → D4
     3,   // IC pin 12 → D3
     2,   // IC pin 13 → D2
    -1,   // IC pin 14  (VCC) — always '0' in PINS message
};

// =============================================================================
// Relay Configuration
// Module assumed active-LOW: LOW = relay energized (closes NO contact).
// If your module is active-HIGH, swap HIGH/LOW inside setRelay().
// =============================================================================
const uint8_t RELAY_PIN      = A0;     // Arduino Nano A0
const float   OVERCURRENT_MA  = 100.0f; // overcurrent trip threshold in mA
const float   LOW_CURRENT_MA  =   9.5f; // undercurrent trip threshold in mA
const uint32_t BLANKING_MS      = 50;   // ignore current for this long after relay closes
const uint32_t LOW_CURRENT_MS   = 1000;  // relay opens after this long below LOW_CURRENT_MA

// =============================================================================
// Gate Definitions — 0-based IC pin indices (IC physical pin - 1)
//
// 7404: [input_idx, output_idx]
// Groups: (1,2), (3,4), (5,6), (13,12), (11,10), (9,8)
// =============================================================================
const uint8_t GATES_7404[6][2] = {
    { 0,  1},   // pin1  → pin2
    { 2,  3},   // pin3  → pin4
    { 4,  5},   // pin5  → pin6
    {12, 11},   // pin13 → pin12
    {10,  9},   // pin11 → pin10
    { 8,  7},   // pin9  → pin8
};

// 7408 / 7432: [inA_idx, inB_idx, output_idx]
// Groups: (1,2,3), (4,5,6), (10,9,8), (13,12,11)
const uint8_t GATES_2IN[4][3] = {
    { 0,  1,  2},   // pin1,  pin2  → pin3
    { 3,  4,  5},   // pin4,  pin5  → pin6
    { 9,  8,  7},   // pin10, pin9  → pin8
    {12, 11, 10},   // pin13, pin12 → pin11
};

// =============================================================================
// Globals
// =============================================================================
Adafruit_INA219 ina219(0x40);
uint16_t        selectedIC      = 0;
// pinState[i]: '1' if gate group containing IC pin (i+1) passed all combos,
//              '0' if it failed or is a power pin.
uint8_t         pinState[14]    = {};
unsigned long   lastCurrentMs   = 0;
bool            relayOpen        = true;   // true = relay open, false = relay closed
unsigned long   relayCloseTime   = 0;      // timestamp of last relay close, for blanking
unsigned long   lowCurrentSince  = 0;      // millis() when low current first detected, 0 = not active

// =============================================================================
// Relay — single entry point for all relay state changes
// =============================================================================

// All relay control goes through this function for easy bare metal AVR translation.
void setRelay(bool open) {
    if (open) {
        digitalWrite(RELAY_PIN, HIGH);  // AVR: PORTC |= (1<<PC0) — de-energize coil, NO opens
    } else {
        digitalWrite(RELAY_PIN, LOW);   // AVR: PORTC &= ~(1<<PC0) — energize coil, NO closes
        relayCloseTime  = millis();     // AVR: read timer0_millis — start blanking window
        lowCurrentSince = 0;            // reset low-current timer on each close
    }
    relayOpen = open;
}

// Returns true if currently inside the post-close blanking window.
bool inBlankingWindow() {
    return !relayOpen && (millis() - relayCloseTime < BLANKING_MS);
}

// Reads INA219. Skips check during blanking window.
// If current > OVERCURRENT_MA: opens relay, sends "OVERCURRENT\n", returns true.
bool checkOvercurrent() {
    if (inBlankingWindow()) return false;
    float mA = ina219.getCurrent_mA();  // AVR: TWI — TWCR START, SLA+R 0x40, 2-byte read
    if (mA > OVERCURRENT_MA) {
        setRelay(true);
        Serial.println("OVERCURRENT");  // AVR: USART0 TX
        return true;
    }
    if (mA < LOW_CURRENT_MA) {
        if (lowCurrentSince == 0) {
            lowCurrentSince = millis();          // AVR: read timer0_millis — start low-current timer
        } else if (millis() - lowCurrentSince >= LOW_CURRENT_MS) {
            setRelay(true);
            return true;
        }
    } else {
        lowCurrentSince = 0;                     // reading recovered — reset timer
    }
    return false;
}

// =============================================================================
// Pin Helpers
// Each Arduino API call annotated with AVR bare-metal equivalent.
// =============================================================================

void releaseAllPins() {
    for (uint8_t i = 0; i < 14; i++) {
        if (IC_PINS[i] < 0) continue;
        pinMode(IC_PINS[i], INPUT);   // AVR: DDRx &= ~(1 << bit)
    }
}

// =============================================================================
// IC Test Routines
//
// For each gate group:
//   - run all input combinations
//   - if ALL pass: set every pin in that group to 1 in pinState[]
//   - if ANY fail: set every pin in that group to 0 in pinState[]
// checkOvercurrent() called after each combination — aborts test if tripped.
// Power pins (idx 6, 13) stay 0.
// =============================================================================

bool test7404() {
    bool allPass     = true;
    bool overcurrent = false;
    memset(pinState, 0, sizeof(pinState));
    releaseAllPins();

    for (uint8_t g = 0; g < 6 && !overcurrent; g++) {
        uint8_t inIdx  = GATES_7404[g][0];
        uint8_t outIdx = GATES_7404[g][1];
        int8_t  inPin  = IC_PINS[inIdx];
        int8_t  outPin = IC_PINS[outIdx];
        bool    gPass  = true;

        pinMode(outPin, INPUT);               // AVR: DDRx &= ~(1 << bit)
        for (uint8_t a = 0; a <= 1 && !overcurrent; a++) {
            pinMode(inPin, OUTPUT);           // AVR: DDRx |= (1 << bit)
            digitalWrite(inPin, a);           // AVR: val ? PORTx |= (1<<bit) : PORTx &= ~(1<<bit)
            delayMicroseconds(50);            // AVR: _delay_us(50) — ~800 NOPs at 16 MHz
            if (checkOvercurrent()) { overcurrent = true; allPass = false; break; }
            uint8_t y = digitalRead(outPin);  // AVR: (PINx >> bit) & 1
            if (y != (uint8_t)(!a)) gPass = false;
        }

        if (!overcurrent) {
            uint8_t v        = gPass ? 1 : 0;
            pinState[inIdx]  = v;
            pinState[outIdx] = v;
            if (!gPass) allPass = false;
        }
    }

    releaseAllPins();
    return allPass;
}

bool test7408() {
    bool allPass     = true;
    bool overcurrent = false;
    memset(pinState, 0, sizeof(pinState));
    releaseAllPins();

    for (uint8_t g = 0; g < 4 && !overcurrent; g++) {
        uint8_t inA   = GATES_2IN[g][0];
        uint8_t inB   = GATES_2IN[g][1];
        uint8_t out   = GATES_2IN[g][2];
        int8_t  pinA  = IC_PINS[inA];
        int8_t  pinB  = IC_PINS[inB];
        int8_t  pinY  = IC_PINS[out];
        bool    gPass = true;

        pinMode(pinY, INPUT);                 // AVR: DDRx &= ~(1 << bit)
        for (uint8_t ab = 0; ab < 4 && !overcurrent; ab++) {
            uint8_t a = (ab >> 1) & 1;
            uint8_t b =  ab       & 1;
            pinMode(pinA, OUTPUT);            // AVR: DDRx |= (1 << bit)
            digitalWrite(pinA, a);            // AVR: PORTx bit = a
            pinMode(pinB, OUTPUT);            // AVR: DDRx |= (1 << bit)
            digitalWrite(pinB, b);            // AVR: PORTx bit = b
            delayMicroseconds(50);            // AVR: _delay_us(50)
            if (checkOvercurrent()) { overcurrent = true; allPass = false; break; }
            uint8_t y = digitalRead(pinY);    // AVR: (PINx >> bit) & 1
            if (y != (a & b)) gPass = false;
        }

        if (!overcurrent) {
            uint8_t v     = gPass ? 1 : 0;
            pinState[inA] = v;
            pinState[inB] = v;
            pinState[out] = v;
            if (!gPass) allPass = false;
        }
    }

    releaseAllPins();
    return allPass;
}

bool test7432() {
    bool allPass     = true;
    bool overcurrent = false;
    memset(pinState, 0, sizeof(pinState));
    releaseAllPins();

    for (uint8_t g = 0; g < 4 && !overcurrent; g++) {
        uint8_t inA   = GATES_2IN[g][0];
        uint8_t inB   = GATES_2IN[g][1];
        uint8_t out   = GATES_2IN[g][2];
        int8_t  pinA  = IC_PINS[inA];
        int8_t  pinB  = IC_PINS[inB];
        int8_t  pinY  = IC_PINS[out];
        bool    gPass = true;

        pinMode(pinY, INPUT);                 // AVR: DDRx &= ~(1 << bit)
        for (uint8_t ab = 0; ab < 4 && !overcurrent; ab++) {
            uint8_t a = (ab >> 1) & 1;
            uint8_t b =  ab       & 1;
            pinMode(pinA, OUTPUT);            // AVR: DDRx |= (1 << bit)
            digitalWrite(pinA, a);            // AVR: PORTx bit = a
            pinMode(pinB, OUTPUT);            // AVR: DDRx |= (1 << bit)
            digitalWrite(pinB, b);            // AVR: PORTx bit = b
            delayMicroseconds(50);            // AVR: _delay_us(50)
            if (checkOvercurrent()) { overcurrent = true; allPass = false; break; }
            uint8_t y = digitalRead(pinY);    // AVR: (PINx >> bit) & 1
            if (y != (a | b)) gPass = false;
        }

        if (!overcurrent) {
            uint8_t v     = gPass ? 1 : 0;
            pinState[inA] = v;
            pinState[inB] = v;
            pinState[out] = v;
            if (!gPass) allPass = false;
        }
    }

    releaseAllPins();
    return allPass;
}

// =============================================================================
// Serial Output Helpers
// =============================================================================

// Sends "PINS:xxxxxxxxxxxxxx\n"
// Each char: '1' = gate group passed, '0' = failed or power pin.
void sendPins() {
    char buf[21];
    buf[0]='P'; buf[1]='I'; buf[2]='N'; buf[3]='S'; buf[4]=':';
    for (uint8_t i = 0; i < 14; i++) {
        buf[5 + i] = (IC_PINS[i] < 0) ? '0' : ('0' + pinState[i]);
    }
    buf[19] = '\n';
    buf[20] = '\0';
    Serial.print(buf);   // AVR: poll UDRE0 in UCSR0A, write each byte to UDR0
}

// Sends "I:<mA>\n" — current in mA, 2 decimal places.
// Sends "I:0.00\n" during blanking window instead of a real reading.
void sendCurrent() {
    if (inBlankingWindow()) {
        Serial.print("I:0.00\n");   // AVR: USART0 TX
        return;
    }
    float mA = ina219.getCurrent_mA();   // AVR: TWI — TWCR START, SLA+R 0x40, 2-byte read
    char num[12];
    dtostrf(mA, 1, 2, num);              // AVR libc float-to-ASCII, avoids printf bloat
    Serial.print("I:");                  // AVR: USART0 TX via UDR0/UDRE0
    Serial.print(num);
    Serial.print('\n');
}

// =============================================================================
// Setup & Loop
// =============================================================================

void setup() {
    // AVR: UBRR0 = (F_CPU/(8UL*115200))-1; UCSR0A |= (1<<U2X0);
    //      UCSR0B |= (1<<RXEN0)|(1<<TXEN0); UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
    Serial.begin(115200);

    // AVR: TWBR = ((F_CPU/100000UL)-16)/2; TWCR |= (1<<TWEN);
    //      then I2C write to 0x40 to configure INA219 calibration register
    ina219.begin();

    pinMode(RELAY_PIN, OUTPUT);  // AVR: DDRC |= (1<<PC0)
    setRelay(true);              // relay starts open — closes only on first START

    releaseAllPins();
    Serial.println("READY");     // AVR: USART0 TX
}

void loop() {
    // ── Current reporting — non-blocking 500 ms interval ────────────────────
    // AVR: millis() reads volatile timer0_millis, incremented by TIMER0_OVF ISR
    unsigned long now = millis();
    if (now - lastCurrentMs >= 250UL) {
        lastCurrentMs = now;
        sendCurrent();
    }

    // ── Overcurrent watchdog — only when relay is closed and blanking is over ─
    if (!relayOpen) {
        checkOvercurrent();
    }

    // ── Serial command parsing ───────────────────────────────────────────────
    if (Serial.available()) {   // AVR: UCSR0A & (1 << RXC0)
        // AVR: read UDR0 bytes until '\n' or Serial timeout (1000 ms default)
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if      (cmd == "7404") { selectedIC = 7404; }
        else if (cmd == "7408") { selectedIC = 7408; }
        else if (cmd == "7432") { selectedIC = 7432; }
        else if (cmd == "START") {
            if (selectedIC == 0) return;

            if (relayOpen) {
                setRelay(false);  // only close if tripped — starts new blanking window
                delay(100);       // wait for circuit to fully power on before testing
            }

            bool result = false;
            if      (selectedIC == 7404) result = test7404();
            else if (selectedIC == 7408) result = test7408();
            else if (selectedIC == 7432) result = test7432();

            if (!relayOpen) {  // skip result if overcurrent aborted the test
                sendPins();
                Serial.println(result ? "RESULT:PASS" : "RESULT:FAIL");  // AVR: USART0 TX
            }
        }
    }
}
