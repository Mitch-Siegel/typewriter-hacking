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
volatile bool busReadBuf[1024];
volatile uint64_t busReadTimesBuf[1024];
volatile uint16_t busReadP = 0;
volatile uint16_t logicChanges[100];
volatile uint16_t logicChangesP = 0;

hw_timer_t *readTimer = NULL;
// volatile uint8_t timesCalled = 0;
void IRAM_ATTR busReaderInterruptHandler()
{
	busReadBuf[busReadP] = readInPinDMA();
	if (readingBus)
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

	// 1MHz timer
	// readTimer = timerBegin(0, 24, true);
	// timerStart(readTimer);

	// timerAttachInterrupt(readTimer, &busReaderInterruptHandler, true);
	// timerAlarmWrite(readTimer, 1, true);
	// timerAlarmEnable(readTimer);
	// timerStart(readTimer);
}

uint16_t IRAM_ATTR readBusDMA()
{
	busReadP = 0;

	unsigned int startMicros = micros();
	unsigned int thisMicrosOffset;

	while ((thisMicrosOffset = (micros() - startMicros)) < 55)
	{
		busReadBuf[busReadP] = readInPinDMA();
		busReadTimesBuf[busReadP++] = thisMicrosOffset;
	}
	Serial.print("n reads: ");
	Serial.println(busReadP);
	Serial.print("micros:");
	Serial.println(micros() - startMicros);

	uint16_t logicChanges[10];
	uint8_t logicChangesP = 0;
	for (int i = 1; i < busReadP; i++)
	{
		if (busReadBuf[i] != busReadBuf[i - 1])
		{
			logicChanges[logicChangesP++] = busReadTimesBuf[i];
		}
	}

	float logicChangeTimes[12] = {0.0};
	uint8_t logicChangeTimesP = 0;
	float speedMultiplier = 0;
#define FIXED_POINT_MULTIPLIER 10000
	// float avgChangeTime;
	for (int i = 0; i < logicChangesP; i++)
	{
		Serial.print("Level change @ ");
		Serial.print(logicChanges[i]);
		Serial.print("\t");
		Serial.print(i);
		Serial.print("\t");
		
		float expectedBit = ((logicChanges[i] * FIXED_POINT_MULTIPLIER) / (.535 * FIXED_POINT_MULTIPLIER));
		float bitTiming = (expectedBit / floor(expectedBit));
		logicChangeTimes[logicChangeTimesP++] = bitTiming;
		speedMultiplier += bitTiming;
		Serial.print(logicChangeTimes[logicChangeTimesP - 1]);
		Serial.println();
	}
	char floatSpeed[10];
	speedMultiplier /= (logicChangeTimesP);
	sprintf(floatSpeed, "%1.5f", speedMultiplier);

	Serial.print("Overall speed of ");
	Serial.print(floatSpeed);
	Serial.println("x expected");
	
	uint16_t value = 0;
	/*
	bool bitState = 0;
	for (int i = 0; i < 10; i++)
	{
		if (logicChangeTimes[i])
		{
			bitState = !bitState;
		}
		value <<= 1;
		value |= bitState;
	}
	*/

	// Serial.print("Avg bit time (us): ");
	// Serial.println(avgChangeTime);

	for (int i = 0; i < busReadP; i++)
	{
		for (size_t j = Serial.print(busReadBuf[i]); j < 3; j++)
		{
			Serial.print(" ");
		}
	}
	Serial.println();
	for (int i = 0; i < busReadP; i++)
	{
		for (size_t j = Serial.print(busReadTimesBuf[i]); j < 3; j++)
		{
			Serial.print(" ");
		}
	}
	Serial.println();

	return value;
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
