/* Header for logging system
 *
 * Usage:
 *
 * Use the appropriate log leve macro as a if it were
 * a function call. The first argument should be either
 * a string literal or format string followed by an
 * unlimited number of optional arguments as necessary
 * to specify the values of a format string.
 *
 * Example:
 *
 * DEBUG_LOG("This is debug log entry %d", 1);
 *
 * The value of the VT7_LOG_LEVEL macro determines which
 * log levels, if any, are compiled.
 *
 * Macros are provided for each log level. See below.
 *
 * Log macros include
 *
 * CRITICAL_LOG		(indicates imminent crash)
 * ERROR_LOG		(program will continue in a degraded state)
 * WARNING_LOG		(possible issue or error state)
 * INFO_LOG		(important event)
 * DEBUG_LOG		(potentially useful details)
 * TRACE_LOG		(fine grained program details)
 */

#ifndef VAT7_LOGGER_H
#define VAT7_LOGGER_H


#include <stdbool.h>


/* DO NOT CALL THESE DIRECTLY. USE THE MACROS BELOW */
void _vt7_log_init (void);
void _vt7_log_create_entry
(
	int log_level,
	const char *src_file,
	int line,
	const char *fmt_str,
	...
);
//TODO remove this function from header
void _vt7_log_flush (bool force_flush);
void _vt7_log_shutdown (void);


/* Defining VT7_LOG_LEVEL using these macros enables all log
 * levels up to and including that level. If VT7_LOG_LEVEL is
 * defined as VT7_LOG_LEVEL_DISABLED (i.e., 0), then no logging
 * code will run
 */
#define VT7_LOG_LEVEL_DISABLED 0  // logging disabled
#define VT7_LOG_LEVEL_CRITICAL 1  // imminent crash
#define VT7_LOG_LEVEL_ERROR    2  // can continue in a degraded state
#define VT7_LOG_LEVEL_WARNING  3  // possible issue or error state
#define VT7_LOG_LEVEL_INFO     4  // important event
#define VT7_LOG_LEVEL_DEBUG    5  // potentially useful details
#define VT7_LOG_LEVEL_TRACE    6  // extreme detail


#ifndef VT7_LOG_LEVEL
#define VT7_LOG_LEVEL VT7_LOG_LEVEL_DISABLED
#endif


#if (VT7_LOG_LEVEL >= VT7_LOG_LEVEL_TRACE)
	#define VT7_LOG_TRACE(...) \
		do {_vt7_log_create_entry( \
			VT7_LOG_LEVEL_TRACE, __FILE__, __LINE__, \
			__VA_ARGS__);} \
		while(0)
#else
	#define VT7_LOG_TRACE(...) do { } while(0)
#endif


#if (VT7_LOG_LEVEL >= VT7_LOG_LEVEL_DEBUG)
	#define VT7_LOG_DEBUG(...) \
		do {_vt7_log_create_entry( \
			VT7_LOG_LEVEL_DEBUG, __FILE__, __LINE__, \
			__VA_ARGS__);} \
		while(0)
#else
	#define VT7_LOG_DEBUG(...) do { } while(0)
#endif


#if (VT7_LOG_LEVEL >= VT7_LOG_LEVEL_INFO)
	#define VT7_LOG_INFO(...) \
		do {_vt7_log_create_entry( \
			VT7_LOG_LEVEL_INFO, __FILE__, __LINE__, \
			__VA_ARGS__);} \
		while(0)
#else
	#define VT7_LOG_INFO(...) do { } while(0)
#endif


#if (VT7_LOG_LEVEL >= VT7_LOG_LEVEL_WARNING)
	#define VT7_LOG_WARNING(...) \
		do {_vt7_log_create_entry( \
			VT7_LOG_LEVEL_WARNING, __FILE__, __LINE__, \
			__VA_ARGS__);} \
		while(0)
#else
	#define VT7_LOG_WARNING(...) do { } while(0)
#endif


#if (VT7_LOG_LEVEL >= VT7_LOG_LEVEL_ERROR)
	#define VT7_LOG_ERROR(...) \
		do {_vt7_log_create_entry( \
			VT7_LOG_LEVEL_ERROR, __FILE__, __LINE__, \
			__VA_ARGS__);} \
		while(0)
#else
	#define VT7_LOG_ERROR(...) do { } while(0)
#endif


#if (VT7_LOG_LEVEL >= VT7_LOG_LEVEL_CRITICAL)
	#define VT7_LOG_CRITICAL(...) \
		do {_vt7_log_create_entry( \
			VT7_LOG_LEVEL_CRITICAL, __FILE__, __LINE__, \
			__VA_ARGS__);} \
		while(0)
	#define VT7_LOG_INIT() do {_vt7_log_init();} while(0)
	#define VT7_LOG_SHUTDOWN() do {_vt7_log_shutdown();}while(0)
#else
	#define VT7_LOG_CRITICAL(...) do { } while(0)
	#define VT7_LOG_INIT() do { } while(0)
	#define VT7_LOG_SHUTDOWN() do { } while(0)
#endif


#endif  // header guard
