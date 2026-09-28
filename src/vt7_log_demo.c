/* A demo for the Vat7 Logger */


#include "vat7_logger.h"

#include <stdlib.h>


int main (void)
{
	VT7_LOG_INIT();
	VT7_LOG_CRITICAL("Demo log %d", 1);
	VT7_LOG_ERROR("Demo log %d", 2);
	VT7_LOG_WARNING("Demo log %d", 3);
	VT7_LOG_INFO("Demo log %d", 4);
	VT7_LOG_DEBUG("Demo log %d", 5);
	VT7_LOG_TRACE("Demo log %d", 6);
	VT7_LOG_SHUTDOWN();

	return EXIT_SUCCESS;
}
