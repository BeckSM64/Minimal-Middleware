/**
 * Create a publisher
 */
int mmw_create_publisher(const char* topic);

/**
 * Create a subscriber
 */
int mmw_create_subscriber(const char* topic, void (*mmw_callback)(const char*));

/**
 * Publish a message
 */
int mmw_publish(const char* topic, const char *message);

/**
 * Clean up publishers/subscribers
 */
int mmw_cleanup();
