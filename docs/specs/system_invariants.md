# System Invariants
## Document Metadata
- Author: [Pritesh Surana]
- Date: [2026-01-22]
- Status: DRAFT
- Reviewers: N/A

## Purpose
This document defines the invariants that must hold true all the times in DM-TPP. If any invariant is violated, the system is an undefined state.

## Core Invariants
### INV-001: Transaction State Never Regresses
**Statement** Once a transaction reaches state S, it may only trasition to states >= S in the defined partial order.

**Partial Order**:
```
RECEIVED < VALIDATED < PERSISTED < DISPATCHED < EXECUTED < FINALIZED < FAILED(terminal)
```

**Verification**:
- Every state transition must be logged with (txn_id, from_state, to_state, timestamp, node_id)
- Automated checker scans Postgres txn_state_log table for violations
- Relay must produce identical state sequence

**Consequences if violated**:
- External observers may see non-monotonic progress
- Idempotency guarantees break
- Replay produces different history

---


### INV-002: External Effects Tied to Durable State
**Statement**: No externally visible effect (HTTP response, message publisher) occurs unless the corresponding state is persisted in Postgres.

**Implementation**;
- HTTP 200 only after Postgres COMMIT
- Kafka produce happens inside DB transaction boundary (two-phase semantics)
- If DB write fails, no external signal is sent

**Verification**:
- Chaos: kill process after the external effect, before DB commit -> must detect inconsistent 
- Check: for every external effect E, DB record R such that timestamp(R) <= timestamp(E)

---

### INV-003: Kafka offsets are not business state
**Statement**: Kafka consumer offsets are tracking metadata, not source of truth for complete transaction.

**Rationale**:
- Offset commit can fail independently of business logic
- Offsets can be reset for replay
- Business state must survive offset loss

**Implementation**:
- Transaction completion stored in 'transaction' table, not offset metadata
- Offset commit is "best effort" optimization, not correctness requirement
---

### INV-004: Replay Determinism
**Statement**: Given identical Kafka log+Postgres snapshot, replaying produces identical final Postgress state
**Requirements**:
- No wall-clock time in business logic (use event timestamp)
- No random UUIDs (use deterministic ID generation)
- No iteration over unordered maps
- No reliance on thread scheduling

**Verification**;
- Capture Postgres snapshot at T0
- Replay Kafka from T0 to T1
- Compute diff(Postgres_T1_original, Postgres_T1_replay)-> must be empty

---

### INV-005: Idempotency Key Uniqueness
**Statement**: Each idempotency key maps to at most one finalized transaction.

**Implementation**:
- Unique constraint on `transactions.idempotency_key`
- Insert with ON CONFLICT DO NOTHING
- Return existing result if key collision detected

---

### INV-006: Cross-Store Consistency (Eventually)
**Statement**: RocksDB state must be rebuildable from Postgres + Kafka within bounded time.

**Requirements**:
- RocksDB data can be deleted without business logic failure
- Rebuild procedure documented and tested weekly
- Checksum validation after rebuild

---

### INV-007: No Silent Failures
**Statement**: Every error is logged with full context. No error is swallowed.

**Implementation**:
- Result<T, Error> forces error handling
- Every error carries: component, operation, code, context
- Errors propagate to structured logs

---
Todo:
---

## Anti-Invariants (Explicitly NOT Guaranteed)

### Non-Guarantee 1: Message Delivery Exactly Once
We do NOT guarantee Kafka/RabbitMQ delivers each message exactly once.
We DO guarantee each message produces at most one finalized transaction (application-level idempotency).

### Non-Guarantee 2: Zero Latency
We do NOT guarantee real-time processing. We guarantee correct processing.

### Non-Guarantee 3: Global Ordering
We do NOT guarantee global message order. We guarantee per-key ordering in Kafka.

---

## Verification Checklist

- [ ] Every invariant has a violation detection mechanism
- [ ] Every invariant has a chaos experiment that tries to break it
- [ ] Every invariant is tested in replay scenario
- [ ] Every invariant appears in monitoring dashboard

---

## Open Questions
- Should FAILED state allow retry to RECEIVED? (Decision: No, create new txn)
- What happens if Postgres is partitioned during state transition? (See failure_model.md)