# Failure Model

## Document Metadata
- Author: [Pritesh Surana]
- Date: [2026-01-22]
- Status: DRAFT

## Purpose
Enumerate every failure scenario the system must handle or detect.

## Failure Taxonomy

### Category A: Process Failures

#### F-001: Ingress Crash After Kafka Produce, Before HTTP Response
**Scenario**:
1. Ingress receives POST /transaction
2. Writes to Kafka successfully
3. Process killed (SIGKILL)
4. Client receives connection reset

**Expected Behavior**:
- Kafka consumer processes event
- Idempotency key prevents duplicate finalization
- Client retries with same key, receives 200 + existing result

**Detection**:
- Monitor HTTP 5xx rate
- Check Kafka lag vs ingress throughput

**Verification**:
- Chaos: kill ingress with `kill -9` during load test
- Assert: no duplicate finalized transactions

---

#### F-002: Consumer Crash Mid-Batch
**Scenario**:
1. Consumer fetches 100 messages
2. Processes 50 messages, commits offsets for 0-49
3. Crashes before processing 50-99
4. Rebalance assigns partition to different consumer
5. New consumer starts from offset 50

**Expected Behavior**:
- Messages 50-99 reprocessed
- Idempotency prevents duplicate effects
- All 100 messages eventually finalized

**Verification**:
- Inject crash after N messages
- Check for duplicate transaction IDs in Postgres

---

### Category B: Network Failures

#### F-010: Kafka Produce Timeout
**Scenario**:
- Ingress calls `producer.send()`, network partitioned, timeout
- Client sees 500 error
- Unknown if Kafka received message

**Expected Behavior**:
- Ingress returns 500 to client
- Client retries with same idempotency key
- System detects duplicate, returns original result

---

#### F-011: Postgres Connection Loss During Transaction
**Scenario**:
- Worker begins transaction
- Network partition after INSERT, before COMMIT
- Connection timeout

**Expected Behavior**:
- Postgres rolls back transaction (isolation guarantee)
- Worker retries transaction
- Idempotency key prevents duplicate application

---

### Category C: Message Delivery Failures

#### F-020: Duplicate Kafka Delivery
**Scenario**:
- Consumer processes message M
- Commits offset
- Offset commit fails (network)
- On restart, message M redelivered

**Expected Behavior**:
- Second processing detected via idempotency key
- No duplicate effect
- Offset eventually committed

**Verification**:
- Disable offset commit
- Force reprocessing
- Assert single finalized transaction

---

#### F-021: Out-of-Order Delivery (Cross-Partition)
**Scenario**:
- Message M1 sent to partition P1 at T1
- Message M2 sent to partition P2 at T2
- M2 processed before M1 (different consumers)

**Expected Behavior**:
- System does NOT guarantee global order
- Each partition ordered independently
- Business logic must not assume global order

---

### Category D: Data Corruption

#### F-030: Partial Postgres Commit Visibility
**Scenario**:
- Transaction commits on leader
- Replica lag causes read-your-writes failure

**Expected Behavior**:
- Use synchronous_commit = on
- Reads go to leader or use repeatable read

---

#### F-031: RocksDB Data Loss
**Scenario**:
- Node crashes, RocksDB data corrupted

**Expected Behavior**:
- Delete RocksDB directory
- Rebuild from Postgres + Kafka
- Verify checksum matches previous state

---

### Category E: Clock Skew

#### F-040: NTP Drift Across Nodes
**Scenario**:
- Node A clock: 10:00:00
- Node B clock: 10:00:30 (30s drift)

**Expected Behavior**:
- Do not use wall-clock for business logic
- Use event-provided timestamp or Postgres sequence

---

### Category F: Resource Exhaustion

#### F-050: Kafka Disk Full
**Scenario**:
- Kafka broker disk reaches 100%
- Produce requests fail

**Expected Behavior**:
- Ingress returns 503 (backpressure)
- Client retries with exponential backoff

---

#### F-051: Postgres Connection Pool Exhaustion
**Scenario**:
- All 100 connections in use
- New request blocked

**Expected Behavior**:
- Request waits or fails fast with timeout
- Monitor connection pool utilization

---

## Failure Matrix

| Failure ID | Component | Detection | Recovery | SLA Impact |
|------------|-----------|-----------|----------|------------|
| F-001 | Ingress | Client retry | Idempotency | None |
| F-002 | Consumer | Offset lag | Reprocess | Latency +N |
| F-010 | Kafka | Timeout | Client retry | None |
| ... | ... | ... | ... | ... |

## Deliberate Non-Handling

### NH-001: Byzantine Failures
We assume components fail by crashing, not by sending malicious data.

### NH-002: Hardware Bit Flips
We assume ECC memory and disk checksums prevent silent corruption.

### NH-003: Kafka Cluster Complete Loss
We assume Kafka cluster is replicated (replication.factor=3).

---

## Chaos Experiment Mapping

Each failure MUST have a corresponding chaos experiment:

- F-001 → chaos/scenarios/ingress_kill.py
- F-002 → chaos/scenarios/consumer_crash.py
- F-010 → chaos/scenarios/kafka_partition.py
...