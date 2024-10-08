#include <stdio.h> //for printf
#include "rx5808.h"

struct rx5808_freq_table ftab;

int main() {
	rx5808_freq_table_init(&ftab);

	for (int i = 0; i < RX5808_FREQ_TABLE_LEN; i++)
		printf("%i\n",
		       rx5808_freq_table_get_channel_frequency(&ftab, i));
	
	return 0;
}
