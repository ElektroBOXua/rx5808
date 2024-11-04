#include <stdio.h> //for printf
#include "rx5808.h"

void rx5808_test_print_ftab()
{
	struct rx5808_freq_table ftab;
	rx5808_freq_table_init(&ftab);

	printf("Traversing through all registered frequencies:\n\t");
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
	struct rx5808 rx;
	
	rx5808_init(&rx);

	#define RX5808_TEST(cond) \
		if (cond) \
			printf("LINE %i: PASSED\n", __LINE__); \
		else \
			printf("LINE %i: FAILED\n", __LINE__)
	
	RX5808_TEST(rx5808_update(&rx) == RX5808_EVENT_WRITE);
	RX5808_TEST(rx5808_update(&rx) == RX5808_EVENT_WRITE);
	RX5808_TEST(rx5808_update(&rx) == RX5808_EVENT_NONE);
	
	rx5808_set_freq_mhz(&rx, 5400);
	RX5808_TEST(rx5808_update(&rx) == RX5808_EVENT_WRITE);
	RX5808_TEST(rx5808_update(&rx) == RX5808_EVENT_NONE);
}

int main()
{
	rx5808_test_print_ftab();
	rx5808_test_events();

	return 0;
}
