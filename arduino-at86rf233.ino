#include <SoftwareSerial.h>

#include <SPI.h>

byte Read  = 0B10000000; //Read command register access
byte Write = 0B11000000; //Write command register access

byte Command_Write = 0B01100000; //Write command frame access
byte PHR_Write = 0B00010001; //Length of the frame to be sent (n + 2; n - Number of PSDU bytes)
byte FCF1 = 0B00100001; //Frame control field, octet 1
byte FCF2 = 0B10011000; //Frame control field, octet 2
byte D_PAN_ID0 = 0B00000001;
byte D_PAN_ID1 = 0B00000001;
byte D_address0 = 0B00000001;
byte D_address1 = 0B00000001;
byte S_PAN_ID0 = 0B00000001;
byte S_PAN_ID1 = 0B00000001;
byte S_address0 = 0B00000001;
byte S_address1 = 0B00000001;
byte MSDU1 = 0B00000001;
byte MSDU2 = 0B00000010;
byte MSDU3 = 0B00000011;
byte MSDU4 = 0B00000100;
byte FCS1 = 0B00000000; //to be replaced by FCS octet 1
byte FCS2 = 0B00000000; //to be replaced by FCS octet 2

byte Command_Read = 0B00100000; //Read command frame access

byte SRAM_Read = 0B00000000; //Read command SRAM access

unsigned long ctrlLED = 13;
unsigned long RESET = 8;
unsigned long SLP_TR = 7;
unsigned long IRQ = 9;
unsigned long SEL = 6;

int ctrlState = LOW;
long last = 0;
long Intervall = 1000;


void setup() {
  
        Serial.begin(115200);
        Serial.println("Booting");
        pinMode(ctrlLED, OUTPUT);
        pinMode(RESET, OUTPUT);
        pinMode(SLP_TR, OUTPUT);
        pinMode(IRQ, INPUT);  
        pinMode(SEL, OUTPUT);
            

        SPI.begin();
        //Data is transmitted and received MSB first
        SPI.setBitOrder(MSBFIRST);  
        //SPI interface will run at 1MHz if 8MHz chip or 2Mhz if 16Mhz
        SPI.setClockDivider(SPI_CLOCK_DIV8);   
        //Data is clocked on the rising edge and clock is low when inactive
        SPI.setDataMode(SPI_MODE0);      

        delay(100);
        
        digitalWrite(SLP_TR, LOW);
        digitalWrite(RESET, HIGH);
        digitalWrite(SEL, HIGH);
        
        writeRegister(0x08, 0x0B); //Channel: 2405 MHz
        
        writeRegister(0x20, 0x01); //Short address bit[7:0]
        writeRegister(0x21, 0x01); //Short address bit[15:8]
        writeRegister(0x22, 0x01); //PAN ID bit[7:0]
        writeRegister(0x23, 0x01); //PAN ID bit[15:8]
        
        byte TRX_CTRL_0 = readRegister(0x03);
        TRX_CTRL_0 = TRX_CTRL_0 | 0B10000000; //TOM mode enabled
        writeRegister(0x03, TRX_CTRL_0);
  
        //byte XAH_CTRL_1 = readRegister(0x17);
        //XAH_CTRL_1 = XAH_CTRL_1 | 0B00000001; //bit[4] = AACK_UPLD_RES_FT; bit[1] = AACK_PROM_MODE
        //writeRegister(0x17, XAH_CTRL_1);
  
        delay(1); 
}

void loop() { 
  
  byte CurrentState = readRegister(0x01);
  byte Interrupt = readRegister(0x0F); //Indicate whether state changed from P_ON to TRX_OFF (IRQ_4_AWAKE_END = 1)

  unsigned long now = millis();
  
  if (now - last > Intervall)
  {
  last = now;
  Serial.println(".");
  Serial.print("; State: ");
  Serial.print(CurrentState, HEX);
  
  Serial.print("; Interrupt: ");
  Serial.print(Interrupt, BIN);
  
  
  if (ctrlState == LOW)
  ctrlState = HIGH;
  else
  ctrlState = LOW;
  
  digitalWrite(ctrlLED, ctrlState);    

  }
   
 if (CurrentState == 0x00) {
    delay(1);
    writeRegister(0x0E, 0B00010000); // Interrupt AWAKE_END (IRQ_4) enabled    
    writeRegister(0x02, 0x08); //Go from P_ON to TRX_OFF state
    delay(1);
  }
 delay(2000);
 if (CurrentState == 0x08) {
    delay(1);
    writeRegister(0x0E, 0B00000001); // Interrupt PLL_LOCK (IRQ_0) enabled    
    writeRegister(0x02, 0x09); //Go from TRX_OFF to PLL_ON state
    delay(0.016);
  }
  
 if (Interrupt == 0B00000001) { //if PLL is locked
    writeRegister(0x0E, 0B00001000); // Interrupt TRX_END (IRQ_3) enabled
    writeFrame(0xAA); //Data to send (Sequence number)
    readFrame(0x00);
    writeRegister(0x02, 0x19); //Go from PLL_ON to TX_ARET_ON state
  }
  
 if (CurrentState == 0x19) {
    digitalWrite(SLP_TR, HIGH); delay(1);
 }
 
 
  byte TRAC = readRegister(0x02); //if 000: SUCCESS
  Serial.print("; TRAC: ");
  Serial.print(TRAC, BIN);
  readSRAM(0x7D);

}

// Read from register
  byte readRegister(byte thisRegister) {
  byte result = 0x00;
  byte dataToSend = thisRegister | Read; //delay (1000);
  digitalWrite(SEL, LOW);
  SPI.transfer(dataToSend);
  result = SPI.transfer(0x00);
  digitalWrite(SEL, HIGH);
return(result);  
}

// Write to register
  void writeRegister(byte thisRegister, byte thisValue) {
  byte dataToSend = thisRegister | Write; //delay (1000);
  digitalWrite(SEL, LOW);
  SPI.transfer(dataToSend);
  SPI.transfer(thisValue);
  digitalWrite(SEL, HIGH);
}

// Write to frame
  void writeFrame(byte SEQ_NR) {
  digitalWrite(SEL, LOW);
  SPI.transfer(Command_Write);
  delay (0.16);
  SPI.transfer(PHR_Write);
  delay (0.032);
  SPI.transfer(FCF1); //transmitted data
  delay (4.064);
  SPI.transfer(FCF2); //transmitted data
  delay (4.064);
  SPI.transfer(SEQ_NR); //transmitted data
  delay (4.064);
  SPI.transfer(D_PAN_ID0); //transmitted data
  delay (4.064);
  SPI.transfer(D_PAN_ID1); //transmitted data
  delay (4.064);
  SPI.transfer(D_address0); //transmitted data
  delay (4.064);
  SPI.transfer(D_address1); //transmitted data
  delay (4.064);
  SPI.transfer(S_PAN_ID0); //transmitted data
  delay (4.064);
  SPI.transfer(S_PAN_ID1); //transmitted data
  delay (4.064);  
  SPI.transfer(S_address0); //transmitted data
  delay (4.064);
  SPI.transfer(S_address1); //transmitted data
  delay (4.064);  
  SPI.transfer(MSDU1); //transmitted data
  delay (4.064);
  SPI.transfer(MSDU2); //transmitted data
  delay (4.064);
  SPI.transfer(MSDU3); //transmitted data
  delay (4.064);
  SPI.transfer(MSDU4); //transmitted data
  delay (4.064);
  SPI.transfer(FCS1); //transmitted data
  delay (4.064);
  SPI.transfer(FCS2); //transmitted data
  delay (4.064);
  digitalWrite(SEL, HIGH);
}

// Read from frame
  byte readFrame(byte re) {
  byte result2 = 0x00;
  byte result3 = 0x00;
  byte result4 = 0x00;
  byte result5 = 0x00;
  byte result6 = 0x00;
  byte result7 = 0x00;
  byte result8 = 0x00;
  byte result9 = 0x00;
  byte result10 = 0x00;
  byte result11 = 0x00;
  byte result12 = 0x00;
  byte result13 = 0x00;
  byte result14 = 0x00;
  byte result15 = 0x00;
  byte result16 = 0x00;
  byte result17 = 0x00;
  digitalWrite(SEL, LOW);
  SPI.transfer(Command_Read);
  result2 = SPI.transfer(0x00);
  Serial.print("; PHR: ");
  Serial.print(result2, HEX);
  result3 = SPI.transfer(0x00);
  Serial.print("; FCF1: ");
  Serial.print(result3, HEX);
  result4 = SPI.transfer(0x00);
  Serial.print("; FCF2: ");
  Serial.print(result4, HEX);
  result5 = SPI.transfer(0x00);
  Serial.print(" SEQ_NR: ");
  Serial.print(result5, HEX);
  result6 = SPI.transfer(0x00);
  Serial.print(" D_PAN_ID0: ");
  Serial.print(result6, HEX);
  result7 = SPI.transfer(0x00);
  Serial.print(" D_PAN_ID1: ");
  Serial.print(result7, HEX);
  result8 = SPI.transfer(0x00);
  Serial.print(" D_address0: ");
  Serial.print(result8, HEX);
  result9 = SPI.transfer(0x00);
  Serial.print(" D_address1: ");
  Serial.print(result9, HEX);  
  result10 = SPI.transfer(0x00);
  Serial.print(" S_PAN_ID0: ");
  Serial.print(result10, HEX);
  result11 = SPI.transfer(0x00);
  Serial.print(" S_PAN_ID1: ");
  Serial.print(result11, HEX);  
  result12 = SPI.transfer(0x00);
  Serial.print(" S_address0: ");
  Serial.print(result12, HEX);
  result13 = SPI.transfer(0x00);
  Serial.print(" S_address1: ");
  Serial.print(result13, HEX);  
  result14 = SPI.transfer(0x00);
  Serial.print(" MSDU1: ");
  Serial.print(result14, HEX);
  result15 = SPI.transfer(0x00);
  Serial.print(" MSDU2: ");
  Serial.print(result15, HEX);
  result16 = SPI.transfer(0x00);
  Serial.print(" MSDU3: ");
  Serial.print(result16, HEX);
  result17 = SPI.transfer(0x00);
  Serial.print(" MSDU4: ");
  Serial.print(result17, HEX);
  digitalWrite(SEL, HIGH);
return(result5);  
}

//Read from SRAM (for time-of-flight)
  byte readSRAM(byte beginwith) {
  byte resulti = 0x00;
  byte resultii = 0x00;
  byte resultiii = 0x00;
  digitalWrite(SEL, LOW);
  SPI.transfer(SRAM_Read);
  SPI.transfer(beginwith); //Read frame beginning with this register
  resulti = SPI.transfer(0x00); //Time-of-flight, bits [7:0] (register 7D)
  Serial.print("; a: ");
  Serial.print(resulti, HEX);
  resultii = SPI.transfer(0x00); //Time-of-flight, bits [15:8] (register 7E)
  Serial.print("; b: ");
  Serial.print(resultii, HEX);
  resultiii = SPI.transfer(0x00); //Time-of-flight, bits [23:16] (register 7F)
  Serial.print("; c: ");
  Serial.print(resultiii, HEX);
  digitalWrite(SEL, HIGH);
 return(resulti);
  }
