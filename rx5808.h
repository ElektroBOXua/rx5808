#ifndef  RX5808_GUARD
#define  RX5808_GUARD

///////////////////////////////////////////////////////////////////////////////
// RX5808 5.8 gHz video receiver bands */
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

struct rx5808_frame {
	uint32_t data;
	uint8_t	 len_in_bits;
};

uint8_t rx5808_frame_get_len_in_bits(struct rx5808_frame *self)
{
	return self->len_in_bits;
}

///////////////////////////////////////////////////////////////////////////////
// RX5808 5.8 gHz video receiver module
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>

enum rx5808_event {
	RX5808_EVENT_NONE,
	RX5808_EVENT_WRITE,
	RX5808_EVENT_QUERY_RSSI
};

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

enum rx5808_rw { RX5808_READ, RX5808_WRITE };

struct rx5808 {
	struct rx5808_frame frame;
	enum rx5808_event   event;

	float rssi;

	uint16_t clock_freq_mhz;
	uint32_t ref_clock_div;
	float	 ref_clock;
	bool	 set_clock_divisor_update;

	uint16_t set_freq_mhz;
	bool	 set_freq_mhz_update;
};

// PRIVATE --------------------------------------------------------------------
void rx5808_write_frame(struct rx5808 *self, uint8_t reg, uint32_t data)
{
	self->frame.len_in_bits = 25;
	self->frame.data	= reg << 0 | RX5808_WRITE << 4 | data << 5;
}

bool rx5808_event_is_processed(struct rx5808 *self, enum rx5808_event event)
{
	return self->event != event;
}

// CONFIG ---------------------------------------------------------------------
void rx5808_set_clock_freq_mhz(struct rx5808 *self, uint16_t freq)
{
	self->clock_freq_mhz = freq;
}

//ref_clock_freq = (clock_freq / ref_clock_div). Default: 8.
bool rx5808_set_ref_clock_divisor(struct rx5808 *self, uint32_t div)
{
	if (self->set_clock_divisor_update)
		return false;

	self->ref_clock_div	       = div;
	self->ref_clock		       = (float)self->clock_freq_mhz / div;
	self->set_clock_divisor_update = true;

	return true;
}

//Returns true if success.
bool rx5808_set_freq_mhz(struct rx5808 *self, uint16_t freq)
{
	if (self->set_freq_mhz_update)
		return false;

	self->set_freq_mhz	  = freq;
	self->set_freq_mhz_update = true;

	return true;
}

// INIT -----------------------------------------------------------------------
void rx5808_init(struct rx5808 *self)
{
	self->event = RX5808_EVENT_NONE;

	self->rssi = 0;

	rx5808_set_clock_freq_mhz(self, 8);
	rx5808_set_ref_clock_divisor(self, 8);
	rx5808_set_freq_mhz(self, 5800);
}

// API ------------------------------------------------------------------------
//Returns SPI data (LSBFIRST). CS must be LOW during transmission.
uint32_t rx5808_get_data_u32(struct rx5808 *self) { return self->frame.data; }

uint8_t rx5808_get_data_len_in_bits(struct rx5808 *self)
{
	return self->frame.len_in_bits;
}

//Acknowledge rx5808 module that we successfully wrote SPI frame
void rx5808_ack_write(struct rx5808 *self)
{
	if (self->event == RX5808_EVENT_WRITE)
		self->event = RX5808_EVENT_NONE;
}

//Acknowledge rx5808 module that we successfully acknowledged new rssi value
void rx5808_ack_rssi(struct rx5808 *self, float rssi)
{
	self->rssi = rssi;

	if (self->event == RX5808_EVENT_QUERY_RSSI)
		self->event = RX5808_EVENT_NONE;
}

void rx5808_ack_all(struct rx5808 *self) { self->event = RX5808_EVENT_NONE; }

// UPDATE ---------------------------------------------------------------------
float rx5808_get_rssi(struct rx5808 *self) { return self->rssi; }

enum rx5808_event rx5808_get_event(struct rx5808 *self) { return self->event; }

enum rx5808_event rx5808_update(struct rx5808 *self)
{
	/* Assert if mandatory events had not been processed. */
	assert(rx5808_event_is_processed(self, RX5808_EVENT_WRITE));
	assert(rx5808_event_is_processed(self, RX5808_EVENT_QUERY_RSSI));

	if (self->set_clock_divisor_update) {
		self->set_clock_divisor_update = false;

		rx5808_write_frame(self, RX5808_REG_A, self->ref_clock_div);

		self->event = RX5808_EVENT_WRITE;
		return self->event;
	}

	if (self->set_freq_mhz_update) {
		uint16_t syn_rf_n_reg  = (self->set_freq_mhz - 479) / 2;
		uint8_t	 syn_rf_a_reg  = syn_rf_n_reg % 32;
		syn_rf_n_reg	      /= 32;

		self->set_freq_mhz_update = false;

		rx5808_write_frame(self, RX5808_REG_B,
				   (uint32_t)(syn_rf_n_reg << 7) |
				       syn_rf_a_reg);

		self->event = RX5808_EVENT_WRITE;
		return self->event;
	}

	self->event = RX5808_EVENT_QUERY_RSSI;
	return self->event;
}

#endif //RX5808_GUARD
