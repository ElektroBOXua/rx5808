#include <stdio.h> //for printf
#include "rx5808.h"

void rx5808_test_print_ftab()
{
	printf("rx5808_test_print_ftab()\n");

	struct rx5808_freq_table ftab;
	rx5808_freq_table_init(&ftab);

	printf("\t");
	for (int i = 0; i < RX5808_FREQ_TABLE_LEN; i++) {
		printf("%i",
		       rx5808_freq_table_get_channel_frequency(&ftab, i));

		if (i < RX5808_FREQ_TABLE_LEN - 1)
			printf(", ");

		if (!((i + 1) % 8))
			printf("\n\t");
	}

	printf("\n");
}

void rx5808_test_events()
{
	printf("rx5808_test_events()\n");

	struct rx5808 rx;

	rx5808_init(&rx);

	#define RX5808_TEST(cond) \
		if (cond) \
			printf("\tLINE %i: PASSED\n", __LINE__); \
		else \
			printf("\tLINE %i: FAILED\n", __LINE__)

	RX5808_TEST(rx5808_update(&rx, 0) == RX5808_EVENT_WRITE);
	RX5808_TEST(rx5808_update(&rx, 0) == RX5808_EVENT_WRITE);
	RX5808_TEST(rx5808_update(&rx, 0) == RX5808_EVENT_NONE);

	rx5808_set_freq_mhz(&rx, 5400);
	RX5808_TEST(rx5808_update(&rx, 0) == RX5808_EVENT_WRITE);
	RX5808_TEST(rx5808_update(&rx, 0) == RX5808_EVENT_NONE);

	printf("\n");
}

void rx5808_test_rssi()
{
	printf("rx5808_test_rssi()\n");

	struct rx5808 rx;
	rx5808_init(&rx);

	rx5808_update(&rx, 0);
	rx5808_ack_rssi(&rx, 0);

	rx5808_update(&rx, 0);
	rx5808_ack_rssi(&rx, 5);

	RX5808_TEST(rx5808_get_rssi(&rx) == 0.0);
	//printf("\tRSSI: %f\n", rx5808_get_rssi(&rx));

	rx5808_update(&rx, 25);
	rx5808_ack_rssi(&rx, 5);

	RX5808_TEST(rx5808_get_rssi(&rx) == 5.0);
	//printf("\tRSSI: %f\n", rx5808_get_rssi(&rx));

	rx5808_update(&rx, 25);
	rx5808_ack_rssi(&rx, 10);

	RX5808_TEST(rx5808_get_rssi(&rx) == 7.5);
	//printf("\tRSSI: %f\n", rx5808_get_rssi(&rx));

	printf("\n");
}

int main()
{
	rx5808_test_print_ftab();
	rx5808_test_events();
	rx5808_test_rssi();

	return 0;
}
