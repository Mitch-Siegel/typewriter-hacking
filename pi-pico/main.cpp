#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "clkdiv.pio.h"
#include "hardware/regs/intctrl.h"
/*
int main() {

    const uint led_pin = 25;

    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    // Initialize chosen serial port
    stdio_init_all();

    // Loop forever
    while (true) {

        // Blink LED
        printf("Blinking!\r\n");
        gpio_put(led_pin, true);
        sleep_ms(1000);
        gpio_put(led_pin, false);
        sleep_ms(1000);
    }
}*/

const uint8_t asciiTrans[128] =
    // col: 0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f     row:
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0

     //
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1

     //    sp     !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /
     0x00, 0x49, 0x4b, 0x38, 0x37, 0x39, 0x3f, 0x4c, 0x23, 0x16, 0x36, 0x3b, 0xc, 0x0e, 0x57, 0x28, // 2

     //     0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
     0x30, 0x2e, 0x2f, 0x2c, 0x32, 0x31, 0x33, 0x35, 0x34, 0x2a, 0x4e, 0x50, 0x00, 0x4d, 0x00, 0x4a, // 3

     //     @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
     0x3d, 0x20, 0x12, 0x1b, 0x1d, 0x1e, 0x11, 0x0f, 0x14, 0x1F, 0x21, 0x2b, 0x18, 0x24, 0x1a, 0x22, // 4

     //     P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
     0x15, 0x3e, 0x17, 0x19, 0x1c, 0x10, 0x0d, 0x29, 0x2d, 0x26, 0x13, 0x41, 0x00, 0x40, 0x00, 0x4f, // 5

     //     `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o
     0x00, 0x01, 0x59, 0x05, 0x07, 0x60, 0x0a, 0x5a, 0x08, 0x5d, 0x56, 0x0b, 0x09, 0x04, 0x02, 0x5f, // 6

     //     p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~    DEL
     0x5c, 0x52, 0x03, 0x06, 0x5e, 0x5b, 0x53, 0x55, 0x51, 0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00}; // 7

#define PIN_SEND 16
#define PIN_LISTEN 17

#define LETTER_DELAY 450
#define CARRIAGE_WAIT_BASE 200
// #define CARRIAGE_WAIT_MULTIPLIER 15
#define CARRIAGE_WAIT_MULTIPLIER 10

#define VERT_DELAY 45 // delay for paper_vert
#define MICROSPACE_DELAY 23
#define NOSPACE_DELAY 50

#define busRead() gpio_get(PIN_LISTEN)

#define LED_PIN 25

volatile uint64_t interruptsCalled = 0;

volatile bool flashLED = true;
volatile bool LEDState = false;

enum SerialMode
{
    mode_idle,
    mode_xmit,
    mode_rcv
};
volatile enum SerialMode serialMode = mode_idle;
// bool flashLED = true;

volatile uint16_t bitbangSerialBuf = 0;
volatile uint8_t bitsToBang = 0;

volatile int nIdles = 0;
volatile int nXmits = 0;
volatile int nRcvs = 0;

template <class T>
class Queue
{
private:
    volatile T *buf;
    volatile size_t size;
    volatile uint16_t nextP; // index of next output value to be gotten
    volatile uint16_t endP;  // index where next input value will be placed

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
        this->endP %= size;
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
            return (((size * 2) - this->nextP) - this->endP);
        }
    }

    // get the next char from the queue (should always be wrapped in an available() check first as this will permit overruns)
    T Next()
    {
        T toReturn = this->buf[this->nextP++];
        this->nextP %= size;
        return toReturn;
    }

    void Clear()
    {
        this->nextP = this->endP;
    }
};

Queue<uint32_t> busReadQueue = Queue<uint32_t>(128);
Queue<char> printQueue = Queue<char>(1024);

volatile  uint32_t totalRXs = 0;
uint rxSM, txSM;

void rxIRQHandler()
{
    totalRXs++;
    LEDState = !LEDState;
    busReadQueue.Add(pio_sm_get(pio0, rxSM));
    pio_interrupt_clear(pio0, 1);
}

void sendByte(int b)
{
    pio_sm_put(pio0, txSM, b);
    sleep_us(LETTER_DELAY); // wait a bit before sending next char
}

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

    sleep_ms(VERT_DELAY); // give it a bit more time
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
    // sleep_ms(LETTER_DELAY + LETTER_DELAY / 10 * microspaces/5);
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
    // sleep_ms(LETTER_DELAY / 10); // a bit more time
}

void send_return(int numChars)
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
    sleep_ms(CARRIAGE_WAIT_BASE + CARRIAGE_WAIT_MULTIPLIER * numChars);
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
    // sendByte(microspace#define busRead() PIND & (1 << (PIN_READ))
    sendByte(0x121);
    sendByte(0x00d);
    sendByte(0x006);
    sendByte(0x121);
    sendByte(0x006);

    sendByte(0b010000000);
    // sendByte(0b001111100);
    sendByte(microspaces * 3);
    // sleep_ms(LETTER_DELAY + LETTER_DELAY / 10 * num_microspaces / 5);
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

bool bold = false;
bool underline = false;

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

int printOne(uint8_t charToPrint, int charCount)
{
    // print the character, and return the number of chars on the line that are left.
    // E.g., if we start with 6 and we print a character, then we return 7.
    // But, if we print a return, then that clears the charCount to 0, and if we
    // go left by one, that subtracts it, etc.

    /*
    if (charToPrint < 0) {
        charToPrint += 256; // correct for signed char
    }*/

    if (charToPrint == '\r' or charToPrint == '\n')
    {
        send_return(charCount);
        charCount = 0;
    }
    else if (charToPrint == '\200' or charToPrint == '\201')
    {
        paper_vert((charToPrint == '\200' ? 0 : 1), 8); // full line up/down
    }
    else if (charToPrint == '\004')
    { // micro-down, ctrl-d is 4
        paper_vert(1, 1);
    }
    else if (charToPrint == '\025')
    { // micro-up, ctrl-u is 21
        paper_vert(0, 1);
    }

    // originally: if (charToPrint == 130 or charToPrint == 0x7f)
    else if (charToPrint == '\202' or charToPrint == '\177')
    {
        // left arrow or delete
        if (charCount > 0)
        {
            backspace_no_correct();
            charCount--;
        }
    }
    else if (charToPrint == 131)
    { // micro-backspace
        // DOES NOT UPDATE CHARCOUNT!
        // THIS WILL CAUSE PROBLEMS WITH RETURN!
        // TODO: FIX THIS ISSUE
        micro_backspace(1);
    }
    else if (charToPrint == 132)
    { // micro-forwardspace
        // DOES NOT UPDATE CHARCOUNT!
        // THIS WILL CAUSE PROBLEMS WITH RETURN!
        // TODO: FIX THIS ISSUE
        forwardSpaces(1);
    }
    else
    {
        // Serial.println("about to print");
        send_letter(asciiTrans[charToPrint]);
        charCount++;
    }

    // Serial.println("ok"); // sends back our characters (one) printed
    // Serial.flush();
    // sleep_ms(10);

    return charCount;
}

int main()
{
    set_sys_clock_khz(133000, true);
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_set_dir(PIN_LISTEN, GPIO_IN);
    gpio_set_dir(PIN_SEND, GPIO_OUT);

    gpio_put(LED_PIN, true);
    stdio_init_all();

    // Choose PIO instance (0 or 1)
    PIO pio = pio0;

    // set up the state machine to watch the GPIO pin attached to the typewriter clock and set the IRQ flag on a clock edge
    {
        // Get first free state machine in PIO 0
        uint sm = pio_claim_unused_sm(pio, true);

        // Add PIO program to PIO instruction memory. SDK will find location and
        // return with the memory offset of the program.
        uint offset = pio_add_program(pio, &edgedetector_program);

        // Initialize the program using the helper function in our .pio file
        // use pin 0
        edgedetector_program_init(pio, sm, offset, 0, 1);

        // Start running our PIO program in the state machine
        pio_sm_set_enabled(pio, sm, true);
    }

    // set up the state machine which waits for a certain number of clock edges to divide the clock before setting its own IRQ flag
    {
        // Get first free state machine in PIO 0
        uint sm = pio_claim_unused_sm(pio, true);

        // Add PIO program to PIO instruction memory. SDK will find location and
        // return with the memory offset of the program.
        uint offset = pio_add_program(pio, &clkdivider_program);

        // Initialize the program using the helper function in our .pio file
        clkdivider_program_init(pio, sm, offset, 1);

        // Start running our PIO program in the state machine
        pio_sm_set_enabled(pio, sm, true);
    }

    // set up the state machine which trips an interrupt when a word is received on the bus
    {
        // Get first free state machine in PIO 0
        rxSM = pio_claim_unused_sm(pio, true);

        // Add PIO program to PIO instruction memory. SDK will find location and
        // return with the memory offset of the program.
        uint offset = pio_add_program(pio, &busreader_program);

        // Initialize the program using the helper function in our .pio file
        busreader_program_init(pio, rxSM, offset, 1, PIN_LISTEN);

        // Start running our PIO program in the state machine
        pio_sm_set_enabled(pio, rxSM, true);
    }

    /*
    // set up the state machine which trips an interrupt when a word is received on the bus
    {
        // Get first free state machine in PIO 0
        txSM = pio_claim_unused_sm(pio, true);

        // Add PIO program to PIO instruction memory. SDK will find location and
        // return with the memory offset of the program.
        uint offset = pio_add_program(pio, &busreader_program);

        // Initialize the program using the helper function in our .pio file
        buswriter_program_init(pio, txSM, offset, 1, PIN_SEND);

        // Start running our PIO program in the state machine
        pio_sm_set_enabled(pio, txSM, true);
    }
    */

    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);
    irq_set_enabled(PIO0_IRQ_1, true);
    irq_set_exclusive_handler(PIO0_IRQ_1, rxIRQHandler);
    // irq_add_shared_handler(PIO0_IRQ_1, baudIRQHandler, 255);

    // Do nothing
    while (true)
    {
        /*
        static absolute_time_t last_time = 0;

        absolute_time_t this_time = get_absolute_time();
        int64_t diff_us = absolute_time_diff_us(last_time, this_time);
        // run every time the diff is greater than 1M (num of micros in 1 second)
        if (diff_us >= 1000000)
        {

            printf("%llu interrupts for clock in %lfs (%fus)\n", interruptsCalled, static_cast<double>(diff_us) / 1000000.0, (static_cast<double>(diff_us) / interruptsCalled));
            interruptsCalled = 0;

            last_time = this_time;
        }
        /*
        static bool state = true;
        if(flashLED)
        {
            state = !state;
        }
        else
        {
            state = true;
        }*/
        static int checksum = 0;
        static int rcvdThisTime = 0;
        static bool printedEmpty = false;
        if (busReadQueue.Available())
        {
            while (busReadQueue.Available())
            {
                rcvdThisTime++;
                uint32_t thisRead = busReadQueue.Next();
                checksum += thisRead;
                printf("%u\n", thisRead);
                sleep_ms(2);
            }
            printedEmpty = false;
        }
        else
        {
            if (!printedEmpty)
            {
                printf("Queue is empty: %d total words received - checksum %d - %d rcv'd\n\n", totalRXs, checksum, rcvdThisTime);
                checksum = 0;
                rcvdThisTime = 0;
                printedEmpty = true;
            }
        }

        while(uart_is_readable(uart0))
        {
            printQueue.Add(uart_getc(uart0));
        }

        while(printQueue.Available())
        {
            uart_putc(uart0, printQueue.Next());
        }

        static bool LED = true;
        LED = !LED;
        gpio_put(LED_PIN, LED);
        sleep_ms(100);

        /*if(busRead() == 1)
        {
            bitsToBang = 10;
            serialMode = mode_rcv;
        }
        else{
            printf("idles: %d, xmits: %d, rcvs: %d\n", nIdles, nXmits, nRcvs);
        }*/
        // printf("%u\n", mostRecentRX);
        // sleep_ms(1000);

        // printOne('a', 0);
        // sleep_ms(1000);

        // sleep_ms(100);
        // printf("%d interrupts for clock\n", interruptsCalled);
    }
}