# PostgreSQL Isolation Experiment Results

## Experiment 1: Idempotent Upsert

### Results
- First INSERT: Returns inserted row (1 tuple)
- Second INSERT: Returns nothing (0 tuples)
- Final row count: 1

### Conclusion
ON CONFLICT DO NOTHING is idempotent and atomic
---
## Experiment 2: Concurrent Writers
### Results
- Successful INSERTs: 1
- Failed INSERTs: 19
- Error code: 23505 (unique_violation)
- Final DB row count:1

### Conclusion
Postgres unique constraint is atomic. No race condition detected.

---

## Experiment 3: Crash Recovery
### Results
- Row visible before kill: No (not commited)
- Row visible after restart: No (rollback back)

### Conclusion
Postgres guarantees rollback on connection loss.

---
## Experiment 4: Isolation Levels
### READ COMMITTED
- Transaction A sees count increase mid-transaction
- First SELECT: count = 5
- After concurrent INSERT: count=5

### REPEATABLE READ
- Transaction A sees consistent snapshot
- First SELECT: count = 5
- After concurrent INSERT: count = 5

### Seriablizable 
- Same as Repeatable read under low contention

### Conclusion
- For DM-TPP: Use Read committed for most transactions

---
## Experiment 5:  Advisory Locks vs Row Locks
### Row locks
- Automatic with SELECT ... FOR UPDATE
- Released on COMMIT/ROLLBACK
- Worker B blocked for ~2500ms

### Conclusion
Use row locks for data access.

