///////////////////////////////////////////////////////////////////////////////
// https://github.com/furdog/async/blob/main/async.h
#ifndef ASYNC_GUARD
typedef void * async;
#define ASYNC_CAT1(a, b) a##b
#define ASYNC_CAT(a, b) ASYNC_CAT1(a, b)
#define ASYNC_DISPATCH(state) void **_state = &state; \
			 if (*_state) { goto **_state; }
#define ASYNC_YIELD(act) do { *_state = &&ASYNC_CAT(_l, __LINE__); \
			      act; ASYNC_CAT(_l, __LINE__) :; } while (0)
#define ASYNC_AWAIT(cond, act) \
			 do { ASYNC_YIELD(); if (!(cond)) { act; } } while (0)
#define ASYNC_RESET(act) do { *_state = NULL; act; } while (0)
#define ASYNC_GUARD
#endif

///////////////////////////////////////////////////////////////////////////////
#include <Arduino.h>

static clock_t timestamp_prev = 0;
static clock_t timestamp      = 0;

clock_t get_delta_time_ms()
{
	clock_t delta;
	
	timestamp = millis();
	delta = timestamp - timestamp_prev;
	timestamp_prev = timestamp;
	
	return delta;
}

///////////////////////////////////////////////////////////////////////////////
#include <SPI.h>
#include "rx5808.h"

#define RX5808_PIN_SPI_CS 5
#define RX5808_PIN_RSSI 34

struct rx5808_freq_table table;
struct rx5808 rx;

void scan_channels(clock_t delta)
{
	static async state;
	static clock_t timer;
	static int i;
	
	const float alpha = 0.05;
	static float avg;

	timer += delta;

	ASYNC_DISPATCH(state);
	
	for (i = 0; i < RX5808_FREQ_TABLE_LEN; i++) {
		rx5808_set_freq_mhz(&rx,
			   rx5808_freq_table_get_channel_frequency(&table, i));
		
		timer = 0;
		ASYNC_AWAIT(timer >= 25, return);
		
		//set initial value
		avg = rx5808_get_rssi(&rx);
		
		while (timer < 50) {
			avg = alpha * rx5808_get_rssi(&rx) + (1 - alpha) * avg;
	       
			ASYNC_YIELD(return);
		}
		
		printf("%i, %f\n",
		       rx5808_freq_table_get_channel_frequency(&table, i),
		       avg);
	}

	ASYNC_RESET(return);
}

void setup()
{
	Serial.begin(115200);

	analogSetAttenuation(ADC_2_5db);
	pinMode(RX5808_PIN_RSSI, INPUT);
	pinMode(RX5808_PIN_SPI_CS, OUTPUT);
	digitalWrite(RX5808_PIN_SPI_CS, HIGH);

	SPI.begin();

	rx5808_freq_table_init(&table);
	rx5808_init(&rx);
}

void loop()
{
	clock_t delta = get_delta_time_ms();

	enum rx5808_event rx_event = rx5808_update(&rx, delta);

	if (rx_event == RX5808_EVENT_WRITE) {
		size_t i;
		
		uint32_t data = rx5808_get_data_u32(&rx);

		SPI.setBitOrder(LSBFIRST);
		digitalWrite(RX5808_PIN_SPI_CS, LOW);

		for (i = 0; i < rx5808_get_data_len_in_bits(&rx); i += 8)
			SPI.transfer((data >> i) & 0xFF);

		digitalWrite(RX5808_PIN_SPI_CS, HIGH);
		//SPI.setBitOrder(MSBFIRST);
	} else {
		rx5808_ack_rssi(&rx, analogRead(RX5808_PIN_RSSI));
	}

	scan_channels(delta);
}
