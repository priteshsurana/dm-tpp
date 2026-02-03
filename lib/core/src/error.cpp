#include "error.h"
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;
namespace dmtpp ::core {
    const char* error_code_to_string(ErrorCode code) {
        switch (code) {
        case E_TRANSIENT_NETWORK: return "E_TRANSIENT_NETWORK";
        case E_TRANSIENT_TIMEOUT: return "E_TRANSIENT_TIMEOUT";
        case E_TRANSIENT_RESOURCE_EXHAUSTED: return "E_TRANSIENT_RESOURCE_EXHAUSTED";
        case E_TRANSIENT_KAFKA_UNAVAILABLE: return "E_TRANSIENT_KAFKA_UNAVAILABLE";
        case E_TRANSIENT_POSTGRES_DEADLOCK: return "E_TRANSIENT_POSTGRES_DEADLOCK";
        case E_TRANSIENT_POSTGRES_CONNECTION: return "E_TRANSIENT_POSTGRES_CONNECTION";
        case E_TRANSIENT_RABBITMQ_UNAVAILABLE: return "E_TRANSIENT_RABBITMQ_UNAVAILABLE";
        case E_PERMANENT_INVALID_INPUT: return "E_PERMANENT_INVALID_INPUT";
        case E_PERMANENT_SCHEMA_VALIDATION: return "E_PERMANENT_SCHEMA_VALIDATION";
        case E_PERMANENT_DUPLICATE_KEY: return "E_PERMANENT_DUPLICATE_KEY";
        case E_PERMANENT_NOT_FOUND: return "E_PERMANENT_NOT_FOUND";
        case E_PERMANENT_AUTHORIZATION: return "E_PERMANENT_AUTHORIZATION";
        case E_PERMANENT_CORRUPTION: return "E_PERMANENT_CORRUPTION";
        case E_PERMANENT_INVARIANT_VIOLATED: return "E_PERMANENT_INVARIANT_VIOLATED";
        case E_FATAL_ASSERTION: return "E_FATAL_ASSERTION";
        case E_FATAL_INVARIANT_VIOLATED: return "E_FATAL_INVARIANT_VIOLATED";
        case E_FATAL_MEMORY_CORRUPTION: return "E_FATAL_MEMORY_CORRUPTION";
        case E_FATAL_UNRECOVERABLE: return "E_FATAL_UNRECOVERABLE";
        default: return "UNKNOWN_ERROR_CODE";
        }
    }

    // Serialize error code to JSON
    std::string Error::to_json() const {
        json j;

        //Core fields
        j["code"] = static_cast<uint32_t>(code);
        j["code_name"] = error_code_to_string(code);
        j["component"] = component;
        j["operation"] = operation;
        j["message"] = message;
        j["category"] = category();
        j["retriable"] = is_retriable();
        j["fatal"] = is_fatal();

        // Context (if any)
        if (!context.empty()) {
            j["context"] = context;
        }

        // Cause chain (if any)
        if (cause.has_value() && cause.value() != nullptr) {
            j["cause"] = json::parse(cause.value()->to_json());
        }
        return j.dump();    
    }

    // Human-readable string representation
    std::string Error::to_string() const {
        std::ostringstream oss;
        oss << "Error[" << error_code_to_string(code) << " (" << static_cast<uint32_t>(code) << ")] "
            << "in component '" << component << "' during operation '" << operation << "': "
            << message;

        // Context (if any)
        if (!context.empty()) {
            oss << " | Context: {";
            for (const auto& [key, value] : context) {
                oss << key << ": " << value << ", ";
            }
            oss.seekp(-2, oss.cur); // Remove trailing comma
            oss << "}";
        }

        // Cause chain (if any)
        if (cause.has_value() && cause.value() != nullptr) {
            oss << " | Caused by: " << cause.value()->to_string();
        }
        return oss.str();
    }

    // Factory method: transient network error
    Error make_transient_network_error(const std::string& component,
                                       const std::string& operation,
                                       const std::string& message) {
        return Error(E_TRANSIENT_NETWORK, component, operation, message);
    }

    // Factory method: permanent invalid input error
    Error make_permanent_invalid_input_error(const std::string& component,
                                             const std::string& operation,
                                             const std::string& message) {
        return Error(E_PERMANENT_INVALID_INPUT, component, operation, message);
    }   

    // Factory method: corruption detected error
    Error make_corruption_detected_error(const std::string& component,
                                            const std::string& operation,
                                            const std::string& message) {
            return Error(E_PERMANENT_CORRUPTION, component, operation, message);
    }

    // Factory method: fatal invariant violated error
    Error make_fatal_invariant_violated_error(const std::string& component,
                                              const std::string& operation,
                                              const std::string& message) {
        return Error(E_FATAL_INVARIANT_VIOLATED, component, operation, message);
    }

    


}