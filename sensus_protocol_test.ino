#include <Arduino.h>

#define RxData 3
#define TxClock 2 //D5(pin 14)

int cycle = 0;
byte val = 0;
byte clkState = LOW;
unsigned int ab;
unsigned int meterByte[100];
unsigned long count = 0;
unsigned long test = 0;
unsigned long currentMillis = millis();
unsigned long currentMicros = micros();
unsigned long previousMillis = 0; // For Delay between meter reads
unsigned long previousClkMicros = 0; // For TxClock timing
unsigned long interval = 30000; // 10 sec Time delay between Meter Reads
unsigned long ClkTime = 550; //[550 100%], [500 No Good], [800 NO], [450 NO], [600 OK]


void ReadCycle()
{
  for (int ReadByte=0; ReadByte<7; ReadByte++)
  {
    // delay(10);
    //10 bit byte
    for (int bitCount=0;bitCount<10;bitCount++)
    { 
      int var = 0;
      // 2 phase per clock cycle
      while(var <2)
      { 
        currentMicros = micros();

        // change state every 550uS
        if (currentMicros - previousClkMicros > ClkTime)
        { 
          if (clkState==LOW)
          {
            delayMicroseconds(50); // time for wifi overhead
            clkState = HIGH;
            digitalWrite(TxClock,HIGH);
            previousClkMicros = currentMicros;
          }
          else
          { //clkState was HIGH
            // strip Start,top 3 bits, Stop, and parity
            if (bitCount >0 && bitCount <8)
            { 
              delayMicroseconds(50); // wifi overhead
              val = digitalRead(RxData); // Read at end of HIGH
              bitWrite(meterByte[ReadByte], bitCount-1, val);// write bit state
            }
            clkState = LOW;
            digitalWrite(TxClock,LOW);
            previousClkMicros = currentMicros;
          }
          var++;
        }
      }
    }
  }
  Serial.println();
}

// 50 ascii char pre clock
void PreClock()
{ 

 //NOT 75, 10, 20
 for (int y=0; y<30; y++)
 { 
   // ascii char 1S, 7Db, 1P, 1ST = 10 bits
   for (int i=0;i<10;i++)
   { 
      int var = 0;
      while(var <2)
      {
        currentMicros = micros();
        // change state every 550uS
        if (currentMicros - previousClkMicros > ClkTime)
        { 
          if (clkState==LOW)
          {
            clkState = HIGH;
            digitalWrite(TxClock,HIGH);
            previousClkMicros = currentMicros;
          }
          else 
          { //clkState was HIGH
            clkState = LOW;
            digitalWrite(TxClock,LOW);
            previousClkMicros = currentMicros;
          }
          var++;
        }
      }
    }
  }
}

void DataPrint()
{
  for (int i=1; i<6; i++)
  {
    char ab = meterByte[i];
    Serial.print(ab);
  }
  Serial.print(".");
  char ab = meterByte[6];
  Serial.print(ab);
  Serial.print(" cycle count is ");
  cycle++;
  Serial.println(cycle); // counts between resets
}

void SyncCycle() // potentially loose sync after 25 or so - need to renull if R is not found
{
  for (int ReadByte=0; ReadByte<36; ReadByte++)
  { // 36 bytes
    for (int bitCount=0;bitCount<10;bitCount++)
    { // 10 bits
      int var = 0;
      while(var <2)
      {
        currentMicros = micros();
        if (currentMicros - previousClkMicros > ClkTime)
        { // change state every 500uS
          if (clkState==LOW)
          {
            clkState = HIGH;
            digitalWrite(TxClock,HIGH);
            previousClkMicros = currentMicros;
          }
          else 
          { //clkState was HIGH
            if (bitCount >0 && bitCount <8)
            { // strip Start, Stop, and parity
              val = digitalRead(RxData); // Read on HIGH
              bitWrite(meterByte[ReadByte], bitCount-1, val);// write bit state
            }
            clkState = LOW;
            digitalWrite(TxClock,LOW);
            previousClkMicros = currentMicros;
          }
          var++;
        }
      }
    }

    char ab = meterByte[ReadByte];
    if ((meterByte[ReadByte])== 82)
    { //R=82 This works!!!
      break;
    }
  }
}

void AlignByte()
{//in 360 bits find low

  for (int bitCount=0; bitCount<360; bitCount++)
  {
    int var = 0;
    while(var <2)
    {
      currentMicros = micros();
      if (currentMicros - previousClkMicros > ClkTime)
      { // change state every 500uS
        if (clkState==LOW)
        {
          clkState = HIGH;
          digitalWrite(TxClock,HIGH);
          previousClkMicros = currentMicros;
        }
        else 
        { //clkState was HIGH
          val = digitalRead(RxData); // Read on HIGH
          if (val==LOW)
          {
            break;
          }
          clkState = LOW;
         digitalWrite(TxClock,LOW);
          previousClkMicros = currentMicros;

        }
        var++;
      }
    }
  }
}

void FindNull()
{ //look for 11 ones in a row
  int eleven = 0;
  for (int bitCount=0; bitCount<360; bitCount++)
  {
    int var = 0;
    while(var <2)
    {
      currentMicros = micros();
      if (currentMicros - previousClkMicros > ClkTime)
      { // change state every 500uS
        if (clkState==LOW)
        {
          clkState = HIGH;
          digitalWrite(TxClock,HIGH);
          previousClkMicros = currentMicros;
        }
        else 
        { //clkState was HIGH
          val = digitalRead(RxData); // Read at end of HIGH
          if (val==HIGH)
          {
            eleven++;
            if (eleven ==11)
            {
              break;
            }
          }
          else
          { // val == LOW
            eleven = 0;
          }
          clkState = LOW;
          digitalWrite(TxClock,LOW);
          previousClkMicros = currentMicros;
        }
        var++;
      }
    }
  }
}

void GetData()
{
  Serial.println("GetData");
  PreClock(); // 0-50 ascii char
  FindNull(); // start in a null patch
  AlignByte(); // look for start bit within 45 bytes
  SyncCycle(); //collect bytes until B
  ReadCycle(); // Get 5 digit reading
}


void setup() { // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.flush();
  pinMode (RxData,INPUT_PULLUP); // Meter Read Pin
  pinMode (TxClock,OUTPUT); // clock Pin
  Serial.begin(115200);//115200
  digitalWrite(TxClock,LOW); // off
  Serial.println("Setup Done!");
}

void loop() 
{ // put your main code here, to run repeatedly:
  currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    int x = millis();
    GetData(); // verify data

    int y = millis();
    int z = y-x;
    Serial.print(" Disconnected. Connect time ");
    Serial.print(z);
    Serial.print(" ");
    DataPrint();
  }
  
}
