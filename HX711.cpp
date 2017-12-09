#include <Arduino.h>
#include <HX711.h>

#if ARDUINO_VERSION <= 106
    // "yield" is not implemented as noop in older Arduino Core releases, so let's define it.
    // See also: https://stackoverflow.com/questions/34497758/what-is-the-secret-of-the-arduino-yieldfunction/34498165#34498165
    void yield(void) {};
#endif

HX711::HX711(byte *dout, byte count, byte pd_sck, byte gain) {
	begin(dout, count, pd_sck, gain);
}

HX711::HX711() {
}

HX711::~HX711() {
}

void HX711::begin(byte *dout, byte count, byte pd_sck, byte gain) {
	PD_SCK = pd_sck;
	ic_count = count;
	for (byte i=0; i<count; i++){
		DOUT[i] = dout[i];
		pinMode(DOUT[i], INPUT);
	}

	pinMode(PD_SCK, OUTPUT);
	pinMode(DOUT, INPUT);

	set_gain(gain);
}

bool HX711::is_ready() {
	bool ready;
	ready = true;
	for (byte i=0; i<ic_count; i++){
		ready &= (digitalRead(DOUT[i]) == LOW);
	}
	return ready;
}

void HX711::set_gain(byte gain) {
	switch (gain) {
		case 128:		// channel A, gain factor 128
			GAIN = 1;
			break;
		case 64:		// channel A, gain factor 64
			GAIN = 3;
			break;
		case 32:		// channel B, gain factor 32
			GAIN = 2;
			break;
	}

	digitalWrite(PD_SCK, LOW);
	read(0);
}

void HX711::parallelShiftIn(unsigned long *dest){
	byte value;
	//memset(dest, 0, sizeof(*dest)*ic_count);
	for (byte ic_no=0; ic_no<ic_count; ic_no++)
		dest[ic_no]=0;
	// 643686	742613	986572	1397212
	//
	for (byte bit_no=0; bit_no<24; bit_no++){
		digitalWrite(PD_SCK, HIGH);
		digitalWrite(PD_SCK, LOW);
		for (byte ic_no=0; ic_no<ic_count; ic_no++){
			dest[ic_no] <<= 1;
			value = digitalRead(DOUT[ic_no]);
			if ( (bit_no==0) && (value==1) ){
				// Fill all bit_nos with one, when MSB is set
				dest[ic_no] = -1;
			}
			dest[ic_no] |= value;
		}
	}
}

long *HX711::readAll(){
	while (!is_ready()) {
		// Will do nothing on Arduino but prevent resets of ESP8266 (Watchdog Issue)
		yield();
	}
	parallelShiftIn(last_values);

	// set the channel and the gain factor for the next reading using the clock pin
	for (unsigned int i = 0; i < GAIN; i++) {
		digitalWrite(PD_SCK, HIGH);
		digitalWrite(PD_SCK, LOW);
	}
	return(last_values);
}

long HX711::read(byte channel) {
	// wait for the chip to become ready
	while (!is_ready()) {
		// Will do nothing on Arduino but prevent resets of ESP8266 (Watchdog Issue)
		yield();
	}

	unsigned long value = 0;
	uint8_t data[3] = { 0 };
	uint8_t filler = 0x00;

	// pulse the clock pin 24 times to read the data
	data[2] = shiftIn(DOUT, PD_SCK, MSBFIRST);
	data[1] = shiftIn(DOUT, PD_SCK, MSBFIRST);
	data[0] = shiftIn(DOUT, PD_SCK, MSBFIRST);

	// set the channel and the gain factor for the next reading using the clock pin
	for (unsigned int i = 0; i < GAIN; i++) {
		digitalWrite(PD_SCK, HIGH);
		digitalWrite(PD_SCK, LOW);
	}

	// Replicate the most significant bit to pad out a 32-bit signed integer
	if (data[2] & 0x80) {
		filler = 0xFF;
	} else {
		filler = 0x00;
	}

	// Construct a 32-bit signed integer
	value = ( static_cast<unsigned long>(filler) << 24
			| static_cast<unsigned long>(data[2]) << 16
			| static_cast<unsigned long>(data[1]) << 8
			| static_cast<unsigned long>(data[0]) );

	return static_cast<long>(value);
}

long *HX711::read_averages(byte times) {
	long sum[8];
	//memset(sum, 0, sizeof(sum));
	for (byte i = 0; i< ic_count; i++)
		sum[i]=0;

	for (byte i = 0; i < times; i++) {
		readAll();
		for (byte x = 0; x < ic_count; x++)
		{
			sum[x] += last_values[x];
		}
		yield();
	}
	for (byte x = 0; x < ic_count; x++)
		last_values[x] = sum[x] / times;
	return last_values;
}

long HX711::read_average(byte times, byte channel) {
	long sum = 0;
	for (byte i = 0; i < times; i++) {
		sum += read(channel);
		yield();
	}
	return sum / times;
}

double HX711::get_value(byte times, byte channel) {
	return read_average(times, channel) - OFFSET[channel];
}

float HX711::get_units(byte times, byte channel) {
	return get_value(times, channel) / SCALE;
}

void HX711::tare(byte times) {
	read_averages(times);
	for (byte i=0; i<ic_count; i++)
		OFFSET[i] = last_values[i];
	//set_offset(sum);
}

void HX711::set_scale(float scale) {
	SCALE = scale;
}

float HX711::get_scale() {
	return SCALE;
}

void HX711::set_offset(long offset) {
	// TODO: STUB!!!
	//OFFSET = offset;
}

long HX711::get_offset() {
	return OFFSET;
}

void HX711::power_down() {
	digitalWrite(PD_SCK, LOW);
	digitalWrite(PD_SCK, HIGH);
}

void HX711::power_up() {
	digitalWrite(PD_SCK, LOW);
}
