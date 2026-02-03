# ADR-001: No exceptions across service boundaries

## Context
C++ Supports exceptions for error handling, but they have significant drawbacks in distributed systems:
- Hidden control flow (difficult to trace in logs)
- Performance overhead (stack unwinding)
- Unclear error propagation across library boundaries
- Difficult to make deterministic for replay

## Decision
**No exceptions shall cross service boundaries or public API boundaries.**

Internal implementation may use exceptions, but all public APIs must return `Result<T, Error>`.

## Rationale
### Why not exceptions?
**1. Hidden Control Flow**
```cpp
//Bad: Where can this throw?
Transaction process(const Event& event);

// Good: Explicit error handling
Result<Transaction, Error> process (const Event& event);
```

**2. Performance**
- Exception throwing involves stack unwinding (expensive)
- Measured: 100-1000x slower than returning error value
- For hot paths (message processing), this is unacceptable

**3. Determinism**
- Exception behavior can vary based on timing, memory pressure
- Makes replay non-deterministic
- Error codes + context are deterministic

**4. Cross-Language boundaries**
- Exceptions dont work across service with different stack (C, Go, Python)
- Result<T, Error> serializes to JSON cleanly
- Future:j microservices in different languages

### Why Result<T, Error>?

**1. Explicit**
```cpp
auto result = kafka_producer.send(event);
if (result.is_err()) {
    LOG_ERROR("Send failed", {{"error", result.error().to_json()}});
    return result.error();
}
// Must explicitly handle or propagate
```

**2. Composable**
```cpp
auto result = validate(event)
    .and_then([](Event e) {return persist(e); })
    .and_then([](Transaction t) { return dispatch(t); })
    .map([](Transaction t) {return t.id});

if (result.is_err()) {
    handle_error(result.error());
}
```

**3. Type-Safe**
- Compiler forces error handling
- Cannot forget to check
- Cannot accidentally ignore error

**4. Inspectable**
```cpp
Error err = result.error();
std::string json = err.to_json();
// Ship to logging system, serialize to disk, etc.
```

## Exceptions Allowed
### Internal Implementation  Only
```cpp
class PostgresClient {
    private:
    // Internal helper can throw
    void connect_internal() {
        if (!connection) {
            throw ConnectionError("Failed to connect");
        }
    }

    public:
    //Public API - no exceptions
    Result<Connection, Error> connect () {
        try {
            connect_internal();
            return Result::Ok(connection);

        }
        catch (const ConnectionError& e) {
            return Result::Err(Error(
                E_TRANSIENT_POSTGRES_CONNECTION, 
                "postgres_client",
                "connect",
                e.what()
            ));
        }
    }
};

```

### Test code
```cpp
TEST(KafkaProducer, SendMessage) {
    auto producer = KafkaProducer::create(config).unwrap(); // OK
    auto result = producer.send("key", "value");
    ASSERT_TRUE(result.is_ok());
}
```

## Consequences
### Positive
- Explicit error handling
- Deterministic behavior
- Easy to log and serialize errors
- Cross-language compatibilty
- Performance (no stack unwinding)

### Negative
- More verbose than exceptions
- Requires discipline (cant forget to check)
- Unfamiliar to developers from Java/C# backgrounds

## Implementation Guidelines
### Rule 1: All Service Entry Points Return Result
```cpp
//Ingress HTTP handler
Result<void, Error> handle_request(const Request& req);

//Kafka consumer
Result<void, Error> process_message(const Message& msg);
```


### Rule 2: Propagate Errors Up
```cpp
Result<void, Error> process() {
    auto validation = validate();
    if (validation.is_err()) {
        return Result::Err(validation.error());
    }

    auto persist = save_to_db ();
    if(persist.is_err()) {
        return Result::Err(persist.error());
    }

    return Result::Ok();
}
```

### Rule 3: Add context at each layer
```cpp
Result<void, Error> high_level_operation() {
    auto result = low_level_operation();
    if (result.is_err()) {
        Error wrapped = result.error();
        wrapped.with_context("transaction_id", txn_id)
        .with_context("operation", "high_level");
        return Result::Err(wrapped);
    }
    return Result::Ok();
}
```

### Rule 4: Log All Errors
```cpp
if (result.is_err()) {
    LOG_ERROR("Operation failed", {
        {"error", result.error().to_json()},
        {"retriable", result.error.is_retriable()}
    });

    if (result.error().is_retriable()) {
        retry_queue.push(operation);
    } else {
        dead_letter_queue.push(operation);
    }
}
```

## Migration Path
For existing code using exceptions:
1. Wrap in public API with Result
2. Gradually refactor internals
3. Keep exceptions only for unrecoverable errors (assertions)

```cpp
//Old code
Transaction process(const Event& e) {
    if(!validate(e)) {
        throw ValidationError("Invalid event");
    }
    return Transaction{};
}

// New code
Result<Transaction, Error> process(const Event& e) {
    auto validation = validate(e);
    if(validation.is_err()) [
        return Result::Err(validation.error());
    ]
    return Result::Ok(Transaction{});
}
``