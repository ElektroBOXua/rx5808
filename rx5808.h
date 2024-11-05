#ifndef  RX5808_GUARD
#define  RX5808_GUARD

#define RX5808_RSSI_START_DELAY_MS  25
#define RX5808_RSSI_READY_TIME_MS  (RX5808_RSSI_START_DELAY_MS + 5)

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
// RX5808 5.8 gHz video receiver bands
#include <stdint.h>
#include <string.h> //for memcpy
#include <stdlib.h> //for qsort

static const uint16_t rx5808_freq_table_builtin[] = {
	5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, //A
	5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, //B
	5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, //E
	5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, //F
	5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917, //R
	5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621, //D
	5325, 5348, 5366, 5384, 5402, 5420, 5438, 5456, //U
	5474, 5492, 5510, 5528, 5546, 5564, 5582, 5600, //O
	5333, 5373, 5413, 5453, 5493, 5533, 5573, 5613, //L
	5653, 5693, 5733, 5773, 5813, 5853, 5893, 5933, //H
	4990, 5020, 5050, 5080, 5110, 5140, 5170, 5200  //X
};

#define RX5808_FREQ_TABLE_LEN (sizeof(rx5808_freq_table_builtin) / 	      \
			       sizeof(uint16_t))

struct rx5808_freq_table {
	uint16_t frequencies[RX5808_FREQ_TABLE_LEN];
	int iter;
};

int rx5808_freq_compare(const void* a, const void* b)
{
	return (*(uint16_t*)a - *(uint16_t*)b);
}

void rx5808_freq_table_sort(struct rx5808_freq_table *self)
{
	qsort(self->frequencies, RX5808_FREQ_TABLE_LEN, sizeof(uint16_t),
	      rx5808_freq_compare);
}

void rx5808_freq_table_init(struct rx5808_freq_table *self)
{
	memcpy(self->frequencies, rx5808_freq_table_builtin,
	       sizeof(rx5808_freq_table_builtin));

	rx5808_freq_table_sort(self);

	self->iter = 0;
}

uint16_t rx5808_freq_table_get_channel_frequency(
				    struct rx5808_freq_table *self, uint8_t ch)
{
	if (ch >= RX5808_FREQ_TABLE_LEN)
		return 0;

	return self->frequencies[ch];
}

///////////////////////////////////////////////////////////////////////////////
// RX5808 5.8 gHz video receiver module SPI data frame
#include <stdint.h>

enum rx5808_rw { RX5808_READ, RX5808_WRITE };

struct rx5808_frame {
	uint32_t data;
	uint8_t	 len_in_bits;
};

void rx5808_frame_write(struct rx5808_frame *self, uint8_t reg, uint32_t data)
{
	self->len_in_bits = 25;
	self->data        = reg << 0 | RX5808_WRITE << 4 | data << 5;
}

///////////////////////////////////////////////////////////////////////////////
// RX5808 5.8 gHz video receiver module
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>

enum rx5808_registers {
	RX5808_REG_A		       = 0x00,
	RX5808_REG_B		       = 0x01,
	RX5808_REG_C		       = 0x02,
	RX5808_REG_D		       = 0x03,
	RX5808_REG_VCO_SWCAP_CTL       = 0x04,
	RX5808_REG_DFC_CTL	       = 0x05,
	RX5808_REG_6M_AUDIO_DEMOD_CTL  = 0x06,
	RX5808_REG_6M5_AUDIO_DEMOD_CTL = 0x07,
	RX5808_REG_RECV_CTL1	       = 0x08,
	RX5808_REG_RECV_CTL2	       = 0x09,
	RX5808_REG_POWER_DOWN	       = 0x0A,
	RX5808_REG_STATE	       = 0x0F
};

struct rx5808 {
	async async_state;

	uint16_t clock_freq_mhz;
	uint32_t ref_clock_div;
	float	 ref_clock;

	uint16_t set_freq_mhz;
	bool	 set_freq_mhz_update;

	clock_t  rssi_timer;
	uint32_t samples_sum;
	uint32_t samples_count;
	float    rssi_raw;

	struct rx5808_frame frame;
};

///////////////////////////////////////////////////////////////////////////////
// CONFIG
void rx5808_set_clock_freq_mhz(struct rx5808 *self, uint16_t freq)
{
	self->clock_freq_mhz = freq;
}

//ref_clock_freq = (clock_freq / ref_clock_div). Default: 8.
void rx5808_set_ref_clock_divisor(struct rx5808 *self, uint32_t div)
{
	self->ref_clock_div	       = div;
	self->ref_clock		       = (float)self->clock_freq_mhz / div;
}

//Returns true if success.
void rx5808_set_freq_mhz(struct rx5808 *self, uint16_t freq)
{
	self->set_freq_mhz	  = freq;
	self->set_freq_mhz_update = true;
}

///////////////////////////////////////////////////////////////////////////////
// INIT
void rx5808_reset_rssi(struct rx5808 *self)
{
	self->rssi_timer    = 0;
	self->samples_sum   = 0;
	self->samples_count = 0;
	self->rssi_raw      = 0;
}

void rx5808_init(struct rx5808 *self)
{
	self->async_state = 0;

	rx5808_set_clock_freq_mhz(self, 8);
	rx5808_set_ref_clock_divisor(self, 8);
	rx5808_set_freq_mhz(self, 5800);

	rx5808_reset_rssi(self);
}

//Sampling method used for real time rssi averaging
void rx5808_sample_rssi(struct rx5808 *self, uint32_t rssi)
{
	assert(rssi <= 0x7FFFFFFF);

	self->samples_sum += rssi;
	self->samples_count++;

	//Shrink in twice to prevent integer overflow
	if (self->samples_sum >= 0x7FFFFFFF ||
	    self->samples_count >= 0x7FFFFFFF) {
		self->samples_sum   /= 2;
		self->samples_count /= 2;
	}
}

///////////////////////////////////////////////////////////////////////////////
// API
//Returns SPI data (LSBFIRST). CS must be LOW during transmission.
uint32_t rx5808_get_data_u32(struct rx5808 *self) { return self->frame.data; }

uint8_t rx5808_get_data_len_in_bits(struct rx5808 *self)
{
	return self->frame.len_in_bits;
}

//Acknowledge raw RSSI value, so library may use it
void rx5808_ack_rssi(struct rx5808 *self, uint32_t rssi)
{
	self->rssi_raw = rssi;

	//Do not sample until start delay elapsed
	if (self->rssi_timer < RX5808_RSSI_START_DELAY_MS)
		return;

	rx5808_sample_rssi(self, rssi);
}

//Tells if RSSI is ready to be safely measured
bool rx5808_rssi_is_ready(struct rx5808 *self)
{
	return self->rssi_timer >= RX5808_RSSI_READY_TIME_MS;
}

//Returns averaged RSSI
float rx5808_get_rssi(struct rx5808 *self)
{
	if (self->samples_count <= 0)
		return 0;

	return (float)self->samples_sum / self->samples_count;
}

//Returns RAW, unprocessed rssi
uint32_t rx5808_get_rssi_raw(struct rx5808 *self)
{
	return self->rssi_raw;
}

///////////////////////////////////////////////////////////////////////////////
// UPDATE
enum rx5808_event {
	RX5808_EVENT_NONE,
	RX5808_EVENT_WRITE,
};

enum rx5808_event rx5808_try_write_freq_mhz(struct rx5808 *self)
{
	if (!self->set_freq_mhz_update)
		return RX5808_EVENT_NONE;

	self->set_freq_mhz_update = false;

	uint16_t syn_rf_n_reg  = (self->set_freq_mhz - 479) / 2;
	uint8_t	 syn_rf_a_reg  = syn_rf_n_reg % 32;
	syn_rf_n_reg	      /= 32;

	rx5808_frame_write(&self->frame, RX5808_REG_B,
			   (uint32_t)(syn_rf_n_reg << 7) |
			       syn_rf_a_reg);

	rx5808_reset_rssi(self);

	return RX5808_EVENT_WRITE;
}

enum rx5808_event rx5808_update(struct rx5808 *self, clock_t delta)
{
	self->rssi_timer += delta;

	ASYNC_DISPATCH(self->async_state);

	rx5808_frame_write(&self->frame, RX5808_REG_A,
			   self->ref_clock_div);

	ASYNC_YIELD(return RX5808_EVENT_WRITE);

	while (true)
		ASYNC_YIELD(return rx5808_try_write_freq_mhz(self));
}

#endif //RX5808_GUARD
