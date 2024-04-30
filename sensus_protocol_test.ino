#include <Arduino.h>
#define SWAP_CLOCK
#define DELAY_MS 1

#ifdef SWAP_CLOCK
#define clock_ON  1
#define clock_OFF 0
#else 
#define clock_ON  0
#define clock_OFF 1
#endif

#define MAX_BYTES 50

uint8_t clock_pin = 2;
uint8_t read_pin = 3;

uint8_t read_buff[MAX_BYTES]; // receive buffer

// power on meter
void powerUp() {
  Serial.print("Powering Meter...");
  digitalWrite(clock_pin, clock_ON); 
  // wait to stabilize
  delay(500);
  Serial.println("Done");
}

// power off meter
void powerDown() {
  digitalWrite(clock_pin, clock_OFF);
}

uint8_t readBit() {
  digitalWrite(clock_pin, clock_OFF);
  delay(DELAY_MS);
  uint8_t val = digitalRead(read_pin);
  digitalWrite(clock_pin, clock_ON);
  delay(DELAY_MS);
  return val;
}

uint8_t readByte() {
  uint8_t result = 0;
  bool parity = false;
  if (readBit() != 0) {
    Serial.print("{");
  }
  for (int i = 0; i < 7; ++i) {
    if (readBit()) {
      result |= (1 << i);
      parity = !parity;
    }
  }
  if (readBit() != parity) {
    Serial.print("!");
  }
  if (readBit() != 1) {
    Serial.print("}");
  }
  return result;   
}

uint8_t _readByte() {
  uint8_t bits[10];
  for (uint8_t i = 0; i < 10; ++i) {
    bits[i] = readBit();
  }
  uint8_t result = 0;
  for (uint8_t b = 0; b < 7; ++b) {
    uint8_t i = b + 1;
    if (bits[i]) {
      result |= (1 << b);
    }
  }
  return result;    
}

uint8_t readData(uint8_t * p, uint8_t max_bytes) 
{
  uint8_t c , i;
  // Power up Meter
  powerUp();
  Serial.print("Reading : ");
  // Clear receive buffer
  memset(p, 0, sizeof(max_bytes));
  for (i = 0; i < max_bytes; ++i) {
    c = readByte();
    Serial.print(c, HEX);
    // Finished ?
    if (c == '\r') {
      break;
    }
    *p++ = c;
  }
  Serial.println();
  // Unpower Meter
  powerDown();
  return i;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(clock_pin, OUTPUT);

  Serial.print("\r\n\r\nSetup using pins: clk:" );  
  Serial.print(clock_pin);
  Serial.print(" (ON=" );  
  Serial.print(clock_ON);
  Serial.print(")" );  
  Serial.print(" data pin "); 
  Serial.println(read_pin);
  digitalWrite(clock_pin, clock_OFF); // power off the meter
  pinMode(read_pin, INPUT_PULLUP);
  delay(5000); // make sure that the meter is reset
  Serial.println("setup done...");
}

void loop() {
  uint8_t len;
  len = readData(read_buff, MAX_BYTES);
  Serial.print("received "); 
  Serial.print(len); 
  Serial.println(" bytes"); 
  Serial.println((char *) read_buff);
  // Wait 10s before next reading
  delay(10000);
}