#ifdef __cplusplus
#include <cstddef>
extern "C" {
#else
#include <stddef.h>
#endif

/**
 * @enum MmwResult
 * @brief Return codes for all MMW functions.
 */
typedef enum {
    MMW_OK,     /**< Operation completed successfully. */
    MMW_ERROR   /**< Operation failed. */
} MmwResult;

/**
 * @enum MmwReliability
 * @brief Reliability of MMW messages.
 */
typedef enum {
    MMW_BEST_EFFORT, // Fire and forget
    MMW_RELIABLE // At least once
} MmwReliability;

/**
 * @brief Initialize the middleware library.
 *
 * Must be called before creating publishers or subscribers.
 * Optionally loads configuration from a file.
 *
 * @param configPath Path to configuration file (can be NULL for defaults).
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_initialize(const char* brokerIp, unsigned short port);

/**
 * @brief Create a publisher for a topic.
 *
 * Allows publishing messages to all subscribers of the specified topic.
 *
 * @param topic The topic name.
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_create_publisher(const char* topic);

/**
 * @brief Create a subscriber for a topic (string messages).
 *
 * Registers a callback that is invoked when a message is received.
 *
 * @param topic The topic name.
 * @param mmw_callback Callback function that receives the message as a string.
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_create_subscriber(const char* topic, void (*mmw_callback)(const char*));

/**
 * @brief Create a subscriber for a topic (raw byte messages).
 *
 * Registers a callback that is invoked with a pointer to raw message data.
 *
 * @param topic The topic name.
 * @param mmw_callback Callback function that receives the raw message data.
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_create_subscriber_raw(const char* topic, void (*mmw_callback)(void*));

/**
 * @brief Publish a message as a string.
 *
 * Sends a UTF-8 string message to all subscribers of the topic.
 *
 * @param topic The topic name.
 * @param message The message to publish.
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_publish(const char* topic, const char* message, MmwReliability reliability);

/**
 * @brief Publish a message as raw bytes.
 *
 * Sends an arbitrary block of memory to all subscribers of the topic.
 *
 * @param topic The topic name.
 * @param message Pointer to message data.
 * @param size Size of the message in bytes.
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_publish_raw(const char* topic, void* message, size_t size, MmwReliability reliability);

/**
 * @brief Clean up all middleware resources.
 *
 * Destroys all publishers and subscribers and releases internal memory.
 *
 * @return MMW_OK on success, MMW_ERROR on failure.
 */
MmwResult mmw_cleanup();

#ifdef __cplusplus
}
#endif
