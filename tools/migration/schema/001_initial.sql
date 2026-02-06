-- DM-TPP Initial Schema
--Drop existing tables (for local dev)
DROP TABLE IF EXISTS txn_state_log CASCADE;
DROP TABLE IF EXISTS transactions CASCADE;
DROP TABLE IF EXISTS txn_state CASCADE;

-- Transaction state enum

CREATE TYPE txn_state AS ENUM (
    'RECEIVED',
    'VALIDATED',
    'PERSISTED',
    'DISPATCHED',
    'EXECUTED',
    'FINALIZED',
    'FAILED'
);

-- Transactions table
CREATE TABLE transactions (
    id SERIAL PRIMARY KEY,
    idempotency_key VARCHAR(255) UNIQUE NOT NULL,
    state txn_state NOT NULL DEFAULT 'RECEIVED',
    payload JSONB,
    error_details JSONB,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

-- Indexes for common queries
CREATE INDEX idx_transactions_state ON transactions(state);
CREATE INDEX idx_transactions_created_at ON transactions(created_at);
CREATE INDEX idx_transactions_idempotency_key ON transactions(idempotency_key);

-- State transition audit log
CREATE TABLE txn_state_log (
    id SERIAL PRIMARY KEY,
    txn_id INTEGER NOT NULL REFERENCES transactions(id) ON DELETE CASCADE,
    from_state txn_state,
    to_state txn_state NOT NULL,
    transitioned_at TIMESTAMP NOT NULL DEFAULT NOW(),
    node_id VARCHAR(64),
    metadata JSONB
);

CREATE INDEX idx_txn_state_log_txn_id ON txn_state_log(txn_id);
CREATE INDEX idx_txn_state_log_transitioned_at ON txn_state_log(transitioned_at);

-- Updated at trigger
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
   NEW.updated_at = NOW();
   RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_transactions_updated_at
BEFORE UPDATE ON transactions
FOR EACH ROW
EXECUTE FUNCTION update_updated_at_column();

--Sample data for experimentation
INSERT INTO transactions (idempotency_key, state, payload) VALUES
('key1', 'RECEIVED', '{"amount": 100}'),
('key2', 'VALIDATED', '{"amount": 200}'),
('key3', 'PERSISTED', '{"amount": 300}');

COMMENT ON TABLE transactions IS 'Source of truth for transaction state';
COMMENT ON TABLE txn_state_log IS 'Audit log for state transitions';
COMMENT ON COLUMN transactions.idempotency_key IS 'Client-provided unique identifier for idempotent processing';

