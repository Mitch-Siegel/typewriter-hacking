/*
	IBM Wheelwriter hack
	Pin 0: connected through MOSFET to Wheelwriter bus
				 Will pull down the bus when set to zero
	Pin 2: ALSO connected to the bus, but listens instead of
				 sending data
 */
// #include <LedControl.h>

#define PIN_READ 2
#define PIN_WRITE 4

#define DIN 6
#define CS 7
#define CLK 8
// LedControl sseg(DIN, CLK, CS);

// #include<PinChangeInt.h>

#define LETTER_DELAY 450
#define CARRIAGE_WAIT_BASE 200
// #define CARRIAGE_WAIT_MULTIPLIER 15
#define CARRIAGE_WAIT_MULTIPLIER 10

#define busWrite1() REG_WRITE (GPIO_OUT_W1TC_REG, BIT4); // GPIO LOW

#define busWrite0() REG_WRITE (GPIO_OUT_W1TS_REG, BIT4); // GPIO HIGH

// #define busWrite1() PORTD &= 0b11111011

// #define busWrite0() PORTD |= 0b00000100

#define busRead() REG_READ(GPIO_OUT_REG) & (1 << (PIN_READ))

#define busWriteBit(command, bitmask) (command & bitmask) ? busWrite1() : busWrite0()

bool bold = false;		  // off to start
bool underline = false;	  // off to start
bool reverseText = false; // for printing in reverse
bool wordWrap = true;	  // when printing words, dump out to a newline if the word will span the end of one line and the beginning of the next

#define LED 13 // the LED light

// #define nop1_help __asm__ __volatile__("nop\n\t"); // 62.5ns	or 1 cycles
#define nop1_help "nop\n\t"				 // 62.5ns	or 1 cycles
#define nop2_help nop1_help nop1_help	 // 125ns	or 2 cycles
#define nop4_help nop2_help nop2_help	 // 250ns	or 4 cycles
#define nop8_help nop4_help nop4_help	 // 500ns	or 8 cycles
#define nop16_help nop8_help nop8_help	 // 1us	or 16 cycles
#define nop32_help nop16_help nop16_help // 2us	or 32 cycles
#define nop64_help nop32_help nop32_help // 4us	or 64 cycles

#define nop1 __asm__ __volatile__(nop1_help);	// 62.5ns	or 1 cycles
#define nop2 __asm__ __volatile__(nop2_help);	// 125ns	or 2 cycles
#define nop4 __asm__ __volatile__(nop4_help);	// 250ns	or 4 cycles
#define nop8 __asm__ __volatile__(nop8_help);	// 500ns	or 8 cycles
#define nop16 __asm__ __volatile__(nop16_help); // 1us	or 16 cycles
#define nop32 __asm__ __volatile__(nop32_help); // 2us	or 32 cycles
#define nop64 __asm__ __volatile__(nop64_help); // 4us	or 64 cycles

void pulseDelay()
{
	for (int i = 0; i < 199; i++)
	{
		nop1;
	}
	nop2;
}

// on an Arduino Nano, ATmega328, the following will delay roughly 5.25us
// #define pulseDelay()                         \
	// {                                        \
		// for (int i = 0; i < 16; i++)         \
		// {                                    \
			// __asm__ __volatile__("nop\n\t"); \
		// }                                    \
	// }

// on an Arduino Nano, ATmega328, the following will delay roughly 5.25us
#define pulseDelayJank()                     \
	{                                        \
		for (int i = 0; i < 16; i++)         \
		{                                    \
			__asm__ __volatile__("nop\n\t"); \
		}                                    \
	}

#define pulseDelayForRead()                  \
	{                                        \
		for (int i = 0; i < 15; i++)         \
		{                                    \
			__asm__ __volatile__("nop\n\t"); \
		}                                    \
	}

byte asciiTrans[128] =
	// col: 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f     row:
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0

	 //
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1

	 // sp     !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /
	 0x00, 0x49, 0x4b, 0x38, 0x37, 0x39, 0x3f, 0x4c, 0x23, 0x16, 0x36, 0x3b, 0xc, 0x0e, 0x57, 0x28, // 2

	 // 0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
	 0x30, 0x2e, 0x2f, 0x2c, 0x32, 0x31, 0x33, 0x35, 0x34, 0x2a, 0x4e, 0x50, 0x00, 0x4d, 0x00, 0x4a, // 3

	 // @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
	 0x3d, 0x20, 0x12, 0x1b, 0x1d, 0x1e, 0x11, 0x0f, 0x14, 0x1F, 0x21, 0x2b, 0x18, 0x24, 0x1a, 0x22, // 4

	 // P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
	 0x15, 0x3e, 0x17, 0x19, 0x1c, 0x10, 0x0d, 0x29, 0x2d, 0x26, 0x13, 0x41, 0x00, 0x40, 0x00, 0x4f, // 5

	 // `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o
	 0x00, 0x01, 0x59, 0x05, 0x07, 0x60, 0x0a, 0x5a, 0x08, 0x5d, 0x56, 0x0b, 0x09, 0x04, 0x02, 0x5f, // 6

	 // p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~    DEL
	 0x5c, 0x52, 0x03, 0x06, 0x5e, 0x5b, 0x53, 0x55, 0x51, 0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00}; // 7

byte asciiTransRev[128];

void setup()
{
	// initialize serial communication at 115200 bits per second:
	Serial.begin(300);
	Serial.setTimeout(25);

	for (int i = 0; i < 0x100; i++)
	{
		asciiTransRev[asciiTrans[i]] = i;
	}

	// Digital pins
	pinMode(PIN_WRITE, OUTPUT);		  // pin that will trigger the bus
	pinMode(PIN_READ, INPUT); // listening pin

	pinMode(LED, OUTPUT);

	// start the input pin off (meaning the bus is high, normal state)
	digitalWrite(PIN_WRITE, 0);
	// PORTD &= ~(1 << PIN_READ - 1);
	// sseg.shutdown(0, false);
	// sseg.clearDisplay(0);
}

// test print string:
// 0000000001111111111222222222233333333334444444444555555555566666666667777777777812345678901234567890123456789012345678901234567890123456789012345678901234567890

// take in a char, send it along to the typewriter if it can print it natively, or fudge it if not
int printAscii(char c, int charCount)
{
	switch (c)
	{
	case '<':
	{
		micro_backspace(2);
		paper_vert(0, 1);
		letterMicrospace(asciiTrans['.']);

		paper_vert(0, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(0, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 2);
		micro_backspace(4);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);

		paper_vert(0, 1);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	case '>':
	{
		micro_backspace(2);

		paper_vert(0, 3);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterNoSpace(asciiTrans['.']);
		paper_vert(1, 2);
		micro_backspace(4);
		letterMicrospace(asciiTrans['.']);
		paper_vert(0, 1);
		letterMicrospace(asciiTrans['.']);

		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	case '|':
	{
		letterNoSpace(asciiTrans[':']);
		paper_vert(0, 1);
		letterNoSpace(asciiTrans[':']);
		paper_vert(0, 1);
		letterNoSpace(asciiTrans[':']);
		paper_vert(1, 2);
		printOne(asciiTrans[' '], charCount);
	}
		return charCount + 1;
		break;

	case '^':
	{
		micro_backspace(2);
		paper_vert(0, 3);
		letterMicrospace(asciiTrans['.']);
		paper_vert(0, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 3);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	case '{':
	{
		letterNoSpace(asciiTrans['-']);
		letterMicrospace(asciiTrans['(']);
		letterMicrospace(asciiTrans['[']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	case '}':
	{
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[']']);
		letterNoSpace(asciiTrans[')']);
		letterMicrospace(asciiTrans['-']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	case '\\':
	{
		micro_backspace(2);
		paper_vert(0, 4);

		letterNoSpace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterNoSpace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(0, 1);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
	break;

	case '~':
	{
		paper_vert(0, 5);
		letterNoSpace(asciiTrans['_']);
		paper_vert(1, 2);
		micro_backspace(2);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 2);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	case '`':
	{
		paper_vert(0, 5);
		letterMicrospace(asciiTrans['.']);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 1);
		letterMicrospace(asciiTrans['.']);
		paper_vert(1, 4);
		// letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
		letterMicrospace(asciiTrans[' ']);
	}
		return charCount + 1;
		break;

	default:
		return printOne(c, charCount);
		break;
	}
}

template <class T>
class Queue
{
private:
	T *buf;
	size_t size;
	uint16_t nextP; // index of next output value to be gotten
	uint16_t endP;	// index where next input value will be placed

public:
	Queue(size_t size)
	{
		this->buf = new T[size];
		this->size = size;
	}

	~Queue()
	{
		delete[] this->buf;
	}

	// add to the queue
	void Add(T c)
	{
		this->buf[this->endP++] = c;
		this->endP %= this->size;
	}

	// return the number of chars in the queue
	int Available()
	{
		if (this->endP >= this->nextP)
		{
			return this->endP - this->nextP;
		}
		else
		{
			return ((this->size - this->nextP) - this->endP);
		}
	}

	// get the next char from the queue (should always be wrapped in an available() check first as this will permit overruns)
	T Next()
	{
		T toReturn = this->buf[this->nextP++];
		this->nextP %= this->size;
		return toReturn;
	}

	T Lookahead(size_t n)
	{
		return this->buf[(this->nextP + n) % this->size];
	}

	void Clear()
	{
		this->nextP = this->endP;
	}
};

Queue<char> printQueue = Queue<char>(1024);
Queue<uint16_t> busReadQueue = Queue<uint16_t>(128);

void LCD_WriteNumber(int number, int nDigits, int startDigit)
{
	for (int i = 0; i < nDigits; i++)
	{
		// sseg.setDigit(0, i + startDigit, number % 10, false);
		number /= 10;
	}
}

void printRun(uint16_t runLength, char c)
{
	static bool inFastText = false;
	if (c == '\n')
	{
		fastTextFinish();
		// delay(LETTER_DELAY);
		paper_vert(1, 1); // Move down one pixel
		if (runLength > 0)
		{
			micro_backspace(runLength * 3);
		}
		inFastText = false;
	}
	else
	{
		if (c == ' ')
		{
			// turn off fastText if it is on
			if (inFastText)
			{
				fastTextFinish();
				inFastText = false;
			}
			forwardSpaces(runLength);
		}
		else if (!inFastText)
		{
			fastTextInit();
			inFastText = true;
			fastTextCharsMicro(c, runLength);
		}
	}
}

bool gfxMode = false;

// the loop routine runs over and over again forever:
void loop()
{
	static int charCount = 0;
	static bool lastGfxMode = false;

	// char buffer[1024]; // let's make sure we actually have some room to buffer text here
	// uint8_t readLength = 65;
	while (Serial.available())
	{
		printQueue.Add(Serial.read());
	}
	size_t available = printQueue.Available();
	LCD_WriteNumber(available, 4, 0);
	if (gfxMode != lastGfxMode)
	{
		// sseg.setChar(0, 7, gfxMode ? '6' : ' ', false);
		lastGfxMode = gfxMode;
	}

	if (available)
	{
		digitalWrite(LED, 1);

		// handle graphics if we are in that mode
		if (gfxMode)
		{
			// gfx commands come in sets of 3 bytes, so make sure we can read all of them at once
			if (available < 2)
			{
				return;
			}

			char command = printQueue.Next();
			// Serial.print("Command: ");
			// Serial.println(command);
			switch (command)
			{
			case 'n':
				// Serial.print("NL: ");
				// Serial.println(charCount);
				paper_vert(1, 1);
				while (charCount > 0)
				{
#define MAX_MICROBACKSPACES_AT_ONCE 10
					if (charCount > MAX_MICROBACKSPACES_AT_ONCE)
					{
						micro_backspace(MAX_MICROBACKSPACES_AT_ONCE * 4);

						charCount -= MAX_MICROBACKSPACES_AT_ONCE;
					}
					else
					{
						micro_backspace(charCount * 4);
						charCount = 0;
					}
				}
				charCount = 0;
				printQueue.Next();
				break;

			case '1':
			case '0':

			{
				uint8_t runLength = printQueue.Next() - '0';
				// Serial.print("1: ");
				// Serial.println(runLength);
				char toPrint = (command == '1' ? asciiTrans['.'] : asciiTrans[' ']);

				/*if(command != '1')
				{
					while(runLength >= 5)
					{
						send_letter(asciiTrans[' ']);
						send_letter(asciiTrans[' ']);
						runLength -= 5;
						charCount += 5;
					}
				}*/
				for (int i = runLength; i > 0; i--)
				{
					LCD_WriteNumber(i, 3, 4);
					letterMicrospace(toPrint);
					letterMicrospace(asciiTrans[' ']);
				}
				charCount += runLength;
			}
			break;

			default:
				Serial.print("Saw something unexpected in the RLE! (");
				Serial.print(command);
				Serial.println(")");
				break;
			}
		}
		// otherwise just print out text
		else
		{
			static bool skipNext = false;

			if (skipNext)
			{
				printQueue.Next();
				skipNext = false;
				return;
			}

			// Serial.println(printQueue.Available());
			// Serial.print("Read ");
			// Serial.println(bytesRead);
			// Serial.println(" bytes.");

			if (wordWrap && isWhitespace(printQueue.Lookahead(0)))
			{
				int charsRemaining = 80 - charCount;
				for (size_t i = 1; i < available; i++)
				{
					if (isWhitespace(printQueue.Lookahead(i)))
					{
						break;
					}
					else
					{
						charsRemaining--;
					}
				}
				if (charsRemaining <= 0)
				{
					skipNext = true;
					charCount = printOne('\r', charCount);
					return;
				}
			}

			charCount = printAscii(printQueue.Next(), charCount);
			// wrap to a new line at 80 characters
			if (charCount == 80)
			{
				charCount = printOne('\r', charCount);
			}
		}
	}
	else
	{
		digitalWrite(LED, 0);
	}

	/*
	static bool lastRead = 1;
	bool read;
	static uint16_t messages[64];
	static uint8_t messageP = 0;
	if ((read = busRead()) != lastRead)
	{
		noInterrupts();
#define nop0 __asm__ __volatile__("nop\n\t"); // 62.5ns
#define nop1 nop0 nop0						  // 125ns
#define nop2 nop1 nop1						  // 250ns
#define nop3 nop2 nop2						  // 500ns
#define nop4 nop3 nop3						  // 1us
#define nop5 nop4 nop4						  // 2us
#define nop6 nop5 nop5						  // 4us
		uint16_t busRead = read;
		nop6;
		nop4;
		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		busRead |= busRead();
		nop6;
		nop4;

		busRead <<= 1;
		lastRead = busRead();
		busRead |= lastRead;
		nop6;
		nop4;
		messages[messageP++] = busRead;
		interrupts();
		sinceBusReadCounter = 65000;
	}
	*/

	// read one byte, and see if it is a command
	// Commands:
	// 0: next two bytes will be the number of characters we are going
	//    to send
	// 1: image bits (next two bytes will be the width and height)
	// 2: Toggle Bold
	// 3: Toggle Underline
	// 4: Reset typewriter
	// 5: TBA
	// 6: TBA
	// 7: Beep typewriter (traditional 0x07 beep char)
	// If the byte is >= 5, just treat as one character
	// bytesRead = Serial.readBytes(buffer, 1);

	// treat !!x as a command 'x', everything else as text
	/*
	if ((buffer[0] == '!') && (buffer[1] == '!'))
	{
		switch (buffer[2])
		{
		default:
			break;
		}
	}
	else
	{
		digitalWrite(LED, 1);
		// Serial.print("Read ");
		// Serial.println(bytesRead);
		// Serial.println(" bytes.");
		for (int i = 0; i < bufP; i++)
		{
			charCount = printAscii(buffer[i], charCount);
			// wrap to a new line at 80 characters
			if (charCount == 80)
			{
				charCount = printOne('\r', charCount);
			}
		}
	}
	*/
	// Serial.println(int(command));

	/*
	if (command == 0) { // text to print
		// look for next two bytesp
		Serial.readBytes(buffer,2);
		bytesToPrint = buffer[0] + (buffer[1] << 8); // little-endian
		charCount = printAllChars(buffer,bytesToPrint,charCount);
	}
	else if (command == 1) { // image to print
		//Serial.println("image");
		printImage(buffer);
	} else if (command == 2) {
		bold = !bold; // toggle
		Serial.println("ok");
	} else if (command == 3) {
		underline = !underline;
		Serial.println("ok");
	} else if (command == 4) {
		// reset the typewriter so we know we are on the begining of a line
		resetTypewriter();
		bold = underline = reverseText = false;
		Serial.println("ok");
	} else if (command == 5) {
		// reserved for future use
		Serial.println("ok");
	} else if (command == 6) {
		// reserved for future use
		Serial.println("ok");
	} else if (command == 7) {
		beepTypewriter();
		Serial.println("ok");
	}
	else {
		charCount = printOne(command,charCount);
		if(charCount == 10)
		{
			charCount = printOne('\r',charCount);
		}
	}
	*/
	// digitalWrite(LED, 0); // turn off LED when we are finished processing
	// }

	/*
	// button for testing
	byte button = digitalRead(4);
	if (button == 0) {
		digitalWrite(13,1); // led
		delay(100);
		digitalWrite(13,0);
		resetTypewriter();
	}
	*/
	// delay(10);
}
/*
int plusOne(int a)
{
	return a + 1;
}

*/

int printOne(byte charToPrint, int charCount)
{
	// print the character, and return the number of chars on the line that are left.
	// E.g., if we start with 6 and we print a character, then we return 7.
	// But, if we print a return, then that clears the charCount to 0, and if we
	// go left by one, that subtracts it, etc.

	/*
	if (charToPrint < 0) {
		charToPrint += 256; // correct for signed char
	}*/

	switch (charToPrint)
	{

	case '\r':
	/*{
		send_cr(charCount);
		charCount = 0;
	}
	break;*/
	case '\n':
	{
		send_crlf(charCount);
		charCount = 0;
	}
	break;

	case '\t':
	{
		send_tab();
		charCount += 5;
	}
	break;

	case '\200':
	case '\201':
	{
		paper_vert((charToPrint == '\200' ? 0 : 1), 8); // full line up/down
	}
	break;

	case '\004':
	{ // micro-down, ctrl-d is 4
		paper_vert(1, 1);
	}
	break;

	case '\025':
	{ // micro-up, ctrl-u is 21
		paper_vert(0, 1);
	}
	break;

	// originally: if (charToPrint == 130 or charToPrint == 0x7f)
	case '\202':
	case '\177':
	{
		// left arrow or delete
		if (charCount > 0)
		{
			backspace_no_correct();
			charCount--;
		}
	}
	break;

	case 131:
	{ // micro-backspace
		// DOES NOT UPDATE CHARCOUNT!
		// THIS WILL CAUSE PROBLEMS WITH RETURN!
		// TODO: FIX THIS ISSUE
		micro_backspace(1);
	}
	break;

	case 132:
	{ // micro-forwardspace
		// DOES NOT UPDATE CHARCOUNT!
		// THIS WILL CAUSE PROBLEMS WITH RETURN!
		// TODO: FIX THIS ISSUE
		forwardSpaces(1);
	}
	break;

	default:
	{
		// Serial.println("about to print");
		send_letter(asciiTrans[charToPrint]);
		charCount++;
	}
	break;
	}

	// Serial.println("ok"); // sends back our characters (one) printed
	// Serial.flush();
	// delay(10);

	return charCount;
}

int printAllChars(char buffer[],
				  uint16_t bytesToPrint,
				  int charCount)
{
	uint8_t readLength = 65;
	bool fastPrinting = false;
	uint16_t bytesPrinted = 0;
	uint8_t bufferPos = 0;
	uint8_t bytesRead = 0;

	while (bytesToPrint > 0)
	{
		// read bytes from serial
		// Serial.print("about to print ");
		// Serial.print(bytesToPrint);
		// Serial.println(" bytes");
		bytesPrinted = 0;
		// wait for more bytes, but only wait up to 5 seconds
		unsigned long startTime = millis();
		bool timeout = false;
		while (not Serial.available() and not timeout)
		{
			if (millis() - startTime > 5000)
			{
				timeout = true;
			}
			// Bean.sleep(10);
			delay(10);
		}
		if (timeout)
		{
			if (fastPrinting)
			{
				fastTextFinish();
				fastPrinting = false;
				send_crlf(charCount);
				charCount = 0;
			}
			break;
		}
		bytesRead = Serial.readBytes(buffer, readLength - 1);
		Serial.println(bytesRead);
		bufferPos = 0;
		while (bufferPos < bytesRead)
		{
			// print all the bytes
			// check for underline and bold
			if (buffer[bufferPos] == 2)
			{ // bold
				bold = !bold;
			}
			else if (buffer[bufferPos] == 3)
			{ // underline
				underline = !underline;
			}
			else if (buffer[bufferPos] == 5)
			{ // reverse instruction
				// we might need to read in more of the command, which
				// is four bytes long
				int bytesToRead = 4 - (bytesRead - bufferPos);
				while (bytesToRead > 0)
				{
					// read necessary bytes
					int extraBytesRead = Serial.readBytes(buffer + bytesRead, bytesToRead);
					bytesRead += extraBytesRead;
					bytesToRead -= extraBytesRead;
				}
				// now we have enough bytes
				int print_dir = buffer[bufferPos + 1];
				int numSpaces = buffer[bufferPos + 2];
				int spacesDir = buffer[bufferPos + 3];
				bufferPos += 4;
				//
			}
			else if (buffer[bufferPos] != '\r' and buffer[bufferPos] != '\n')
			{
				// begin fast printing
				if (!fastPrinting)
				{
					fastTextInit();
					fastPrinting = true;
				}
				fastTextChars(buffer + bufferPos, 1);
				if (reverseText)
				{
					charCount--;
					if (charCount < 0)
					{
						charCount = 0;
					}
				}
				else
				{
					charCount++;
				}
			}
			else
			{
				if (fastPrinting)
				{
					fastTextFinish();
					fastPrinting = false;
				}
				send_crlf(charCount);
				charCount = 0;
			}
			bufferPos++;
			bytesToPrint--;
			bytesPrinted++;
		}
	}
	if (fastPrinting)
	{
		fastTextFinish();
	}
	Serial.println();
	return charCount;
}

void printImage(char *buffer)
{
	// will read 3-tuples of bytes
	// first byte is low-order byte for run length,
	// second byte is high-order byte for run length,
	// third byte is the byte to print (typically, period, space, or return)
	uint32_t rowBitsPrinted = 0;
	int bitsRead = 0;

	while (1)
	{ // keep reading bytes until we get (0,0,0)
		// read bytes from serial
		// wait for more bytes, but only wait up to 2 seconds
		unsigned long startTime = millis();
		bool timeout = false;
		bool first = true;
		while (Serial.available() == 0 and not timeout)
		{
			if (first)
			{
				Serial.println("Please send.");
				first = false;
			}
			if (millis() - startTime > 2000)
			{
				timeout = true;
			}
			delay(50);
			// Bean.sleep(10);
		}
		if (timeout)
		{
			paper_vert(1, 1);
			Serial.println("timeout");
			micro_backspace(rowBitsPrinted * 3);
			break;
		}
		bitsRead = Serial.readBytes(buffer, 3);
		// Serial.println(bitsRead); // should always be 3
		if (buffer[0] == 0 and buffer[1] == 0 and buffer[2] == 0)
		{
			Serial.println("end received");
			break; // no more bits to print
		}
		// we do have a run to print
		uint16_t runLength = buffer[0] + (buffer[1] << 8);
		printRun(runLength, buffer[2]);
	}
	Serial.println("done");
}

void print_str(char *s)
{
	while (*s != '\0')
	{
		send_letter(asciiTrans[*s++]);
	}
}

void print_strln(char *s)
{
	// prints a line and then a carriage return
	print_str(s);
	send_crlf(strlen(s));
}

inline void sendByteOnPin(int command)
{
	// This will actually send 10 bytes,
	// starting with a zero (always)
	// and then the next nine bytes

	// when the pin is high (on), the bus is pulled low
	noInterrupts(); // turn off interrupts so we don't get disturbed
	// unrolled for consistency, will send nine bits
	// with a zero initial bit
	// pull low for 5.75us (by turning on the pin)
	busWrite0();
	pulseDelay(); // fudge factor for 5.75

	// this code could definitely be cleaned up but this would alter the timing so leaving as-is for now

	// send low order bit (LSB endian)
	int nextBit = (command & 0b000000001);
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay(); // special shortened...
	}

	// next bit (000000010)
	nextBit = (command & 0b000000010) >> 1;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// next bit (000000100)
	nextBit = (command & 0b000000100) >> 2;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// next bit (000001000)
	nextBit = (command & 0b000001000) >> 3;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// next bit (000010000)
	nextBit = (command & 0b000010000) >> 4;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// next bit (000100000)
	nextBit = (command & 0b000100000) >> 5;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// next bit (001000000)
	nextBit = (command & 0b001000000) >> 6;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// next to last bit (010000000)
	nextBit = (command & 0b010000000) >> 7;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay();
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// final bit (100000000)
	nextBit = (command & 0b100000000) >> 8;
	if (nextBit == 0)
	{
		// turn on
		busWrite0();
		pulseDelay(); // last one is special :)
	}
	else if (nextBit == 1)
	{
		// turn off
		busWrite1();
		pulseDelay();
	}

	// make sure we aren't still pulling down
	busWrite1();

	// re-enable interrupts
	interrupts();
}

void send_letter(int letter)
{
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0b000000011);
	sendByte(letter);
	if (underline)
	{
		sendByte(0b000000000); // no space
		sendByte(0x121);
		sendByte(0x00b);
		sendByte(0x121);
		sendByte(0b000000011);
		sendByte(asciiTrans['_']);
	}
	if (bold)
	{
		sendByte(0b000000001); // one microspace
		sendByte(0x121);
		sendByte(0x00b);
		sendByte(0x121);
		sendByte(0b000000011);
		sendByte(letter);
		sendByte(0b000001001);
	}
	else
	{
		// not bold
		sendByte(0b000001010); // spacing for one character
	}
	// delay(LETTER_DELAY); // before next character
}

#define NOSPACE_DELAY 50
void letterNoSpace(int letter)
{
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0b000000011);
	sendByte(letter);
	sendByte(0);		  // don't send any spaces
	delay(NOSPACE_DELAY); // before next character
}

#define MICROSPACE_DELAY 65

void letterMicrospace(int letter)
{
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0b000000011);
	sendByte(letter);
	sendByte(2);			 // send one microspace
	delay(MICROSPACE_DELAY); // before next character
}

#define VERT_DELAY 45 // delay for paper_vert
// 0 is up, 1 is down
void paper_vert(int direction)
{
	// 0 == up
	// 1 == down
	// 4 == micro-down
	// 21 == micro-up
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0b000000101);
	if (direction == 0)
	{ // cursor-up (paper-down)
		sendByte(0b000001000);
	}
	else if (direction == 1)
	{
		sendByte(0b010001000); // cursor-down (paper-up)
	}
	else if (direction == 4)
	{
		sendByte(0b010000010); // cursor-micro-down (paper-micro-up)
	}
	else if (direction == 21)
	{
		sendByte(0b000000010); // cursor-micro-up (paper-micro-down)
	}
	sendByte(0x121);
	sendByte(0x00b);

	delay(VERT_DELAY); // give it a bit more time
}

// 0 is up, 1 is down
void paper_vert(int direction, int microspaces)
{
	// 0 == up
	// 1 == down
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0b000000101);
	sendByte((direction << 7) | (microspaces << 1));

	sendByte(0x121);
	sendByte(0x00b);
	// delay(LETTER_DELAY + LETTER_DELAY / 10 * microspaces/5);
}

void backspace_no_correct()
{
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0b000000100);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b000000000);
	sendByte(0b000001010);
	/*sendByte(0x121);


	// send one more byte but don't wait explicitly for the response
	// of 0b000000100
	sendByteOnPin(0x00b);*/
	// delay(LETTER_DELAY / 10); // a bit more time
}

void send_crlf(int numChars)
{
	// calculations for further down
	int byte1 = (numChars * 5) >> 7;
	int byte2 = ((numChars * 5) & 0x7f) << 1;

	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0x007);
	sendByte(0x121);

	/*if (numChars <= 23 || numChars >= 26) {*/
	sendByte(0x006);

	// We will send two bytes from a 10-bit number
	// which is numChars * 5. The top three bits
	// of the 10-bit number comprise the first byte,
	// and the remaining 7 bits comprise the second
	// byte, although the byte needs to be shifted
	// left by one (not sure why)
	// the numbers are calculated above for timing reasons
	sendByte(byte1);
	sendByte(byte2); // each char is worth 10
	sendByte(0x121);
	// right now, the platten is moving, maybe?

	/*} else if (numChars <= 25) {
			// not sure why this is so different
			sendByte(0x00d);
			sendByte(0x007);
			sendByte(0x121);
			sendByte(0x006);
			sendByte(0b000000000);
			sendByte(numChars * 10);
			sendByte(0x121);
			// right now, the platten is moving, maybe?
	}*/

	sendByte(0b000000101);
	sendByte(0b010010000);

	/*
	sendByte(0x121);

	// send one more byte but don't wait explicitly for the response
	// of 0b001010000
	sendByteOnPin(0x00b);*/

	// wait for carriage
	delay(CARRIAGE_WAIT_BASE + CARRIAGE_WAIT_MULTIPLIER * numChars);
}

void send_cr(int numChars)
{
	// calculations for further down
	int byte1 = (numChars * 5) >> 7;
	int byte2 = ((numChars * 5) & 0x7f) << 1;

	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0x007);
	sendByte(0x121);

	/*if (numChars <= 23 || numChars >= 26) {*/
	sendByte(0x006);

	// We will send two bytes from a 10-bit number
	// which is numChars * 5. The top three bits
	// of the 10-bit number comprise the first byte,
	// and the remaining 7 bits comprise the second
	// byte, although the byte needs to be shifted
	// left by one (not sure why)
	// the numbers are calculated above for timing reasons
	sendByte(byte1);
	sendByte(byte2); // each char is worth 10
	sendByte(0x121);
	delay(CARRIAGE_WAIT_BASE + CARRIAGE_WAIT_MULTIPLIER * numChars);
}

#define TAB_DELAY 200
void send_tab()
{
	forwardSpaces(17);
	delay(TAB_DELAY);
}

void correct_letter(int letter)
{
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0b000000101);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b000000000);
	sendByte(0b000001010);
	sendByte(0x121);
	sendByte(0b000000100);
	sendByte(letter);
	sendByte(0b000001010);
	sendByte(0x121);
	sendByte(0b000001100);
	sendByte(0b010010000);
}

void micro_backspace(int microspaces)
{
	// 10 microspaces is one space
	sendByte(0x121);
	sendByte(0b000001110);
	sendByte(0b011010000);
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0b000000100);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b000000000);
	// sendByte(microspaces * 3);
	sendByte(microspaces);
	delay(CARRIAGE_WAIT_BASE + CARRIAGE_WAIT_MULTIPLIER * microspaces / 5);
	// sendByte(0b000000010);
	/*
	sendByte(0x121);

	// send one more byte but don't wait explicitly for the response
	// of 0b000000100
	sendByteOnPin(0x00b);*/
}

void forwardSpaces(int num_microspaces)
{
	// five microspaces is one real space
	sendByte(0x121);
	sendByte(0b000001110);
	sendByte(0b010010110);
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0x006);
	sendByte(0x121);
	sendByte(0x006);

	sendByte(0b010000000);
	// sendByte(0b001111100);
	sendByte(num_microspaces * 3);
	// delay(LETTER_DELAY + LETTER_DELAY / 10 * num_microspaces / 5);
}

void spin()
{
	sendByte(0x121);
	sendByte(0x007);
}

void unknownCommand()
{
	sendByte(0x121);
	sendByte(0b000001001);
	sendByte(0b010000000);
}

void sendByte(int b)
{
	// Serial.println("sending byte!");
	sendByteOnPin(b);
	while (busRead())
	{
		// busy
	}
	while (!busRead())
	{
		// busy
	}
	delayMicroseconds(LETTER_DELAY); // wait a bit before sending next char
}
void fastTextInit()
{
	sendByte(0x121);
	sendByte(0x00b);
	sendByte(0x121);
	sendByte(0b000001001);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0b000001010);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0x006);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b010000000);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0b000000101);
	sendByte(0b010000000);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0b000010010);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b010000000);
	sendByte(0b000000000);
}

void fastTextChars(char *s, int length)
{
	// letters start here
	for (int i = 0; i < length; i++)
	{
		sendByte(0x121);
		sendByte(0b000000011);

		sendByte(asciiTrans[*s++]);

		if (underline)
		{
			sendByte(0b000000000); // no space
			sendByte(0x121);
			sendByte(0x00b);
			sendByte(0x121);
			sendByte(0b000000011);
			sendByte(asciiTrans['_']);
		}
		if (bold)
		{
			sendByte(0b000000001); // one microspace
			sendByte(0x121);
			sendByte(0x00b);
			sendByte(0x121);
			sendByte(0b000000011);
			sendByte(asciiTrans[*(s - 1)]);
			sendByte(0b000001001);
		}
		else
		{
			if (reverseText)
			{
				sendByte(0);			 // no forward space
				micro_backspace(0b1010); // full space back
			}
			else
			{
				// full space
				sendByte(0b000001010);
			}
		}
	}
}

void fastTextCharsMicro(char s, uint16_t length)
{
	// letters start here
	if (s == ' ')
	{
		// just make a fast run of spaces
		sendByte(0x121);
		sendByte(0b000000011);
		sendByte(0b000000000);
		sendByte(0b11 * length);
	}
	else
	{
		for (uint16_t i = 0; i < length; i++)
		{
			sendByte(0x121);
			sendByte(0b000000011);

			sendByte(asciiTrans[s]);
			sendByte(0b000000011); // 1 microspace
		}
	}
	// delay(LETTER_DELAY + LETTER_DELAY / 10 * length / 5);
}

void fastTextFinish()
{
	sendByte(0x121);
	sendByte(0b000001001);
	sendByte(0b000000000);

	// delay(LETTER_DELAY * 2); // a bit more time
}

void resetTypewriter()
{
	sendByte(0x121);
	sendByte(0b000000000);
	// sendByte(0b000100110); // receives this...
	sendByte(0x121);
	sendByte(0b000000001);
	sendByte(0b000100000);

	sendByte(0x121);
	sendByte(0b000001001);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0b000010010);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b010000000);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0x00d);
	sendByte(0b000001001);
	sendByte(0x121);
	sendByte(0x006);
	sendByte(0b010000000);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0b000001010);
	sendByte(0b000000000);
	sendByte(0x121);
	sendByte(0x007);
	sendByte(0x121);
	sendByte(0b000001100);
	sendByte(0b000000001);
}

void beepTypewriter()
{
	spin();
	// not currently working
	/*
	sendByte(0x121);
	sendByte(0x00b);		// send one more byte but don't wait explicitly for the response
	*/
}