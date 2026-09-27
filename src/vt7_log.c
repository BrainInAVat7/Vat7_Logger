/* Defines the logging system */


#include "vat7_logger.h"
#include "vat7_precision_timing.h"

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>


#define LOG_FILE "log/logging.log"
#define LOG_BUFFER_SIZE 262144u  // 256 kb
#define LOG_BUFFER_MASK 262143u
#define FLUSH_BUFFER_SIZE 4194304u  // 4 mb
/* Header is 1 char followed by a uint16 */
#define LOG_HEADER_SIZE (sizeof(char) + sizeof(uint16_t))
#define LOG_ENTRY_MAX_SIZE 1021u

#define LOG_ENTRY_STATUS_UNCOMMITTED 0u
#define LOG_ENTRY_STATUS_COMMITTED 1u


_Static_assert(((LOG_BUFFER_SIZE) & (LOG_BUFFER_SIZE -1)) == 0,
	"Log buffer size must be a power of 2.");
_Static_assert(LOG_BUFFER_MASK == LOG_BUFFER_SIZE - 1,
	"Log buffer mask must be 1 less than log buffer size.");
_Static_assert(LOG_BUFFER_SIZE % (LOG_ENTRY_MAX_SIZE + LOG_HEADER_SIZE) == 0,
	"Log entry max plus header bytes must divide evenly into"
	"Log buffer size");
_Static_assert(LOG_ENTRY_MAX_SIZE <= UINT16_MAX,
	"Log entry max length must be less than uint16 max.");


/* SHARED STATE */

// NOTE: Cursors can have values greater than LOG_BUFFER_SIZE, so
// user code must mod to range using bitwise and with LOG_BUFFER_MASK
static _Atomic char log_buffer[LOG_BUFFER_SIZE] =
	{LOG_ENTRY_STATUS_UNCOMMITTED};
static _Atomic size_t log_write_cursor = 0;
static _Atomic size_t log_read_cursor = 0;
static _Atomic uint64_t successful_log_entries = 0;
static _Atomic uint64_t failed_log_entries = 0;
static _Atomic bool logging_thread_running;

/* END SHARED STATE */

static uint64_t vt7_log_start_time_count = 0;
static double vt7_log_time_frequency = 0.0;
static FILE *log_output_file;
static char flush_buffer[FLUSH_BUFFER_SIZE] = {'\0'};
static pthread_t logging_thread;


static void *_vt7_log_start_flush_loop (void *arg)
{
	(void)arg;
	bool run = true;
	while (run)
	{
		_vt7_log_flush(false);
		__atomic_load(&logging_thread_running, &run, __ATOMIC_ACQUIRE);
	}
	return NULL;
}


void _vt7_log_init (void)
{
	vt7_log_start_time_count = vt7_pt_count();
	vt7_log_time_frequency = (double)vt7_pt_frequency();
	// _vt7_log_shutdown reverses this
	log_output_file = fopen(LOG_FILE, "w");
	logging_thread_running = true;
	pthread_create(&logging_thread, NULL, _vt7_log_start_flush_loop,
		NULL);
}


/* Return a current timestamp in seconds logging init.
 *
 * This function is a helper for _vt7_log_create_entry
 */
static inline double logging_get_timestamp (void)
{
	return (double)(vt7_pt_count() - vt7_log_start_time_count)
		/ vt7_log_time_frequency;
}


/* Create a log entry.
 * Used by logging macros and should not be called
 * directly by usage code.
 */
void _vt7_log_create_entry
(
	int log_level,
	const char *src_file,
	int line,
	const char *fmt_str,
	...
)
{
	if (!logging_thread_running)
	{
		fprintf(stderr, "%s",
			"Log attempted without initializing logging\n");
		return;
	}
	// Begin va* idiom
	char message_buffer[LOG_ENTRY_MAX_SIZE];
	// Would be odd to violate this, but truncation handling code
	// at several points in this function relies on it.
	_Static_assert(sizeof(message_buffer) > 3,
		"Message buffer to small for truncation string");
	va_list args;
	va_start(args, fmt_str);
	int msg_written = vsnprintf(
		message_buffer, sizeof(message_buffer), fmt_str, args);
	va_end(args);
	//end va* idiom

	const char msg_fmt_fail[] = "[Failed to format log message]";
	_Static_assert(sizeof(message_buffer) > sizeof(msg_fmt_fail),
		"Message buffer too small for format failure message");
	if (msg_written < 0)
	{
		for (size_t i = 0; i < sizeof(msg_fmt_fail); i++)
		{
			message_buffer[i] = msg_fmt_fail[i];
		}
	}

	// Need else here because size_t cast would make negative
	// msg_written value a huge positive number.
	else if ((size_t)msg_written >= sizeof(message_buffer))
	{
		message_buffer[sizeof(message_buffer) - 1] = '\0';
		message_buffer[sizeof(message_buffer) - 2] = '.';
		message_buffer[sizeof(message_buffer) - 3] = '.';
		message_buffer[sizeof(message_buffer) - 4] = '.';
	}

	// Create log entry
	char log_entry_buffer[LOG_ENTRY_MAX_SIZE];
	// This with previous assert that msg_buffer is large enough for
	// truncation marker ensures log_entry_buffer is also large
	// enough for truncation marker.
	_Static_assert(sizeof(log_entry_buffer) >= sizeof(message_buffer),
		"Log entry buffer cannot be smaller than message buffer");
	char *level_string;
	switch(log_level)
	{
		case VT7_LOG_LEVEL_CRITICAL: level_string = "CRITICAL"; break;
		case VT7_LOG_LEVEL_ERROR: level_string = "ERROR"; break;
		case VT7_LOG_LEVEL_WARNING: level_string = "WARNING"; break;
		case VT7_LOG_LEVEL_INFO: level_string = "INFO"; break;
		case VT7_LOG_LEVEL_DEBUG: level_string = "DEBUG"; break;
		case VT7_LOG_LEVEL_TRACE: level_string = "TRACE"; break;
		default: level_string = "UNDEFINED"; break;
	}

	double timestamp = logging_get_timestamp();

	int entry_written = snprintf
	(
	 	log_entry_buffer,
		sizeof(log_entry_buffer),
	 	"[%s] %s line %d at %f seconds\n\t%s\n\n",
		level_string,
		src_file,
		line,
		timestamp,
		message_buffer
	);

	const char log_fmt_fail[] = "[Failed to format log entry]a\n\n";
	_Static_assert(sizeof(log_entry_buffer) >= sizeof(log_fmt_fail),
		"Log entry buffer too small for log format failure message");
	if (entry_written < 0)
	{
		for (size_t i = 0; i < sizeof(log_fmt_fail); i++)
		{
			log_entry_buffer[i] = log_fmt_fail[i];
		}
	}

	// Need else here because size_t cast would make negative
	// msg_written value a huge positive number.
	else if ((size_t)entry_written >= sizeof(log_entry_buffer))
	{
		log_entry_buffer[sizeof(log_entry_buffer) - 1] = '\0';
		log_entry_buffer[sizeof(log_entry_buffer) - 2] = '.';
		log_entry_buffer[sizeof(log_entry_buffer) - 3] = '.';
		log_entry_buffer[sizeof(log_entry_buffer) - 4] = '.';
		// This is safe because if by some crazy turn of events
		// the entry is larger than INT_MAX, C standard
		// dictates snprintf return negative, so entry_written
		// would be negative and handled above.
		entry_written = (int)(sizeof(log_entry_buffer) - 1);
	}

	// Add entry to log.
	_Static_assert(sizeof(log_buffer) >= sizeof(log_entry_buffer),
		"Log entry buffer cannot be larger than log buffer");

	size_t log_cursor = __atomic_fetch_add(&log_write_cursor,
		(size_t)entry_written + LOG_HEADER_SIZE, __ATOMIC_SEQ_CST);

	size_t current_log_read_cursor_value;
	__atomic_load(&log_read_cursor, &current_log_read_cursor_value,
		__ATOMIC_ACQUIRE);
	size_t buffer_used = log_cursor - current_log_read_cursor_value;
	if (buffer_used + LOG_HEADER_SIZE + (size_t)entry_written
		>= LOG_BUFFER_SIZE)
	{
		// Once a failure happens, the reader encounters, the
		// reserved space is never committed, so the reader
		// stops. The distance between write and read cursor
		// gets larger with more reservations, so this keeps
		// triggering and logs will fail until the program
		// ends. If this happens, the solution is either to
		// make the buffer larger or flush more often.
		__atomic_fetch_add(&failed_log_entries, 1, __ATOMIC_RELAXED);

		return;
	}

	for (size_t i = 0; i < (size_t)(entry_written); i++)
	{
		size_t log_index =
			(size_t)((log_cursor + i + LOG_HEADER_SIZE)
				& LOG_BUFFER_MASK);
		log_buffer[log_index] = log_entry_buffer[i];
	}

	// Fill Log Entry Header
	char flag = LOG_ENTRY_STATUS_COMMITTED;
	uint16_t entry_size = (uint16_t)entry_written;
	char entry_size_bytes[2];
	memcpy(entry_size_bytes, &entry_size, 2);
	log_buffer[(log_cursor + 2) & LOG_BUFFER_MASK] = entry_size_bytes[1];
	log_buffer[(log_cursor + 1) & LOG_BUFFER_MASK] = entry_size_bytes[0];
	__atomic_store(&log_buffer[log_cursor & LOG_BUFFER_MASK],
		&flag, __ATOMIC_RELEASE);

	__atomic_fetch_add(&successful_log_entries, 1, __ATOMIC_RELAXED);
}


/* Flush the log buffer
 */
void _vt7_log_flush(bool force_flush)
{
	size_t local_read_cursor;
	__atomic_load(&log_read_cursor, &local_read_cursor,
		__ATOMIC_ACQUIRE);
	char read_ready_flag;
	__atomic_load(&log_buffer[local_read_cursor & LOG_BUFFER_MASK],
		&read_ready_flag, __ATOMIC_ACQUIRE);

	static bool flush_buffer_full = false;
	static size_t flush_buffer_cursor = 0;
	while(read_ready_flag == LOG_ENTRY_STATUS_COMMITTED)
	{
		char log_entry_size_raw[2];
		uint16_t log_entry_size = 0;
		log_entry_size_raw[0] =
			log_buffer[(local_read_cursor + 1) & LOG_BUFFER_MASK];
		log_entry_size_raw[1] =
			log_buffer[(local_read_cursor + 2) & LOG_BUFFER_MASK];
		memcpy(&log_entry_size, log_entry_size_raw, 2);

		if (flush_buffer_cursor + log_entry_size
			>= sizeof(flush_buffer))
		{
			flush_buffer_full = true;
			break;
		}

		for (size_t i = 0; i < log_entry_size; i++)
		{
			size_t log_index =
				(i + LOG_HEADER_SIZE + local_read_cursor);
			flush_buffer[flush_buffer_cursor + i] =
				log_buffer[log_index & LOG_BUFFER_MASK];
		}

		char updated_header_status = LOG_ENTRY_STATUS_UNCOMMITTED;
		__atomic_store(&updated_header_status,
			&log_buffer[local_read_cursor & LOG_BUFFER_MASK],
			__ATOMIC_RELEASE);

		flush_buffer_cursor += log_entry_size;
		local_read_cursor += LOG_HEADER_SIZE + log_entry_size;
		__atomic_load(&log_buffer[local_read_cursor & LOG_BUFFER_MASK],
			&read_ready_flag, __ATOMIC_ACQUIRE);

		__atomic_store(&log_read_cursor, &local_read_cursor,
			__ATOMIC_RELEASE);
	}

	if (force_flush || flush_buffer_full)
	{
		if (log_output_file != NULL)
		{
			size_t write_success = fwrite(flush_buffer, 1,
				flush_buffer_cursor, log_output_file);
			if (write_success != flush_buffer_cursor)
			{
				fprintf(stderr, "%s",
					"Error writing log to file");
			}
		}
		else
		{
			// If this fails, fail silently. Nothing is output.
			// If logs aren't writing, make sure to troubleshoot
			// by running from the console.
			fprintf(stderr, "%s", "Failed to write to log file.");
		}

		if (flush_buffer_full)
		{
			flush_buffer_full = false;
			flush_buffer_cursor = 0;
			// clear flush buffer to avoid reprinting old logs
			memset(flush_buffer, '\0', sizeof(flush_buffer));
		}
	}
}


// TODO document this
void _vt7_log_shutdown(void)
{
	if (!logging_thread_running)
	{
		fprintf(stderr, "%s",
			"Logging shutdown attempted without initializing"
			" logging\n");
		return;
	}
	bool run_log = false;
	__atomic_store(&logging_thread_running, &run_log, __ATOMIC_RELEASE);
	pthread_join(logging_thread, NULL);

	_vt7_log_flush(true);

	uint64_t successes;
	uint64_t failures;
	__atomic_load(&successful_log_entries, &successes,
		__ATOMIC_ACQUIRE);
	__atomic_load(&failed_log_entries, &failures,
		__ATOMIC_ACQUIRE);

	if (log_output_file != NULL)
	{
		fprintf(log_output_file, "\n\nSuccessful Logs: %"PRIu64"\n"
			"Failed Logs: %"PRIu64"\n",
			successes, failures);
	}
	else
	{
		// If this fails, fail silently. Nothing is output.
		// If logs aren't writing, make sure to troubleshoot
		// by running from the console.
		fprintf(stderr, "%s", "Failed to write to log file.");
	}
	fclose(log_output_file);
}
