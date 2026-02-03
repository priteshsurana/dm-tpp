# C++ Error Handling Guide

## Principles

### P1: No Exceptions Across Service Boundaries
**Rationale**: Exceptions hide control flow, complicate retry logic, and break determinism.

**Rule**: Internal libraries may use exceptions. Service entrypoints must catch all exceptions and convert to Result<T>.

---

### P2: Result<T, Error> for Fallible Operations
**Signature**:
```cpp
Result processTransaction(const Event& event);
```

**Incorrect**:
```cpp
Transaction processTransaction(const Event& event); // May throw? May return invalid object?
```

---

### P3: Errors Must Be Actionable
Every error must answer:
- What failed? (component + operation)
- Why did it fail? (error code)
- Is it retriable? (transient vs permanent)
- What context is relevant? (txn_id, partition, offset)

---

## Error Taxonomy

### Category 1: Transient Errors
- Network timeout
- Connection pool exhausted
- Kafka broker unavailable
- Postgres deadlock

**Action**: Retry with exponential backoff.

**Error Codes**:
- E_TRANSIENT_NETWORK = 1000
- E_TRANSIENT_RESOURCE_EXHAUSTED = 1001
- E_TRANSIENT_TIMEOUT = 1002

---

### Category 2: Permanent Errors
- Invalid schema
- Idempotency key collision (duplicate)
- Authorization failure
- Data corruption detected

**Action**: Log, send to DLQ, alert.

**Error Codes**:
- E_PERMANENT_INVALID_INPUT = 2000
- E_PERMANENT_DUPLICATE = 2001
- E_PERMANENT_CORRUPTION = 2002

---

### Category 3: Fatal Errors
- Assertion failure
- Invariant violation
- Memory corruption

**Action**: Crash process, trigger restart.

**Error Codes**:
- E_FATAL_INVARIANT_VIOLATED = 9000
- E_FATAL_ASSERTION = 9001

---

## Implementation

### Result<T, Error> Type

**File**: `lib/core/include/result.h`
```cpp
// Outline, not full implementation
template
class Result {
public:
    // Construct successful result
    static Result Ok(T value);
    
    // Construct error result
    static Result Err(E error);
    
    // Check if successful
    bool is_ok() const;
    bool is_err() const;
    
    // Unwrap value (aborts if error)
    T unwrap();
    
    // Unwrap or return default
    T unwrap_or(T default_value);
    
    // Map transformations
    template
    Result map(std::function func);
    
    template
    Result and_then(std::function<Result(T)> func);
    
    // Get error (aborts if ok)
    E error();
    
private:
    std::variant data_;
};
```

---

### Error Structure

**File**: `lib/core/include/error.h`
```cpp
struct Error {
    // Error code from taxonomy
    int code;
    
    // Component that produced error (e.g., "kafka_producer")
    std::string component;
    
    // Operation that failed (e.g., "send_message")
    std::string operation;
    
    // Human-readable message
    std::string message;
    
    // Context map (e.g., {{"txn_id", "12345"}, {"partition", "3"}})
    std::map context;
    
    // Causal error (if this error wraps another)
    std::optional<std::shared_ptr> cause;
    
    // Is this error retriable?
    bool is_retriable() const;
    
    // Is this error fatal?
    bool is_fatal() const;
    
    // Serialize to JSON for logging
    std::string to_json() const;
};
```


---

### Error Propagation Example
```cpp
Result ingestEvent(const Event& event) {
    // Validate schema
    auto validation_result = validator.validate(event);
    if (validation_result.is_err()) {
        return Result::Err(validation_result.error());
    }
    
    // Check idempotency
    auto idempotency_result = checkIdempotency(event.idempotency_key);
    if (idempotency_result.is_err()) {
        return Result::Err(idempotency_result.error());
    }
    
    // Produce to Kafka
    auto produce_result = kafka_producer.send(event);
    if (produce_result.is_err()) {
        // Wrap error with context
        Error wrapped = produce_result.error();
        wrapped.context["event_id"] = event.id;
        wrapped.cause = std::make_shared(produce_result.error());
        return Result::Err(wrapped);
    }
    
    return Result::Ok();
}
```

---

## Logging Discipline

### Rule 1: Log at State Transitions, Not in Loops
**Bad**:
```cpp
for (auto& msg : messages) {
    LOG_INFO("Processing message", {{"id", msg.id}});  // Noise
    process(msg);
}
```

**Good**:
```cpp
LOG_INFO("Batch processing started", {{"count", messages.size()}});
for (auto& msg : messages) {
    process(msg);
}
LOG_INFO("Batch processing completed", {{"count", messages.size()}});
```

---

### Rule 2: Structured Logs Only
**Bad**:
```cpp
LOG_INFO("Transaction " + txn_id + " moved to PERSISTED");
```

**Good**:
```cpp
LOG_INFO("Transaction state changed", {
    {"txn_id", txn_id},
    {"from_state", "VALIDATED"},
    {"to_state", "PERSISTED"},
    {"duration_ms", duration}
});
```

---

### Rule 3: Error Logs Include Full Context
**Example**:
```cpp
if (result.is_err()) {
    LOG_ERROR("Transaction processing failed", {
        {"txn_id", txn_id},
        {"error_code", result.error().code},
        {"component", result.error().component},
        {"operation", result.error().operation},
        {"retriable", result.error().is_retriable()},
        {"error_json", result.error().to_json()}
    });
}
```

---
## Persistence Discipline

### Rule 1: No In-Memory State That Can't Be Rebuilt
**Bad**: Storing "transaction processing started" flag in memory only.

**Good**: Every state transition persisted in Postgres.

---

### Rule 2: Every Mutation Must Be Explainable Post-Crash
If the process crashes at any point, we must be able to:
1. Query Postgres to see what state was reached
2. Replay Kafka to see what inputs were processed
3. Reconcile any discrepancy

---

### Rule 3: Write-Ahead Logging Mental Model
Think of Postgres as a write-ahead log:
- Write intent to log (BEGIN transaction)
- Apply state change
- Commit log (COMMIT)
- External effect only after COMMIT visible

---

## ADR-001 Template
### Todo
(See file structure above, similar to ADR-000)