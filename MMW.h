#ifdef __cplusplus
extern "C" {
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
 * Publish a message
 */
MmwResult mmw_publish(const char* topic, const char *message);

/**
 * Clean up publishers/subscribers
 */
MmwResult mmw_cleanup();

#ifdef __cplusplus
}
#endif
