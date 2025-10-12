/**
 * Create a publisher
 */
int mmw_create_publisher();

/**
 * Create a subscriber
 */
int mmw_create_subscriber(void (*mmw_callback)(const char*));

/**
 * Publish a message
 */
int mmw_publish(const char *message);

/**
 * Clean up publishers/subscribers
 */
int mmw_cleanup();
