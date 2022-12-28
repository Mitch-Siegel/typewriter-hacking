#include "esp_system.h"

#define PIN_READ 4

// DueTimer readTimer = DueTimer(0);

inline bool readInPinDMA()
{
	return (REG_READ(GPIO_IN_REG) >> PIN_READ) & 0b1;
	// return ((REG_PIOB_PDSR) >> 25) & 0x01;
}

volatile int bitsRead = 0;

volatile uint16_t currentBusRead;

volatile bool readingBus;
volatile bool busReadBuf[4096]; // 1024 * 100ns = 102.4us
volatile uint16_t busReadP = 0;
volatile uint16_t logicChanges[100];
volatile uint16_t logicChangesP = 0;

hw_timer_t *readTimer = NULL;
// volatile uint8_t timesCalled = 0;
void IRAM_ATTR busReaderInterruptHandler()
{
	busReadBuf[busReadP] = readInPinDMA();
	if(readingBus)
	{
		busReadP++;
	}
	// if(busReadP >= 60)
	// {
	// readingBus = false;
	// }
	/*
	if(busReadBuf[busReadP] != busReadBuf[busReadP - 1])
	{
		logicChanges[logicChangesP++] = busReadP;
	}*/
	// busReadP++;
}

void setup()
{
	// readTimer.attachInterrupt(busReaderInterruptHandler);
	//  readTimer.setFrequency(190476); // 5.25 us
	//  readTimer.setFrequency(187265); // 5.34 us (real)
	// readTimer.setFrequency(192265);
	pinMode(PIN_READ, INPUT);

	Serial.begin(500000);
	delay(200);
	setCpuFrequencyMhz(240);
	Serial.print("Initialized CPU at ");
	Serial.print(getCpuFrequencyMhz());
	Serial.println("MHz");

	// 10MHz timer
	readTimer = timerBegin(0, 24, true);

	timerAttachInterrupt(readTimer, &busReaderInterruptHandler, true);
	timerAlarmWrite(readTimer, 50, true);
	timerAlarmEnable(readTimer);
	timerStart(readTimer);
}

uint16_t readBusDMA()
{
	Serial.println("READBUSDMA FUNCTION");
	unsigned int startMicros = micros();

	busReadP = 0;
	logicChangesP = 0;
	readingBus = true;
	timerStart(readTimer);
	delayMicroseconds(55);
	// sample for 55us
	// while ((busReadP < (55)))
	// {
		// Serial.println(busReadP);
	// }
	timerStop(readTimer);
	readingBus = false;

	// timerAlarmDisable(readTimer);
	Serial.print("Read function duration (us): ");
	Serial.println(micros() - startMicros);
	Serial.print("BusReadP:");
	Serial.println(busReadP);
	/*
	unsigned int startMicros = micros();

	busReadP = 0;
	logicChangesP = 0;

	timerWrite(readTimer, 0);
	timerStart(readTimer);
	bitsRead = 1;
	currentBusRead = 0;
	while((logicChangesP < 10) && (busReadP < (54 * 10)))
	{
		Serial.print("busReadP:");
		Serial.println(busReadP);
	}
	timerStop(readTimer);
	Serial.println(micros() - startMicros);
	return currentBusRead;*/
	return 0;
}

uint16_t busBuffer[16384];
uint16_t busBufP = 0;
void loop()
{
	//  Serial.print(digitalRead(PIN_READ));
	static byte lastRead = 1;
	static uint32_t counter = 0;
	static bool printSeparator = false;
	byte thisRead = readInPinDMA();
	static uint32_t checksum = 0;
	static uint32_t lastChecksum;
	if (thisRead != lastRead)
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
		if (printSeparator && (++counter >= ((2 << 18) - 1)))
		{
			Serial.print(busBufP);
			Serial.println(" Bus reads in buffer");
			for (int i = 0; i < busBufP; i++)
			{
				/*
				for(int j = 0; j < 3; j++)
					{
						uint8_t thisNibble = (busBuffer[i] >> (j * 4)) & 0xf;
						if(thisNibble < 58)
						{
							Serial.write(thisNibble + 48);
						}
						else
						{
							Serial.write(thisNibble + 65);
						}
					}
					Serial.println();
					*/
				for (int j = 0; j < 10; j++)
				{
					Serial.print((busBuffer[i] >> j) & 0b1);
				}
				Serial.print(":");

				Serial.println(busBuffer[i]);
			}
			counter = 0;
			Serial.println("Done... for now - checksums: (current, last)\n");
			Serial.print(checksum);
			Serial.print(", ");
			Serial.print(lastChecksum);
			Serial.print(", ");
			Serial.println((int)checksum - (int)lastChecksum);
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
