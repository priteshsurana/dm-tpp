# ADR-000: Why No Exactly-Once Claims at Broker Level

## Status
ACCEPTED

## Context
Kafka supports idempotent producers and transactional APIs. RabbitMQ supports publisher confirms.

## Decision
We do NOT rely on broker-level exactly-once guarantees. We implement exactly-once semantics at the application level via idempotency keys.

## Rationale

### Why Not Kafka Transactions?
1. **Complexity**: Kafka transactions require transactional consumers, producer fencing, and careful error handling.
2. **Brittleness**: Transaction coordinator failures can block producers.
3. **Opacity**: Hard to debug transaction state across brokers.
4. **Over-reliance**: Shifts responsibility to broker, hides application bugs.

### Why Application-Level Idempotency?
1. **Clarity**: Idempotency keys are explicit and visible in logs.
2. **Debuggability**: Easy to trace: "Did transaction X finalize?" → SELECT * FROM transactions WHERE key='X'
3. **Broker Independence**: Works with any message broker.
4. **Interview Value**: Forces deep understanding of distributed idempotency.

## Consequences

### Positive
- Simple mental model: "Brokers deliver at-least-once, application ensures at-most-once effect."
- Easy to explain in interviews.
- Works across Kafka, RabbitMQ, SQS, etc.

### Negative
- Must implement idempotency everywhere.
- Slightly higher latency (DB lookup per message).

## Alternatives Considered
1. Use Kafka transactions → Rejected (too magical, hard to debug).
2. Rely on "exactly-once" delivery → Rejected (doesn't exist in reality).

## Verification
- Chaos: disable offset commits, force redelivery → assert no duplicate effects.
- Interview question: "How do you handle duplicate Kafka messages?" → "Idempotency keys in Postgres."