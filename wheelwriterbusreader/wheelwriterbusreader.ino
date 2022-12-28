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

void setup()
{
	pinMode(PIN_READ, INPUT);

	Serial.begin(500000);
	delay(200);
	setCpuFrequencyMhz(240);
	Serial.print("Initialized CPU at ");
	Serial.print(getCpuFrequencyMhz());
	Serial.println("MHz");
}

uint8_t IRAM_ATTR readBusDMA()
{
	busReadP = 0;

	unsigned int startMicros = micros();
	unsigned int thisMicrosOffset;

	while ((thisMicrosOffset = (micros() - startMicros)) < 54)
	{
		busReadBuf[busReadP] = readInPinDMA();
		busReadTimesBuf[busReadP++] = thisMicrosOffset;
	}

	float sampleTime = 5.25;
	// float nextSampleAt = sampleTime / 2;
	float nextSampleAt = sampleTime / 3;
	int sampleScanP = 0;
	uint16_t value = 0;
	for (int i = 0; i < 10; i++)
	{
		value <<= 1;
		while (busReadTimesBuf[sampleScanP] < nextSampleAt)
		{
			sampleScanP++;
		}
		value |= busReadBuf[sampleScanP];
		nextSampleAt += sampleTime;
	}

	return (value >> 1) & 0xff;
}

uint16_t busBuffer[16384];
uint16_t busBufP = 0;
void loop()
{
	//  Serial.print(digitalRead(PIN_READ));
	static byte lastRead = 1;
	static uint32_t counter = 0;
	static bool printSeparator = false;
	static uint32_t checksum = 0;
	static uint32_t lastChecksum;


	byte thisRead = readInPinDMA();
	
	if (thisRead != lastRead)
	{
		// delayMicroseconds((micros() % 5) / 2);
		// delayMicroseconds(2);
		uint16_t bussy = readBusDMA();
		busBuffer[busBufP++] = bussy;

		checksum += bussy;

		printSeparator = true;
		lastRead = 1;
		counter = 0;
	}
	else
	{
		// lastRead = thisRead;
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
				for (int j = 0; j < 8; j++)
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
