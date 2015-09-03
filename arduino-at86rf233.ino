#include <SPI.h>

/* Constants */
#define REG_READ     0B10000000 //Read command register access
#define REG_WRITE    0B11000000 //Write command register access
#define FRAME_WRITE  0b01100000

/* SPI Commands */
#define CMD_FB_READ 0B00100000 //Frame Buffer read
#define CMD_SRAM_READ 0B00000000 //SRAM Read command

/* Interrupt codes */
#define IRQ_BAT_LOW	(1 << 7)
#define IRQ_TRX_UR	(1 << 6)
#define IRQ_AMI		(1 << 5)
#define IRQ_CCA_ED	(1 << 4)
#define IRQ_TRX_END	(1 << 3)
#define IRQ_RX_START	(1 << 2)
#define IRQ_PLL_UNL	(1 << 1)
#define IRQ_PLL_LOCK	(1 << 0)

/* Possible states (register: xxx) */
#define STATE_P_ON		0x00	/* BUSY */
#define STATE_BUSY_RX		0x01
#define STATE_BUSY_TX		0x02
#define STATE_FORCE_TRX_OFF	0x03
#define STATE_FORCE_TX_ON	0x04	/* IDLE */
/* 0x05 */				/* INVALID_PARAMETER */
#define STATE_RX_ON		0x06
/* 0x07 */				/* SUCCESS */
#define STATE_TRX_OFF		0x08
#define STATE_TX_ON		0x09
/* 0x0a - 0x0e */			/* 0x0a - UNSUPPORTED_ATTRIBUTE */
#define STATE_SLEEP		0x0F
#define STATE_PREP_DEEP_SLEEP	0x10
#define STATE_BUSY_RX_AACK	0x11
#define STATE_BUSY_TX_ARET	0x12
#define STATE_RX_AACK_ON	0x16
#define STATE_TX_ARET_ON	0x19
#define STATE_RX_ON_NOCLK	0x1C
#define STATE_RX_AACK_ON_NOCLK	0x1D
#define STATE_BUSY_RX_AACK_NOCLK 0x1E
#define STATE_TRANSITION_IN_PROGRESS 0x1F


// Set up pins
unsigned long ctrlLED = 13;
unsigned long IRQ = 9;
unsigned long RESET = 8;
unsigned long SLP_TR = 7;
unsigned long SEL = 6;

// unused shit:
//byte sPHR = 0B00000101; //Length of the frame to be sent (n + 2; n - Number of PSDU bytes)
//byte sFCF1 = 0B01000000;
//byte sFCF2 = 0B00000100;


//int ctrlZustand = LOW;
//long zuletzt = 0;
//long Intervall = 1000;

int received = 0;

void setup() {

  Serial.begin(115200);
  Serial.println("Booting AT86RF233 sniffer...");
//  pinMode(ctrlLED, OUTPUT); // disable LED for now
  pinMode(RESET, OUTPUT);
  pinMode(SLP_TR, OUTPUT);
  pinMode(IRQ, INPUT);
  pinMode(SEL, OUTPUT);

  // Set up SPI
  SPI.begin();
  //Data is transmitted and received MSB first
  SPI.setBitOrder(MSBFIRST);
  //SPI interface will run at 1MHz if 8MHz chip or 2Mhz if 16Mhz
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  //Data is clocked on the rising edge and clock is low when inactive
  SPI.setDataMode(SPI_MODE0);

  delay(10); // wait for SPI to be ready
  digitalWrite(SLP_TR, LOW);
  digitalWrite(RESET, HIGH);
  digitalWrite(SEL, HIGH);

  writeRegister(0x08, 0x0B); //Channel: 2405 MHz

  // Enable promiscuous mode:
  writeRegister(0x20, 0x00); //Short address bit[7:0]
  writeRegister(0x21, 0x00); //Short address bit[15:8]
  writeRegister(0x22, 0x00); //PAN ID bit[7:0]
  writeRegister(0x23, 0x00); //PAN ID bit[15:8]
  writeRegister(0x24, 0x00); //IEEE_ADDR_0
  writeRegister(0x25, 0x00); //IEEE_ADDR_1
  writeRegister(0x26, 0x00); //IEEE_ADDR_2
  writeRegister(0x27, 0x00); //IEEE_ADDR_3
  writeRegister(0x28, 0x00); //IEEE_ADDR_4
  writeRegister(0x29, 0x00); //IEEE_ADDR_5
  writeRegister(0x2A, 0x00); //IEEE_ADDR_6
  writeRegister(0x2B, 0x00); //IEEE_ADDR_7
  writeRegister(0x17, 0B00000010); //AACK_PROM_MODE
  writeRegister(0x2E, 0B00010000); //AACK_DIS_ACK

  byte TRX_CTRL_0 = readRegister(0x03);
  TRX_CTRL_0 = TRX_CTRL_0 | 0B10000000; //TOM mode enabled
  writeRegister(0x03, TRX_CTRL_0);
  Serial.print("Detected part nr: 0x");
  Serial.println(readRegister(0x1C), HEX);
  Serial.print("Version: 0x");
  Serial.println(readRegister(0x1D), HEX);

  delay(1);
}

void loop() {

  byte CurrentState = readRegister(0x01); //Page 37 of datasheet
  byte Interrupt = readRegister(0x0F);
  byte PHY_RSSI = readRegister(0x06); //if bit[7] = 1 (RX_CRC_VALID), FCS is valid

  //  unsigned long jetzt = millis();

  // This can be used to write status updates
  //  if (jetzt - zuletzt > Intervall)
  //  {
  //    zuletzt = jetzt;
  //
  //    Serial.print("Status report @ ");
  //    Serial.print(zuletzt);
  //    Serial.print(": ");
  //    Serial.print(CurrentState, HEX);
  //
  //    Serial.print("; Interrupt ");
  //    Serial.println(Interrupt, BIN);
  //
  //    if (ctrlZustand == LOW)
  //      ctrlZustand = HIGH;
  //    else
  //      ctrlZustand = LOW;
  //
  //    digitalWrite(ctrlLED, ctrlZustand);
  //
  //  }

  if (CurrentState == STATE_P_ON) { //P_ON
    writeRegister(0x0E, IRQ_CCA_ED); // Interrupt AWAKE_END (IRQ_4) enabled
    writeRegister(0x02, STATE_TRX_OFF); //Go from P_ON to TRX_OFF state
    delay(1);
  }
  if (CurrentState == STATE_TRX_OFF) { //TRX_OFF = 0x00
    writeRegister(0x0E, IRQ_PLL_LOCK); // Interrupt PLL_LOCK (IRQ_0) enabled
    writeRegister(0x02, STATE_TX_ON); //Go from TRX_OFF to PLL_ON state
    delay(0.016);
  }

  if (Interrupt & IRQ_PLL_LOCK) { //if PLL is locked
    writeRegister(0x0E, IRQ_RX_START & IRQ_TRX_END & IRQ_AMI); // Interrupts RX_START (IRQ_2), TRX_END (IRQ_3) and AMI (IRQ_5) enabled
    writeRegister(0x02, STATE_RX_AACK_ON); //Go from PLL_ON to RX_AACK_ON state
    // during RX_ON state, listen for incoming frame, BUSY_RX -> receiving frame, interrupt IRQ_TRX_END -> done
    delay(1);
  }

  if (Interrupt & IRQ_TRX_END) {
    // If we receive something, state will be BUSY_RX, interrupt IRQ_TRX_END when receive done
    readFrame();
    received++;
    Serial.print("Total frames received: ");
    Serial.println(received);
  }
}

// Read from register
byte readRegister(byte thisRegister) {
  byte result = 0x00;
  byte dataToSend = thisRegister | REG_READ; //delay (1000);
  digitalWrite(SEL, LOW);
  SPI.transfer(dataToSend);
  result = SPI.transfer(0x00);
  digitalWrite(SEL, HIGH);
  return (result);
}

// Write to register
void writeRegister(byte thisRegister, byte thisValue) {
  byte dataToSend = thisRegister | REG_WRITE; //delay (1000);
  digitalWrite(SEL, LOW);
  SPI.transfer(dataToSend);
  SPI.transfer(thisValue);
  digitalWrite(SEL, HIGH);
}

// Write to frame
void writeFrame(byte seqnr) {
//  digitalWrite(SEL, LOW);
//  SPI.transfer(FRAME_WRITE);
//  delay (0.16);
//  SPI.transfer(sPHR);
//  delay (0.032);
//  SPI.transfer(sFCF1); //transmitted data
//  delay (4.064);
//  SPI.transfer(sFCF2); //transmitted data
//  delay (4.064);
//  SPI.transfer(seqnr); //transmitted data
//  delay (4.064);
//  digitalWrite(SEL, HIGH);
}

// Read from frame
byte* readFrame() {
  digitalWrite(SEL, LOW); //initiate read access
  byte phystatus = SPI.transfer(CMD_FB_READ); //read from frame buffer
  // read first byte (PHY_STATUS, should be equal to 0b01100000)
  if (phystatus == 0x00) { // TODO: PHY_STATUS should be 0x0, can configure using a register
    Serial.print("Got frame: ");
    byte len = SPI.transfer(0x00);
    byte frame[len];
    for (int i = 0; i < len; i++) { // read 5 bytes
      frame[i] = SPI.transfer(0x00);
      Serial.print(frame[i], HEX);
    }
    Serial.println("");
    digitalWrite(SEL, HIGH); // stop reading
    return frame;

  } else {
    Serial.print("ERROR: PHY_STATUS: ");
    Serial.println(phystatus, HEX);
    return 0;
  }
}

//Read from SRAM (for time-of-flight)
byte readSRAM(byte beginwith) {
  byte resulti = 0x00;
  byte resultii = 0x00;
  byte resultiii = 0x00;
  digitalWrite(SEL, LOW);
  SPI.transfer(CMD_SRAM_READ);
  SPI.transfer(beginwith); //Read frame beginning with this register
  resulti = SPI.transfer(0x00); //Time-of-flight, bits [7:0] (register 7D)
  Serial.print("; bit[7:0]: ");
  Serial.print(resulti, HEX);
  resultii = SPI.transfer(0x00); //Time-of-flight, bits [15:8] (register 7E)
  Serial.print("; bit[15:8]: ");
  Serial.print(resultii, HEX);
  resultiii = SPI.transfer(0x00); //Time-of-flight, bits [23:16] (register 7F)
  Serial.print("; bit[23:16]: ");
  Serial.print(resultiii, HEX);
  digitalWrite(SEL, HIGH);
}
