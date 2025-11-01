/**
 * @file mqtt_client_persistent.hpp
 * @brief Persistent MQTT Client with Auto-Reconnection for Dynamic IP
 * 
 * Handles VMG's changing IP address as vehicle moves between cell towers.
 * Uses Clean Session = False and automatic reconnection with exponential backoff.
 */

#ifndef MQTT_CLIENT_PERSISTENT_HPP
#define MQTT_CLIENT_PERSISTENT_HPP

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <map>
#include <queue>

namespace vmg {

/**
 * @brief Message callback function
 */
using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

/**
 * @brief Connection statistics
 */
struct ConnectionStats {
    uint64_t total_connections = 0;
    uint64_t failed_connections = 0;
    uint64_t reconnections = 0;
    uint64_t messages_sent = 0;
    uint64_t messages_received = 0;
    uint64_t messages_queued = 0;
    std::chrono::steady_clock::time_point last_connection_time;
    std::string current_local_ip;
    std::string last_known_ip;
    bool is_connected = false;
};

/**
 * @brief Queued message for offline buffering
 */
struct QueuedMessage {
    std::string topic;
    std::string payload;
    int qos;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Persistent MQTT Client
 * 
 * Features:
 * - Automatic reconnection on network changes
 * - Exponential backoff for retries
 * - Session persistence (Clean Session = False)
 * - QoS 1 for reliable message delivery
 * - IP change detection
 * - Message queueing during disconnection
 * - Thread-safe operations
 */
class MQTTClientPersistent {
public:
    /**
     * @brief Constructor
     * 
     * @param server_url MQTT broker URL (e.g., "ssl://ota-server.com:8883")
     * @param client_id Unique client ID (e.g., "vehicle-VIN123456")
     */
    MQTTClientPersistent(const std::string& server_url,
                         const std::string& client_id);
    
    ~MQTTClientPersistent();
    
    /**
     * @brief Initialize client with TLS credentials
     * 
     * @param cert_path Client certificate path
     * @param key_path Client private key path
     * @param ca_path CA certificate path
     * @return true on success
     */
    bool initialize(const std::string& cert_path,
                   const std::string& key_path,
                   const std::string& ca_path);
    
    /**
     * @brief Start connection manager
     * 
     * Starts background thread that maintains connection
     */
    void start();
    
    /**
     * @brief Stop connection manager
     * 
     * Gracefully disconnects and stops background thread
     */
    void stop();
    
    /**
     * @brief Publish message
     * 
     * @param topic MQTT topic
     * @param payload Message payload
     * @param qos Quality of Service (0, 1, 2)
     * @return true if queued/sent successfully
     */
    bool publish(const std::string& topic,
                const std::string& payload,
                int qos = 1);
    
    /**
     * @brief Subscribe to topic
     * 
     * @param topic MQTT topic (supports wildcards: +, #)
     * @param callback Message callback
     * @param qos Quality of Service
     * @return true on success
     */
    bool subscribe(const std::string& topic,
                  MessageCallback callback,
                  int qos = 1);
    
    /**
     * @brief Unsubscribe from topic
     * 
     * @param topic MQTT topic
     * @return true on success
     */
    bool unsubscribe(const std::string& topic);
    
    /**
     * @brief Check if connected
     */
    bool isConnected() const;
    
    /**
     * @brief Get connection statistics
     */
    ConnectionStats getStats() const;
    
    /**
     * @brief Set reconnection parameters
     * 
     * @param initial_delay_ms Initial delay in milliseconds
     * @param max_delay_ms Maximum delay in milliseconds
     * @param multiplier Exponential backoff multiplier
     */
    void setReconnectionParams(uint32_t initial_delay_ms,
                              uint32_t max_delay_ms,
                              uint32_t multiplier);
    
    /**
     * @brief Set message queue limit
     * 
     * @param max_queued Maximum messages to queue when offline
     */
    void setMessageQueueLimit(size_t max_queued);
    
    /**
     * @brief Force reconnection
     * 
     * Useful when network change is detected externally
     */
    void forceReconnect();

private:
    /**
     * @brief Connection management loop
     * 
     * Runs in background thread
     */
    void connectionLoop();
    
    /**
     * @brief Attempt to connect to broker
     * 
     * @return true if connected successfully
     */
    bool attemptConnection();
    
    /**
     * @brief Handle connection lost event
     */
    void onConnectionLost();
    
    /**
     * @brief Handle message arrived event
     */
    void onMessageArrived(const std::string& topic, const std::string& payload);
    
    /**
     * @brief Detect IP address change
     * 
     * @return true if IP changed since last check
     */
    bool detectIPChange();
    
    /**
     * @brief Get current local IP address
     * 
     * @return IP address string (e.g., "10.1.2.3")
     */
    std::string getCurrentLocalIP();
    
    /**
     * @brief Process queued messages
     * 
     * Sends queued messages after reconnection
     */
    void processQueuedMessages();
    
    /**
     * @brief Add message to queue
     * 
     * @param topic MQTT topic
     * @param payload Message payload
     * @param qos Quality of Service
     */
    void queueMessage(const std::string& topic,
                     const std::string& payload,
                     int qos);

private:
    // Connection parameters
    std::string server_url_;
    std::string client_id_;
    std::string cert_path_;
    std::string key_path_;
    std::string ca_path_;
    
    // Runtime state
    std::atomic<bool> is_running_;
    std::atomic<bool> is_connected_;
    std::thread connection_thread_;
    mutable std::mutex mutex_;
    
    // MQTT client handle (platform-specific)
    void* mqtt_client_;
    
    // Reconnection settings
    uint32_t reconnect_delay_ms_;
    uint32_t max_reconnect_delay_ms_;
    uint32_t reconnect_multiplier_;
    uint32_t current_delay_ms_;
    
    // IP tracking
    std::string last_known_ip_;
    
    // Message queue
    std::queue<QueuedMessage> message_queue_;
    size_t max_queue_size_;
    
    // Subscriptions
    std::map<std::string, MessageCallback> subscriptions_;
    
    // Statistics
    mutable ConnectionStats stats_;
    
    // Static callbacks for C library integration
    static void staticConnectionLostCallback(void* context, char* cause);
    static int staticMessageArrivedCallback(void* context, char* topicName,
                                           int topicLen, void* message);
};

} // namespace vmg

#endif // MQTT_CLIENT_PERSISTENT_HPP

