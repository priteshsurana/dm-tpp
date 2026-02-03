#pragma once
#include "error_codes.h"
#include <string>
#include <map>
#include <optional>
#include <memory>

namespace dmtpp::core {
struct Error {
    // Error code from taxonomy
    ErrorCode code;
    
    // Component that produced error (e.g., "kafka_producer", "postgres_client")
    std::string component;
    
    // Operation that failed (e.g., "send_message", "begin_transaction")
    std::string operation;
    
    // Human-readable message
    std::string message;
    
    // Context key-value pairs (e.g., txn_id, partition, offset)
    std::map<std::string, std::string> context;
    
    // Optional causal error (if this error wraps another)
    std::optional<std::shared_ptr<Error>> cause;
    
    // Constructors
    Error(ErrorCode code, std::string component, std::string operation, std::string message)
        : code(code), component(std::move(component)), operation(std::move(operation)), 
          message(std::move(message)) {}
    
    Error(ErrorCode code, std::string component, std::string operation, std::string message,
          std::map<std::string, std::string> context)
        : code(code), component(std::move(component)), operation(std::move(operation)),
          message(std::move(message)), context(std::move(context)) {}
    
    // Helpers
    bool is_retriable() const {
        return core::is_retriable(code);
    }
    
    bool is_fatal() const {
        return core::is_fatal(code);
    }
    
    std::string category() const {
        return error_category(code);
    }
    
    // Add context field
    Error& with_context(const std::string& key, const std::string& value) {
        context[key] = value;
        return *this;
    }
    
    // Wrap another error as cause
    Error& with_cause(Error cause_error) {
        cause = std::make_shared<Error>(std::move(cause_error));
        return *this;
    }
    
    // Serialize to JSON string for logging
    std::string to_json() const;
    
    // Human-readable string
    std::string to_string() const;
};

} // namespace dmtpp::core

// Usage example;
/*
// Create error with context
Error err = Error::transient_network(
    "kafka_producer",
    "send_message",
    "Connection refused"
);
err.with_context("broker", "localhost:9092")
   .with_context("topic", "transaction_events")
   .with_context("partition", "3");

// Log as JSON
logger.error("Producer error", {{"error", err.to_json()}});

// Output:
// {
//   "timestamp": "2026-01-31T14:23:45.123Z",
//   "level": "ERROR",
//   "message": "Producer error",
//   "error": {
//     "code": 1000,
//     "code_name": "E_TRANSIENT_NETWORK",
//     "component": "kafka_producer",
//     "operation": "send_message",
//     "message": "Connection refused",
//     "category": "transient",
//     "retriable": true,
//     "fatal": false,
//     "context": {
//       "broker": "localhost:9092",
//       "topic": "transaction_events",
//       "partition": "3"
//     }
//   }
// }

// Or use human-readable format for console
std::cerr << err.to_string() << std::endl;
// Output: [E_TRANSIENT_NETWORK] kafka_producer::send_message: 
        Connection refused (broker=localhost:9092, topic=transaction_events, partition=3)

*/


// Wrapping example
/*
Result<void, Error> sendToKafka(const Event& event) {
    auto result = kafka_producer.send(event);
    if (result.is_err()) {
        // Wrap the low-level error with higher-level context
        Error wrapped = Error::transient_network(
            "event_ingress",
            "publish_event",
            "Failed to publish event to Kafka"
        );
        wrapped.with_context("event_id", event.id)
               .with_context("event_type", event.type)
               .with_cause(result.error());  // Attach original error
        
        return Result<void, Error>::Err(wrapped);
    }
    return Result<void, Error>::Ok();
}

// When this error is logged, you get the full chain:
// [E_TRANSIENT_NETWORK] event_ingress::publish_event: 
        Failed to publish event to Kafka (event_id=evt_123, event_type=PAYMENT_INITIATED)
//   caused by: [E_TRANSIENT_NETWORK] kafka_producer::send_message: 
            Connection refused (broker=localhost:9092, topic=transaction_events)
*/