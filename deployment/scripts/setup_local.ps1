# DM-TPP Local Development Setup

param(
    [switch]$Clean = $false
)

Write-Host "Starting DM-TPP Local Development Setup..."

# ----------------------------------------
# Verify Docker is running
# ----------------------------------------
try {
    docker ps -q 2>$null | Out-Null
    Write-Host "Docker is running."
}
catch {
    Write-Error "Docker does not appear to be running. Please start Docker and try again."
    exit 1
}

# ----------------------------------------
# Clean volumes if requested
# ----------------------------------------
if ($Clean) {
    Write-Host "Cleaning up existing Docker volumes..."
    docker compose -f deployment/docker-compose/docker-compose.local.yml down -v
}

# ----------------------------------------
# Start services
# ----------------------------------------
Write-Host "Starting PostgreSQL and Toxiproxy services..."
docker compose -f deployment/docker-compose/docker-compose.local.yml up -d

# ----------------------------------------
# Wait for PostgreSQL (connect as postgres)
# ----------------------------------------
Write-Host "Waiting for PostgreSQL to be ready..."

$env:PGPASSWORD = "postgres"
$retries = 0
$maxRetries = 15

while ($retries -lt $maxRetries) {
    psql -h localhost -U postgres -d postgres -c "SELECT 1" 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PostgreSQL is ready."
        break
    }

    Start-Sleep -Seconds 1
    $retries++
}

if ($retries -eq $maxRetries) {
    Write-Error "PostgreSQL did not become ready in time."
    docker compose -f deployment/docker-compose/docker-compose.local.yml logs postgres
    exit 1
}

# ----------------------------------------
# Ensure role + database exist (idempotent)
# ----------------------------------------
Write-Host "Ensuring database role and database exist..."

psql -h localhost -U postgres -d postgres -v ON_ERROR_STOP=1 -c @"
DO \$\$
BEGIN
   IF NOT EXISTS (
      SELECT FROM pg_roles WHERE rolname = 'dm_tpp_user'
   ) THEN
      CREATE ROLE dm_tpp_user WITH LOGIN PASSWORD 'postgres';
   END IF;
END
\$\$;

DO \$\$
BEGIN
   IF NOT EXISTS (
      SELECT FROM pg_database WHERE datname = 'dm_tpp'
   ) THEN
      CREATE DATABASE dm_tpp OWNER dm_tpp_user;
   END IF;
END
\$\$;
"@

# ----------------------------------------
# Verify application user connection
# ----------------------------------------
Write-Host "Verifying dm_tpp_user connection..."

$env:PGPASSWORD = "postgres"
psql -h localhost -U dm_tpp_user -d dm_tpp -c "SELECT 1" | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to connect as dm_tpp_user."
    exit 1
}

# ----------------------------------------
# Setup Toxiproxy proxy
# ----------------------------------------
Write-Host "Setting up Toxiproxy proxy..."

$retries = 0
$maxRetries = 10

while ($retries -lt $maxRetries) {
    try {
        $existingProxy = Invoke-RestMethod -Method Get -Uri "http://localhost:8474/proxies/postgres_proxy" -UserAgent "PowerShell" -ErrorAction SilentlyContinue
        Write-Host "Toxiproxy proxy already exists."
        break
    }
    catch {
        if ($_.Exception.Response.StatusCode -eq 404) {
            # Proxy doesn't exist, create it
            try {
                Invoke-RestMethod -Method Post -Uri "http://localhost:8474/proxies" -UserAgent "PowerShell" -ContentType "application/json" -Body (@{
                    name = "postgres_proxy"
                    listen = "0.0.0.0:5433"
                    upstream = "postgres:5432"
                    enabled = $true
                } | ConvertTo-Json) | Out-Null
                Write-Host "Toxiproxy proxy created successfully."
                break
            }
            catch {
                Write-Host "Waiting for Toxiproxy to be ready..."
                Start-Sleep -Seconds 1
                $retries++
            }
        }
        else {
            Write-Host "Waiting for Toxiproxy to be ready..."
            Start-Sleep -Seconds 1
            $retries++
        }
    }
}

if ($retries -eq $maxRetries) {
    Write-Error "Toxiproxy did not become ready in time."
    docker compose -f deployment/docker-compose/docker-compose.local.yml logs toxiproxy
    exit 1
}

# ----------------------------------------
# Success output
# ----------------------------------------
Write-Host ""
Write-Host "================================"
Write-Host "Environment setup complete."
Write-Host "================================"
Write-Host "PostgreSQL Connection Details:"
Write-Host "Host: localhost"
Write-Host "Port: 5432 (direct) | 5433 (via Toxiproxy)"
Write-Host "Database: dm_tpp"
Write-Host "User: dm_tpp_user"
Write-Host "Password: postgres"
Write-Host ""
Write-Host "Toxiproxy API: http://localhost:8474"
Write-Host "Toxiproxy Proxy: postgres_proxy (0.0.0.0:5433 -> postgres:5432)"
Write-Host ""
Write-Host "Next steps:"
Write-Host "- Run database migrations"
Write-Host "- Start application services"
