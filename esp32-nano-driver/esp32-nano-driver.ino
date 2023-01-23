

#define RXD2 16
#define TXD2 17

#define LED 2

void setup()
{
    Serial.begin(115200);
    Serial2.begin(300, SERIAL_8N1, RXD2, TXD2);
    pinMode(LED, OUTPUT);
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

Queue<char> SerialQueue(65536);

void loop()
{
    while(Serial.available())
    {
        SerialQueue.Add(Serial.read());
    }

    if(SerialQueue.Available())
    {
        digitalWrite(LED, HIGH);
        Serial2.print(SerialQueue.Next());
    }
    else
    {
        digitalWrite(LED, LOW);
    }
}

