#include "esp32-hal-gpio.h"

#define PIN_READ 4
#define PIN_WRITE 5

// DueTimer readTimer = DueTimer(0);

inline bool readInPinDMA()
{
	return (REG_READ(GPIO_IN_REG) >> PIN_READ) & 0b1;
	// return ((REG_PIOB_PDSR) >> 25) & 0x01;
}

void IRAM_ATTR digitalWriteFast(uint8_t pin, uint8_t val)
{
	if (val)
	{
		if (pin < 32)
		{
			GPIO.out_w1ts = ((uint32_t)1 << pin);
		}
		else if (pin < 34)
		{
			GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
		}
	}
	else
	{
		if (pin < 32)
		{
			GPIO.out_w1tc = ((uint32_t)1 << pin);
		}
		else if (pin < 34)
		{
			GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
		}
	}
}

volatile uint8_t busReadBuf[128];
volatile uint32_t busReadTimesBuf[128];
volatile uint16_t busReadP = 0;

hw_timer_t *readTimer = NULL;

void setup()
{
	pinMode(PIN_READ, INPUT);
	pinMode(PIN_WRITE, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(PIN_WRITE, 0);

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

	float sampleTime = 5.34;
	// float nextSampleAt = sampleTime / 2;
	float nextSampleAt = 1;
	int sampleScanP = 0;
	uint16_t value = 0;
	for (int i = 0; i < 10; i++)
	{
		value <<= 1;
		while (busReadTimesBuf[sampleScanP] < nextSampleAt)
		{
			sampleScanP++;
		}
		value |= ((busReadBuf[sampleScanP] +
				   busReadBuf[sampleScanP + 1] +
				   busReadBuf[sampleScanP + 2]) >= 2);
		nextSampleAt += sampleTime;
	}

	return (value >> 1) & 0xff;
}

void IRAM_ATTR baudDelay()
{
	for (volatile int j = 0; j < 0x50; j++)
	{
	}
}

void IRAM_ATTR sendByte(byte toSend)
{
	byte reversed = 0;
	for (int i = 0; i < 8; i++)
	{
		reversed |= ((toSend >> (7 - i)) & 0b1) << i;
	}

	// invert all writes (1 switches the transistor and pulls the bus low)
	digitalWriteFast(PIN_WRITE, 1);
	baudDelay();
	for (int i = 0; i < 8; i++)
	{
		digitalWriteFast(PIN_WRITE, !(reversed & 0b1));
		reversed >>= 1;
		baudDelay();
		// spin for a bit to eat up the extra time
	}
	digitalWriteFast(PIN_WRITE, 0);
	baudDelay();
	// digitalWriteFast(LED_BUILTIN, 0);
	delayMicroseconds(1000); // wait before sending the next thing
}

uint16_t busBuffer[16384];
uint16_t busBufferCopy[16384];
uint16_t busBufP = 0;
uint16_t busBufPCopy = 0;
uint32_t typingCounter = 0;
bool didSomething = true;

uint8_t printA[36] = {33, 0, 11, 0, 33, 0, 12, 0, 4, 0, 70, 16, 33, 0, 3, 0, 1, 0, 10, 0, 33, 0, 12, 0, 4, 0, 68, 128, 33, 0, 12, 0, 4, 0, 6, 16};
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
		uint8_t bussy = readBusDMA();
		uint8_t reversed = 0;
		for (int i = 0; i < 8; i++)
		{
			reversed |= ((bussy >> (7 - i)) & 0b1) << i;
		}
		if (busBufP > 1023)
		{
			Serial.println("Buffer overflowing!");
		}
		else
		{
			busBuffer[busBufP++] = reversed;

			checksum += reversed;

			printSeparator = true;
			lastRead = 1;
			counter = 0;
		}
	}
	else
	{
		// lastRead = thisRead;
		if (printSeparator && (++counter >= ((2 << 19) - 1)))
		{
			Serial.print(busBufP);
			Serial.print(" Bus reads in buffer\t");
			Serial.print("checksums:(curr:");
			for (int i = Serial.print(checksum); i < 4; i++)
			{
				Serial.print(" ");
			}
			Serial.print(", last:");
			for (int i = Serial.print(lastChecksum); i < 4; i++)
			{
				Serial.print(" ");
			}
			Serial.print(", diff:");
			Serial.print((int)checksum - (int)lastChecksum);
			Serial.println(")");

			for (int i = 0; i < busBufP; i++)
			{

				switch (Serial.print(busBuffer[i]))
				{
				case 1:
					Serial.print(" ");
					break;

				case 2:
					Serial.print(" ");
					break;

				default:
					break;
				}

				if (i != busBufP - 1)
				{
					Serial.print(", ");
				}
				else
				{
					Serial.println();
				}
			}

			Serial.println();

			counter = 0;
			lastChecksum = checksum;
			checksum = 0;
			printSeparator = false;

			memcpy(busBufferCopy, busBuffer, (busBufP + 1) * sizeof(uint16_t));
			busBufPCopy = busBufP;
			didSomething = false;

			busBufP = 0;
		}
		else
		{
			if (!didSomething)
			{
#define RESEND_INTERVAL 500000
				if (++typingCounter > RESEND_INTERVAL)
				{
					Serial.println("TRYING TO DO SOMETHING WITH DA BUS");
					digitalWriteFast(LED_BUILTIN, 1);
					// noInterrupts();
					Serial.print("Let's send this many bytes: ");
					Serial.println(busBufPCopy);
					for (int i = 0; i < busBufPCopy; i++)
					{
						// Serial.print("Byte ");
						// Serial.print(i);
						// Serial.print(":");
						// Serial.println(busBufferCopy[i]);
						sendByte(busBufferCopy[i]);
					}
					Serial.println("Sent em");
					// interrupts();
					typingCounter = 0;
					didSomething = true;
					digitalWriteFast(LED_BUILTIN, 0);
					/*
					for(int i = 0; i < 36; i++)
					{
						sendByte(printA[i]);
					}
					*/
				}
				else if (typingCounter % (RESEND_INTERVAL / 100) == 0)
				{
					Serial.print("Waiting to do something: ");
					Serial.println(RESEND_INTERVAL - typingCounter);
				}
			}
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
