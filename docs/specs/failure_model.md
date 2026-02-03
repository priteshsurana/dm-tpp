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

#### F-061: RabbitMq queue buildup
**Scenario**:
- Workers slower than producers
- RabbitMq queue depth > threshold
- Memory exhaustion on RabbitMq mode

**Expected Behavior**:
- RabbitMq flow control kicks in (blocks publishers)
- Alert on queue depth > threshold
- Scale workers horizontally

---

#### F-062: Postgres Replication Lag
**Scenario**:
- Postgres replica lag> 30 seconds
- Read queries on replica see stale data

**Expected Behavior**:
- Only write to primary
- Read from primary for critical queries
- Use application-level cache (RocksDB) for read-heavy operations
---

### Category H: Partial Failures

#### F-070: Split Brain (Network Partition)
**Scenaio**:
- Network parition between services
- Service A thinks Service B is down
- Service B is running but unreachable

**Expected Behavior**:
- Services use Postgres as source of truth
- No "both sides commit" scenario (Postgres provides consensus)
- Partition heals, services reconcile via Postgres state

**Verification**:
- Use chaos tool to partition network between ingress and consumer
- Verify both continue operating independently
- Verify state converges after partition heals

--- 

#### F-071: Partial Postgres Commit Visibility
**Scenario**:
- Transaction commits on primary
- Replica lag causes read to see old data
- Application reads from replica, doesn't see own write

**Expected Behavior**:
- Use `synchronous_commit=on` for critical writes
- Read from primary after write
- Use causal consistency token (eg. read from replica only if replica_lsn >=write_lsn)
---

#### F-072: Kafka Leader Election
**Scenario**:
- Kafka partition leader fails
- New leader elected
- Brief unavailability (5-30 seconds)

**Expected Behavior**:
- Producer retries automatically (librdkafka handles this)
- Consumer reconnects to new leader
- No message loss (acks=all ensures replication)
---

#### F-073: RabbitMq Node Failure (Cluster)
**Scenario**:
- One RabbitMq node in cluster fails
- Queues on that node unavailable

**Expected Behavior**:
- Quorum queues: replicated to other nodes, available
- Classic queues: unavailable untill node recovers
- Use quorum queues for critical paths
---

### Category I: Data Quality & Validation
#### F-080: Schema Evolution (Breaking Change)
**Scenario**:
- Event schema changes (eg. required field added)
- Old consumers can't parse new events

**Expected Behavior**:
- Version events (eg. `event_version: 2`)
- Consumers handle multiple versions
- Gradual rollout: consumers first, then producers
--- 

#### F-081: Invalid UTF-8 in Message
**Scenaio**:
- Client sends invalid UTF-8 in payload
- JSON parsing fails

**Expected Behavior**;
- Ingress validates encoding, returns 400
- If passes ingress, consumer handles gracefully (DLQ)
--- 

#### F-082: Message Size Exeeds Limit
**Scenario**;
- Client sends 10 MB payload
- Kafka message limit is 1MB

**Expected Behavior**:
- Ingress rehects with 413 (Payload Too Large)
- If using chunking, reassemble in consumer
---

#### F-083: Circular Dependency in Events
**Scenario**:
- Event A triggers Event B
- Event B triggers Event A
- Infinite loop

**Expected Behavior**:
- Detect cycles via event ancestry tracking
- Circuit breaker: max depth of event chains
- Alert on suspiciously deep event chains
---

### Category J: Operational Failures
#### F-090: Deployment Mid-Transaction
**Scenaio**:
- Rolling deployment kills pods
- Transactions in-flight interrupted

**Expected Behavior**:
- Graceful shutdown: Finish in-flight transactions
- SIGTERM handler : stop accepting new work, drain queue
- Kubernetes: preStop hook+ graceful termination period

**Verification**:
- Send 100 requests
- During processing, trigger rolling update
- Verifying all 100 eventually finalized (no lost transactions)
---

#### F-091: Database Migration Failure
**Scenario**:
- Migration script fails mid-execution
- Schema partially applied

**Expected Behavior**:
- Migrations are transactional (BEGIN/COMMIT)
- Rollback on failure
- Test migrations on staging DB first
---

#### F-092: Log Volume Overwhelms Storage
**Scenario**:
- Disk fills with logs
- Services cant write logs or data

**Expected Behavior**:
- Log rotation configured (size+time based)
- Alert on disk > 800% full
- Graceful degradation: reduce log level if disk critical
---

#### F-093: Monitoring System Down
**Scenario**:
- Prometheus/ Grafana unavailable
- No visibility into system health

**Expected Behavior**:
- System continues operating (monitoring is observability, not control plane)
- Secondary monitoring (external uptime check)
- Alert via sepatate channel (pagerduty, email)

---

#### F-094: Certificate Expiry
**Scenario**:
- TLS certifcate expires
- All HTTPS traffic fails

**Expected Behavior**:
- Alert 30 days before expiry
- Automated renewal (Lets Encrypt cert-manager)
- Graceful handling: service continues on HTTP if HTTPS fails (with warning)
---

#### F-095: DNS Resolution Failure
**Scenario**:
- DNS server down
- Services cant resolve hostnames

**Expected Behavior**:
- Use IP addresses as failback
- Cache DNS results (TTL)
- Multiples DNS servers configured

---


## Failure Matrix

| Failure ID | Component | Detection | Recovery | SLA Impact |
|------------|-----------|-----------|----------|------------|
| F-001 | Ingress | Client retry | Idempotency | None |
| F-002 | Consumer | Offset lag | Reprocess | Latency +N |
| F-010 | Kafka | Timeout | Client retry | None |
| F-011 | Postgres | Timeout | Retry | Latency +N |
| F-020 | Kafka | Duplicate check | Skip | None |
| F-021 | Kafka | N/A | Accept | None |
| F-030 | Postgres | Retry | Read leader | Latency +N |
| F-031 | RocksDB | Checksum fail | Rebuild | Downtime |
| F-040 | Clock | Timestamp check | Use event time | None |
| F-050 | Kafka | Produce error | Backpressure | Latency +N |
| F-051 | Postgres | Pool metric | Backpressure | Latency +N |
| F-060 | Kafka | Lag monitor | Scale consumers | Latency +N |
| F-061 | RabbitMQ | Queue depth | Scale workers | Latency +N |
| F-062 | Postgres | Replication lag | Read primary | Latency +N |
| F-070 | Network | Health check | Wait for heal | Partition |
| F-071 | Postgres | Read after write | Sync commit | None |
| F-072 | Kafka | Auto-retry | Wait election | Latency +N |
| F-073 | RabbitMQ | Quorum queue | Failover | None |
| F-080 | Schema | Version check | Multi-version | None |
| F-081 | Validation | Ingress check | Return 400 | None |
| F-082 | Size | Ingress check | Return 413 | None |
| F-083 | Logic | Depth tracking | Circuit break | Alert |
| F-090 | Deploy | Graceful shutdown | Drain queue | None |
| F-091 | Migration | Transaction | Rollback | Downtime |
| F-092 | Storage | Disk monitor | Log rotation | Degraded |
| F-093 | Monitoring | External check | Secondary alert | None |
| F-094 | TLS | Expiry alert | Auto-renew | Brief outage |
| F-095 | DNS | Cache + fallback | Use IP | None |

**Total Failures Enumerated**: 28 distinct scenarios across 10 categories


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