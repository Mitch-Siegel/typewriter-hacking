#define CLOCK_PIN 4

volatile uint64_t clocksSeen = 0;
volatile bool needPrinty;

void IRAM_ATTR clock_isr()
{
    clocksSeen++;
}

/*
static bool last = 0;
    bool this = (REG_READ(GPIO_IN_REG) >> 3) & 0b1;
    if (this == 0 && last == 1)
    {
        clocksSeen++;
    }*/

void setup()
{
    setCpuFrequencyMhz(240);
    Serial.begin(115200);
    delay(500);

    Serial.print("Initialized CPU at ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println("MHz");
    pinMode(CLOCK_PIN, INPUT);
    // attachInterrupt(CLOCK_PIN, clock_isr, RISING);
}

void loop()
{
    static int lastSecond = 0;
    static int lastDivided = 0;
    int thisSecond;


    static bool lastRead = 0;
    bool thisRead = (REG_READ(GPIO_IN_REG) >> 3) & 0b1;
    if (thisRead == 0 && lastRead == 1)
    {
        clocksSeen++;
    }
    lastRead = thisRead;
    
    if ((thisSecond = (millis() / 1000)) > lastSecond)
    {
        noInterrupts();
        lastSecond = thisSecond;
        Serial.print(thisSecond);
        Serial.print(": ");
        Serial.println(clocksSeen);
        interrupts();
    }
    /*
    if (needPrinty)
    {
        noInterrupts();
        int thisDivided = clocksSeen / 12000000;
        if (thisDivided > lastDivided)
        {
            Serial.println("Some amount of time has passed");
            lastDivided = thisDivided;
        }
        interrupts();
    }
    else if ((thisSecond = (millis() / 1000)) > lastSecond)
    {
        noInterrupts();
        lastSecond = thisSecond;
        Serial.print(thisSecond);
        Serial.print(": ");
        Serial.println(clocksSeen);
        interrupts();
    }
    */
}