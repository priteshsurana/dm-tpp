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