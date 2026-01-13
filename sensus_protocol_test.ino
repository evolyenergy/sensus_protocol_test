#include <Arduino.h>
#define SWAP_CLOCK          // Allow to quick test swapping clock mode (ie 0->1 or 1->0)
#define DELAY_US       1000 // delay between clock pulse and read (and read to clock end)
#define SETTLE_TIME     250 // Settle time for device 
#define MAX_WAKE_PULSE  100 // Max pulse to wait for start bit of the frame

#ifdef SWAP_CLOCK
#define clock_ON  1
#define clock_OFF 0
#else 
#define clock_ON  0
#define clock_OFF 1
#endif

#define MAX_BYTES 32 // Sensus

// IO Used
uint8_t clock_pin = 2;  // D2 for clock
uint8_t read_pin = 3;   // D3 for data reading

// Sensus receive buffer
uint8_t read_buff[MAX_BYTES]; 
bool wait_start_bit = true;

// power on meter
void powerUp()
{
  Serial.print("Powering Meter, waiting ");
  Serial.print(SETTLE_TIME);
  Serial.print("ms ... ");
  digitalWrite(clock_pin, clock_ON); 

  // wait to stabilize
  delay(SETTLE_TIME);

  // We will need to wait for start bit
  wait_start_bit = true ;

  Serial.println("Done");
}

// power off meter
void powerDown()
{
  digitalWrite(clock_pin, clock_OFF);
}

uint8_t sensus_readBit()
{
  digitalWrite(clock_pin, clock_OFF);
  delayMicroseconds(DELAY_US);
  uint8_t val = digitalRead(read_pin);
  digitalWrite(clock_pin, clock_ON);
  delayMicroseconds(DELAY_US);
  return val;
}

uint8_t sensus_readByte() 
{
  uint8_t data = 0;
  bool parity = false;
  uint8_t bit = sensus_readBit() ;

  // First byte to read ? Need to pulse until
  // We got 1st start bit.
  if (wait_start_bit) {
    uint8_t to = MAX_WAKE_PULSE;
    uint16_t waked_pulses = 1;
    Serial.print("Waiting for 1st Start bit...");
    // Wait until time out or start detected
    while ( (to-- > 0) && (bit != 0) ) {
      waked_pulses++;
      bit = sensus_readBit();
    }
    wait_start_bit = false;
    if (to>0){
      Serial.print("OK ");
      Serial.print(waked_pulses);
      Serial.println(" pulses");
    } else {
      Serial.println("Unable to wake device");
    }
  }

  if (bit != 0) {
    // Start bit error
    Serial.print("{");
  }
  for (int i = 0; i < 7; ++i) {
    if (sensus_readBit()) {
      data |= (1 << i);
      parity = !parity;
    }
  }
  if (sensus_readBit() != parity) {
    // Parity bit error
    Serial.print("!");
  }
  if (sensus_readBit() != 1) {
    // Stop bit error
    Serial.print("}");
  }
  return data;   
}

uint8_t sensus_readData(uint8_t * p, uint8_t max_bytes) 
{
  uint8_t c , i;
  // Power up Meter
  powerUp();

  Serial.print("Reading : ");

  // Clear receive buffer
  memset(p, 0, sizeof(max_bytes));

  for (i = 0; i < max_bytes; ++i) {
    c = sensus_readByte();
    Serial.print(c);
    Serial.print(c, HEX);
    // Finished ?
    if (c == '\r' || c == '\n') {
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

// data comes in as V;RBxxxxxxx;IByyyyy;Kmmmmm
//  where xxxx is the meter read value (arbitrary digits, but not more than 12)
//  yyyy is the meter id (arbitrary digits)
//  mmmm is another meter id (arbitrary digits)
//  Note that the IB and K parts are optional
bool sensus_parseData(uint8_t * p_data, uint32_t * p_index, uint32_t * p_id )
{

  enum STATE {PARSE_V, PARSE_SEMI, PARSE_PRE, PARSE_NUM} state = PARSE_V;

  // temp storage for variables until we've parsed the whole string.
  uint32_t k_number = 0;
  uint32_t * num_ptr = &(k_number);

  while (*p_data) {
    switch (state) {
      case PARSE_V:
        if (*p_data != 'V') {
          return false;
        }
        state = PARSE_SEMI;
      break;
      case PARSE_SEMI:
        if (*p_data != ';') {
          return false;
        }
        state = PARSE_PRE;
      break;
      case PARSE_PRE:
        if ((*p_data == 'R') && (*(p_data + 1) == 'B')) {
          num_ptr = p_index;
          p_data ++;
        } else if ((*p_data == 'I') && (*(p_data + 1) == 'B')) {
          num_ptr = p_id;
          p_data++;
        } else if ((*p_data == 'K')) {
          num_ptr = &k_number;
        }
        *num_ptr = 0;
        state = PARSE_NUM;
      break;
      case PARSE_NUM:
        if (((*p_data) >= 48) && ((*p_data) <= 57)) {
          *num_ptr = (*num_ptr) * 10 + (*p_data) - 48;
          break;
        }
        p_data--;
        state = PARSE_SEMI;
    }
    p_data++;
  }
  return true;
}

void read_sensus() 
{
  uint8_t len;
  len = sensus_readData(read_buff, MAX_BYTES);
  Serial.print("received "); 
  Serial.print(len); 
  Serial.println(" bytes"); 

  if (len<MAX_BYTES) {
    uint32_t volume, id ;
    if (sensus_parseData(read_buff, &volume, &id)) {
      Serial.print("MeterID "); 
      Serial.print(id); 
      Serial.print(", Volume "); 
      Serial.println(volume); 
    } else {
      Serial.print("Unable to decode "); 
      Serial.println((char *) read_buff);
    }
  } else {
    Serial.println("Unable to read "); 
  }

  // Wait 10s before next reading
  delay(10000);
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

  Serial.print("Settle Time :" );  
  Serial.print(SETTLE_TIME );  
  Serial.print("ms,  Clock Pulse Duration :" );  
  Serial.print(DELAY_US );  
  Serial.println("ms");  

  // power off the meter
  digitalWrite(clock_pin, clock_OFF); 
  pinMode(read_pin, INPUT_PULLUP);

  // make sure that the meter is reset
  //delay(2000); 
  
  Serial.println("Sensus Meter setup done...");
}

void loop() 
{
  // Read Data
  read_sensus();

  // Wait 10s before next reading
  delay(10000);
}













