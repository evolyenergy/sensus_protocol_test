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

#define MAX_BYTES 32

uint8_t clock_pin = 2;
uint8_t read_pin = 3;

// receive buffer
uint8_t read_buff[MAX_BYTES]; 

// power on meter
void powerUp()
{
  Serial.print("Powering Meter...");
  digitalWrite(clock_pin, clock_ON); 
  // wait to stabilize
  delay(1000);
  Serial.println("Done");
}

// power off meter
void powerDown()
{
  digitalWrite(clock_pin, clock_OFF);
}

uint8_t readBit()
{
  digitalWrite(clock_pin, clock_OFF);
  delay(DELAY_MS);
  uint8_t val = digitalRead(read_pin);
  digitalWrite(clock_pin, clock_ON);
  delay(DELAY_MS);
  return val;
}

uint8_t readByte() 
{
  uint8_t data = 0;
  bool parity = false;
  if (readBit() != 0) {
    Serial.print("{");
  }
  for (int i = 0; i < 7; ++i) {
    if (readBit()) {
      data |= (1 << i);
      parity = !parity;
    }
  }
  if (readBit() != parity) {
    Serial.print("!");
  }
  if (readBit() != 1) {
    Serial.print("}");
  }
  return data;   
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
    // Save byte
    *p++ = c;
  }
  Serial.println();
  // Unpower Meter
  powerDown();
  return i;
}

void setup() 
{
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
  // power off the meter
  digitalWrite(clock_pin, clock_OFF); 
  pinMode(read_pin, INPUT_PULLUP);

  // make sure that the meter is reset
  delay(2000); 
  
  Serial.println("setup done...");
}

void loop() 
{
  uint8_t len;
  len = readData(read_buff, MAX_BYTES);
  Serial.print("received "); 
  Serial.print(len); 
  Serial.println(" bytes"); 
  Serial.println((char *) read_buff);
  // Wait 10s before next reading
  delay(10000);
}