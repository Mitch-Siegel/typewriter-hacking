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

#define LED_PIN 25

uint64_t interruptsCalled = 0;

volatile bool flashLED = true;
volatile bool LEDState = false;
// bool flashLED = true;

void baudIRQHandler()
{
    interruptsCalled++;
    static uint32_t count = 0;
    if (++count >= 180000)
    {
        LEDState = !LEDState;
        // flashLED = !flashLED;
        count = 0;
    }
    pio_interrupt_clear(pio0, 1);

}

int main()
{
    set_sys_clock_khz(133000, true);
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(DIVIDED_CLOCK_OUTPUT_PIN, GPIO_OUT);
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
    pio_set_irq1_source_enabled(pio, pis_interrupt1, true);
    irq_set_enabled(PIO0_IRQ_1, true);
    irq_set_exclusive_handler(PIO0_IRQ_1, baudIRQHandler);
    // irq_add_shared_handler(PIO0_IRQ_1, baudIRQHandler, 255);
    

    // Do nothing
    while (true)
    {
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
        gpio_put(LED_PIN, LEDState);
        // sleep_ms(100);
        // printf("%d interrupts for clock\n", interruptsCalled);
    }
}