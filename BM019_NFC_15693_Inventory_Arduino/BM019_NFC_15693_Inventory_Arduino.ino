/*
NFC Communication with the Solutions Cubed, LLC BM019 
and an Arduino Uno.  The BM019 is a module that
carries ST Micro's CR95HF, a serial to NFC converter.

Wiring:
 Arduino          BM019
 IRQ: Pin 9       DIN: pin 2
 SS: pin 10       SS: pin 3
 MOSI: pin 11     MOSI: pin 5 
 MISO: pin 12     MISO: pin4
 SCK: pin 13      SCK: pin 6
 
 */

// the sensor communicates using SPI, so include the library:
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif


const int SSPin = 10;  // Slave Select pin
const int IRQPin = 9;  // Sends wake-up pulse
byte TXBuffer[40];    // transmit buffer
byte RXBuffer[40];    // receive buffer
byte NFCReady = 0;  // used to track NFC state


void setup() {
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done
    display.clearDisplay();
      display.display();
        display.setCursor(0,0);
  
  
    pinMode(IRQPin, OUTPUT);
    digitalWrite(IRQPin, HIGH); // Wake up pulse
    pinMode(SSPin, OUTPUT);
    digitalWrite(SSPin, HIGH);

    Serial.begin(9600);
    SPI.begin();
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    SPI.setClockDivider(SPI_CLOCK_DIV32);

    

 
 // The CR95HF requires a wakeup pulse on its IRQ_IN pin
 // before it will select UART or SPI mode.  The IRQ_IN pin
 // is also the UART RX pin for DIN on the BM019 board.
 
    delay(10);                      // send a wake up
    digitalWrite(IRQPin, LOW);      // pulse to put the 
    delayMicroseconds(100);         // BM019 into SPI
    digitalWrite(IRQPin, HIGH);     // mode 
    delay(10);
}

/* IDN_Command identifies the CR95HF connected to the Arduino.
This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response

If the correct response is received the serial monitor is used
to display the CR95HF ID number and CRC code.  This rountine is 
not that useful in using the NFC functions, but is a good way to 
verify connections to the CR95HF. 
*/
void IDN_Command()
 {
 byte i = 0;

// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0);  // SPI control byte to send command to CR95HF
  SPI.transfer(1);  // IDN command
  SPI.transfer(0);  // length of data that follows is 0
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready
// data is ready when a read byte
// has bit 3 set (ex:  B'0000 1000')

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);


// step 3, read the data
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  for (i=0;i<RXBuffer[1];i++)      
      RXBuffer[i+2]=SPI.transfer(0);  // data
  digitalWrite(SSPin, HIGH);
  delay(1);

  if ((RXBuffer[0] == 0) & (RXBuffer[1] == 15))
  {  
    Serial.println("IDN COMMAND-");  //
    Serial.print("RESPONSE CODE: ");
    Serial.print(RXBuffer[0]);
    Serial.print(" LENGTH: ");
    Serial.println(RXBuffer[1]);
    Serial.print("DEVICE ID: ");
    for(i=2;i<(RXBuffer[1]);i++)
    {
      Serial.print(RXBuffer[i],HEX);
      Serial.print(" ");
    }
    Serial.println(" ");
    Serial.print("ROM CRC: ");
    Serial.print(RXBuffer[RXBuffer[1]],HEX);
    Serial.print(RXBuffer[RXBuffer[1]+1],HEX);
    Serial.println(" ");
  }
  else
    Serial.println("BAD RESPONSE TO IDN COMMAND!");

  Serial.println(" ");
}

/* SetProtocol_Command programs the CR95HF for
ISO/IEC 15693 operation.

This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response

If the correct response is received the serial monitor is used
to display successful programming. 
*/
void SetProtocol_Command()
 {
 byte i = 0;
 
// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x00);  // SPI control byte to send command to CR95HF
  SPI.transfer(0x02);  // Set protocol command
  SPI.transfer(0x02);  // length of data to follow
  SPI.transfer(0x01);  // code for ISO/IEC 15693
  SPI.transfer(0x0D);  // Wait for SOF, 10% modulation, append CRC
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);

// step 3, read the data
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  digitalWrite(SSPin, HIGH);

  if ((RXBuffer[0] == 0) & (RXBuffer[1] == 0))
  {
     Serial.println("PROTOCOL SET-");  //
     NFCReady = 1; // NFC is ready
  }
  else
  {
     Serial.println("BAD RESPONSE TO SET PROTOCOL");
     NFCReady = 0; // NFC not ready
  }
  Serial.println(" ");
}

/* Inventory_Command chekcs to see if an RF
tag is in range of the BM019.

This requires three steps.
1. send command
2. poll to see if CR95HF has data
3. read the response

If the correct response is received the serial monitor is used
to display the the RF tag's universal ID.  
*/
void Inventory_Command()
 {
 byte i = 0;
char Str1[128];
// step 1 send the command
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x00);  // SPI control byte to send command to CR95HF
  SPI.transfer(0x04);  // Send Receive CR95HF command
  SPI.transfer(0x03);  // length of data that follows is 0
  SPI.transfer(0x26);  // request Flags byte
  SPI.transfer(0x01);  // Inventory Command for ISO/IEC 15693
  SPI.transfer(0x00);  // mask length for inventory command
  digitalWrite(SSPin, HIGH);
  delay(1);
 
// step 2, poll for data ready
// data is ready when a read byte
// has bit 3 set (ex:  B'0000 1000')

  digitalWrite(SSPin, LOW);
  while(RXBuffer[0] != 8)
    {
    RXBuffer[0] = SPI.transfer(0x03);  // Write 3 until
    RXBuffer[0] = RXBuffer[0] & 0x08;  // bit 3 is set
    }
  digitalWrite(SSPin, HIGH);
  delay(1);


// step 3, read the data
  digitalWrite(SSPin, LOW);
  SPI.transfer(0x02);   // SPI control byte for read         
  RXBuffer[0] = SPI.transfer(0);  // response code
  RXBuffer[1] = SPI.transfer(0);  // length of data
  for (i=0;i<RXBuffer[1];i++)      
      RXBuffer[i+2]=SPI.transfer(0);  // data
  digitalWrite(SSPin, HIGH);
  delay(1);

  if (RXBuffer[0] == 128)
  {  
    Serial.println("TAG DETECTED");
    Serial.print("UID: ");
    for(i=11;i>=4;i--)
    {
      Serial.print(RXBuffer[i],HEX);
      display.setCursor(0,0);
      Str1[i] = RXBuffer[i];

    }
     display.setCursor(0,0);
      display.setTextSize(2);
  display.setTextColor(WHITE);
  display.println(Str1[128],HEX);
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(3.141592);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.display();
    Serial.println(" ");
  }
  else
    {
    Serial.print("NO TAG IN RANGE - ");
display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("NO TAG IN RANGE");
  display.setTextColor(BLACK, WHITE); // 'inverted' text
  display.println(3.141592);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.display();
  display.clearDisplay();

    Serial.print("RESPONSE CODE: ");
    Serial.println(RXBuffer[0],HEX);
    }
  Serial.println(" ");
}


void loop() {
  
  if(NFCReady == 0)
  {
    IDN_Command();  // reads the CR95HF ID
    delay(1000);
    SetProtocol_Command(); // ISO 15693 settings
    delay(1000);
  }
  else
  {

    Inventory_Command();
    delay(1000);    
  }  
  
}

