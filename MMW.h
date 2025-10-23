#ifdef __cplusplus
#include <cstddef>
extern "C" {
#else
#include <stddef.h>
#endif

typedef enum {
    MMW_OK,
    MMW_ERROR
} MmwResult;

/**
 * Initialize library settings
 */
MmwResult mmw_initialize(const char* configPath);

/**
 * Create a publisher
 */
MmwResult mmw_create_publisher(const char* topic);

/**
 * Create a subscriber
 */
MmwResult mmw_create_subscriber(const char* topic, void (*mmw_callback)(const char*));

/**
 * Create a subscriber
 */
MmwResult mmw_create_subscriber_raw(const char* topic, void (*mmw_callback)(void*));

/**
 * Publish a message
 */
MmwResult mmw_publish(const char* topic, const char *message);

/**
 * Publish a message of raw bytes
 */
MmwResult mmw_publish_raw(const char* topic, void* message, size_t size);

/**
 * Clean up publishers/subscribers
 */
MmwResult mmw_cleanup();


#ifdef __cplusplus
}
#endif
