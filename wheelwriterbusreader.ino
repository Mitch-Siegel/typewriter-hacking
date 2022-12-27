#include <DueTimer.h>

#define PIN_READ 2

DueTimer readTimer = DueTimer(0);


inline bool readInPinDMA()
{
  return ((REG_PIOB_PDSR) >> 25) & 0x01;
}

volatile int bitsRead = 0;

volatile uint16_t currentBusRead;

void busReaderInterruptHandler()
{
  currentBusRead <<= 1;
  currentBusRead |= readInPinDMA();
  bitsRead++;
}


void setup() {
  readTimer.attachInterrupt(busReaderInterruptHandler);
//  readTimer.setFrequency(190476); // 5.25 us
  readTimer.setFrequency(187265); // 5.34 us (real)
  pinMode(PIN_READ, INPUT);
  SerialUSB.begin(115200);
  delay(200);
  SerialUSB.println("test");
}

uint16_t readBusDMA() {
    delayMicroseconds(3);
    readTimer.start();
    bitsRead = 1;
    currentBusRead = 1;
    
    while(bitsRead < 10)
    {
    }
    readTimer.stop();
    return currentBusRead;
    
  return currentBusRead;
}


uint16_t busBuffer[1024];
uint16_t busBufP = 0;
void loop() {
//  SerialUSB.println("test");
//  SerialUSB.print(digitalRead(PIN_READ));
  static byte lastRead = 1;
  static uint32_t counter = 0;
  static bool printSeparator = false;
  byte thisRead = readInPinDMA();
  static uint32_t checksum = 0;
  static uint32_t lastChecksum;
  if(thisRead != lastRead)
  {
    uint16_t bussy = readBusDMA();
    busBuffer[busBufP++] = bussy;

    checksum += bussy;
    
    printSeparator = true;
    lastRead = 1;
    counter = 0;
  }
  else
  {
    lastRead = thisRead;
    if(printSeparator && (++counter >= (2 << 17 - 1)))
    {
      SerialUSB.print(busBufP);
      SerialUSB.println(" Bus reads in buffer");
      for(int i = 0; i < busBufP; i++)
      {
        /*
        for(int j = 0; j < 3; j++)
          {
            uint8_t thisNibble = (busBuffer[i] >> (j * 4)) & 0xf;
            if(thisNibble < 58)
            {
              SerialUSB.write(thisNibble + 48);
            }
            else
            {
              SerialUSB.write(thisNibble + 65);
            }
          }
          SerialUSB.println();
          */
          SerialUSB.println(busBuffer[i]);
      }
      counter = 0;
      SerialUSB.println("Done... for now - checksums: (current, last)\n");
      SerialUSB.print(checksum);
      SerialUSB.print(", ");
      SerialUSB.print(lastChecksum);
      SerialUSB.print(", ");
      SerialUSB.println((int)checksum - (int)lastChecksum);
      lastChecksum = checksum;
      checksum = 0;
      printSeparator = false;

      busBufP = 0;
    }
      
  }
}


/*
 * single push of the letter P:
 36 Bus reads in buffer
531
513
833
513
531
513
705
513
881
513
905
721
531
513
769
513
745
513
833
513
531
513
705
513
881
513
649
517
531
513
705
513
881
513
897
721

 */
