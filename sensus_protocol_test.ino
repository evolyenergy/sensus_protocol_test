#include <Arduino.h>
#define SWAP_CLOCK
#define DELAY_MS 1

// Select here meter type to be tested
//#define TYPE_SENSUS
#define TYPE_NEPTUNE

#if defined (TYPE_SENSUS) && defined (TYPE_NEPTUNE)  
#error "Multiple smart meters are defined please use only one"
#endif

#if !defined (TYPE_SENSUS) && !defined (TYPE_NEPTUNE)  
#error "No smart meter defined please select one"
#endif

#ifdef SWAP_CLOCK
#define clock_ON  1
#define clock_OFF 0
#else 
#define clock_ON  0
#define clock_OFF 1
#endif

//#define MAX_BYTES 32 // Sensus
#define MAX_BYTES 35 // Neptune

uint8_t clock_pin = 2;
uint8_t read_pin = 3;

// Sensus / Neptune receive buffer
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

uint8_t sensus_readBit()
{
  digitalWrite(clock_pin, clock_OFF);
  delay(DELAY_MS);
  uint8_t val = digitalRead(read_pin);
  digitalWrite(clock_pin, clock_ON);
  delay(DELAY_MS);
  return val;
}

uint8_t sensus_readByte() 
{
  uint8_t data = 0;
  bool parity = false;
  if (sensus_readBit() != 0) {
    Serial.print("{");
  }
  for (int i = 0; i < 7; ++i) {
    if (sensus_readBit()) {
      data |= (1 << i);
      parity = !parity;
    }
  }
  if (sensus_readBit() != parity) {
    Serial.print("!");
  }
  if (sensus_readBit() != 1) {
    Serial.print("}");
  }
  return data;   
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


uint8_t neptune_readData(uint8_t * p, uint8_t max_bytes) 
{
  unsigned int dataAlign[35]; // 35 is ok Buffer for bit read data 
  int count = 9; 
  int bitcount = 0; 
  int mask = 15; 
  unsigned int last = 0; 
  unsigned int last_A = 0; // Send Data count register
  int bitRate = 415; // 1187hz. Seems more stable than 1200 
  bool state = false;
  bool laststate = false; 
  //byte timing tuning
  // mask 0b0000 0000 0000 1111. Strips 4 bit integer // Send Data record count

  powerUp();

  // set up to put an initial low on clk line
  digitalWrite(clock_pin,clock_OFF); 
  
  state = digitalRead(read_pin);

  // Clk until Rx line changes (Up to 10 minutes)
  while (state == digitalRead(read_pin)) {
    digitalWrite(clock_pin, clock_OFF);
    delayMicroseconds(bitRate);

    // Changed, are we done
    if (state != digitalRead(read_pin)) {
      break;
    }

    digitalWrite(clock_pin, clock_ON);
    delayMicroseconds(bitRate);
  }


  // Look for Rx line to go Low (62 - 95mS)
  // Quickly align transistion of state change 
  state = digitalRead(read_pin);

  while (state == digitalRead(read_pin)) {
  
    for (int y = 0; y < 32; y++) {

      if (y == 0) {
        digitalWrite(clock_pin, clock_OFF);
      }

      if (y == 15) { 
        digitalWrite(clock_pin, clock_ON);
      }

      delayMicroseconds(20);

      if (state != digitalRead(read_pin)) {
        break;
      }
    }
  }

  // Read 34 Data bytes. 316mS to 319mS per 34 bytes read.
  for (int mData = 0; mData < 34; mData++, p++) {

    // 11 bits per byte incl. 2 stop and 1 start
    for (int bytecount = 0; bytecount < 11; bytecount++) { 

      // read each bit 8 times. 4 high, 4 low 
      for (bitcount = 7; bitcount >= 0; bitcount--) { 

        if (bitcount == 7) {
          digitalWrite(clock_pin, clock_OFF);
        }

        if (bitcount == 3) { 
          digitalWrite(clock_pin, clock_ON);
        }

        // 1180 bits/Sec. 107 bytes/Sec 
        delayMicroseconds(96); 

        laststate = digitalRead(read_pin);

        if (bitcount == 5) {
          // write bit state
          bitWrite(read_buff[mData], bytecount, laststate); 
        }
      }

      // fine tune timing. Count from 7 to 11
      delayMicroseconds(count); 
    }

    // dataAlign[mData] = read_buff[mData]; //align 11 bit bytes
    // should be start bit
    if (bitRead(read_buff[mData], 10) == 0) {
      dataAlign[mData] = read_buff[mData]; 
    }
    // shift right may correct align
    if (bitRead(read_buff[mData], 10) > 0) {
      dataAlign[mData] = read_buff[mData] >> 1; 
    }
    // bit 5 & 6 should be 1
    if ((bitRead(read_buff[mData], 5) > 0) && (bitRead(read_buff[mData], 6) > 0)) {
      dataAlign[mData] = read_buff[mData] >> 1;
    } 
    // align 11 bit bytes
    if ((bitRead(read_buff[mData], 6) > 0) && (bitRead(read_buff[mData], 7) > 0)) {
      dataAlign[mData] = read_buff[mData] >> 2;
    } 

    // meterData is least 4 bits of masked aligned data
    read_buff[mData] = dataAlign[mData] & mask; 
    
    // maybe more fine timing tuning
    // delayMicroseconds(count);  
  }

  // put low on meter
  digitalWrite(clock_pin, clock_OFF); 
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

void read_neptune() 
{
  String dataString = "";
  uint8_t len;

  len = neptune_readData(read_buff, MAX_BYTES);

  dataString += read_buff[7];
  dataString += read_buff[8];
  dataString += read_buff[9];
  dataString += read_buff[10];
  dataString += read_buff[11]; 
  dataString += ("."); 
  dataString += read_buff[12];
  Serial.print("Meter : ");
  Serial.println(dataString);
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

  // Select here one to be tested, not both
  #if defined (TYPE_SENSUS) 
  Serial.print("Sensus");
  #elif defined (TYPE_NEPTUNE)  
  Serial.print("Neptune");
  #endif

  // make sure that the meter is reset
  delay(2000); 
  
  Serial.println(" Meter setup done...");
}

void loop() 
{
  // Select here one to be tested, not both
  #if defined (TYPE_SENSUS) 
  read_sensus();
  #elif defined (TYPE_NEPTUNE)  
  read_neptune();
  #endif

  // Wait 10s before next reading
  delay(10000);
}













