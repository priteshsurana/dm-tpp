
```markdown
# Glossary

## Business Concepts

**Transaction**: A unit of work with idempotency key, state, and external effects.

**Idempotency Key**: Client-provided unique identifier ensuring at-most-once effect.

**Event**: Immutable fact stored in Kafka (e.g., "TransactionReceived").

**Command**: Request for action sent via RabbitMQ (e.g., "ExecuteTransaction").

**State Transition**: Change from state S1 to S2, persisted in Postgres.

---

## Technical Concepts

**Exactly-Once (Application-Level)**: Each idempotency key produces at most one finalized transaction.

**At-Least-Once Delivery**: Message may be delivered multiple times; consumer must be idempotent.

**Deterministic Replay**: Replaying same inputs produces identical outputs.

**Source of Truth**: PostgreSQL is the authoritative store for transaction state.

**History Log**: Kafka is the immutable, ordered log of events.

**Ephemeral Cache**: RocksDB stores rebuildable state; not source of truth.

---

## State Definitions

**RECEIVED**: Event ingested, idempotency key stored.

**VALIDATED**: Business rules passed, ready for persistence.

**PERSISTED**: Transaction written to Postgres.

**DISPATCHED**: Command sent to execution workers.

**EXECUTED**: Business logic completed.

**FINALIZED**: All effects applied, transaction complete.

**FAILED**: Terminal error, no retry.

---

## Error Codes

(See lib/core/include/error_codes.h for full list)

**E_TRANSIENT_NETWORK**: Retriable network failure.

**E_PERMANENT_INVALID_INPUT**: Non-retriable validation error.

**E_CORRUPTION_DETECTED**: Data integrity violation.

---

## Infrastructure Terms

**Rebalance**: Kafka consumer group reassigning partitions among consumers.

**Dead Letter Queue (DLQ)**: Queue for messages that cannot be processed.

**Backpressure**: System rejecting new requests to prevent overload.

**StatefulSet**: Kubernetes workload with stable network identity and persistent storage.

**Consumer Group**: Set of Kafka consumers sharing partition consumption load. Rebalance occurs when members join/leave.

**Partition**: Ordered, immutable sequence of records in Kafka. Unit of parallelism for consumers.

**Offset**: Position in Kafka partition. Each consumer tracks offset per partition.

**Replication Factor**: Number of copies of each Kafka partition. Ensures durability.

**In-Sync Replica (ISR)**: Kafka replica that is caught up with leader. Required for acks=all.

**Quorum Queue**: RabbitMQ queue replicated across nodes for high availability.

**Prefetch**: Number of unacknowledged messages RabbitMQ delivers to consumer. Controls concurrency.

**Write-Ahead Log (WAL)**: Append-only log of changes before applying to main data structure. Used by Postgres.

**LSN (Log Sequence Number)**: Position in Postgres WAL. Used for replication tracking.

**Snapshot Isolation**: Transaction sees consistent snapshot of DB at start time. Used by REPEATABLE READ.

**Serialization Failure**: Error when SERIALIZABLE isolation detects conflict between concurrent transactions.

**Advisory Lock**: Application-controlled lock in Postgres. Must be explicitly released.

**Connection Pool**: Pre-established database connections reused by multiple threads. Reduces connection overhead.

**Circuit Breaker**: Pattern that stops calling failing service after threshold. Prevents cascade failures.

**Bulkhead**: Isolation pattern that limits resources for each component. One failure can't exhaust all resources.

**Saga**: Long-running transaction pattern using compensating actions. Alternative to distributed 2PC.

**Outbox Pattern**: Write to DB and message queue atomically by storing message in DB table, then publishing asynchronously.

**CDC (Change Data Capture)**: Stream of database changes (inserts, updates, deletes). Used for event sourcing.
```