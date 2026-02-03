#pragma once

namespace dmtpp ::core {

// Error code ranges:
// 1000-1999: Transient errors (retriable)
// 2000-2999: Permanent errors (not retriable)
// 9000-9999: Fatal errors (crash process)

enum ErrorCode {
    // Transient errors
    E_TRANSIENT_NETWORK = 1000,
    E_TRANSIENT_TIMEOUT = 1001,
    E_TRANSIENT_RESOURCE_EXHAUSTED = 1002,
    E_TRANSIENT_KAFKA_UNAVAILABLE = 1003,
    E_TRANSIENT_POSTGRES_DEADLOCK = 1004,
    E_TRANSIENT_POSTGRES_CONNECTION = 1005,
    E_TRANSIENT_POSTGRES_DOWN = 1006,
    E_TRANSIENT_RABBITMQ_UNAVAILABLE = 1007,
    
    // Permanent errors
    E_PERMANENT_INVALID_INPUT = 2000,
    E_PERMANENT_SCHEMA_VALIDATION = 2001,
    E_PERMANENT_DUPLICATE_KEY = 2002,
    E_PERMANENT_NOT_FOUND = 2003,
    E_PERMANENT_AUTHORIZATION = 2004,
    E_PERMANENT_CORRUPTION = 2005,
    E_PERMANENT_INVARIANT_VIOLATED = 2006,
    E_CORRUPTON_DETECTED = 2007,
    
    // Fatal errors
    E_FATAL_ASSERTION = 9000,
    E_FATAL_INVARIANT_VIOLATED = 9001,
    E_FATAL_MEMORY_CORRUPTION = 9002,
    E_FATAL_UNRECOVERABLE = 9003,
};

// Helper: is error code retriable?
inline bool is_retriable(ErrorCode code) {
    return code >= 1000 && code < 2000;
}

// Helper: is error code fatal?
inline bool is_fatal(ErrorCode code) {
    return code >= 9000;
}

// Helper: get error category string
inline const char* error_category(ErrorCode code) {
    if (code >= 1000 && code < 2000) return "TRANSIENT";
    if (code >= 2000 && code < 3000) return "PERMANENT";
    if (code >= 9000) return "FATAL";
    return "UNKNOWN";
}

} // namespace dmtpp::core
