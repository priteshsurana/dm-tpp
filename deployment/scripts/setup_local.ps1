# DM-TPP Local Development Setup

param(
    [switch]$Clean = $false

)

Write-Host "Starting DM-TPP Local Development Setup..."

# Check Docker is running
try{
    docker ps -q 2>$null | Out-Null
    Write-Host "Docker is running."

}
catch {
    Write-Error "Docker does not appear to be running. Please start Docker and try again."
    exit 1
}

# Clean volumes if requested
if ($Clean) {
    Write-Host "Cleaning up existing Docker volumes..."
    docker-compose -f deployment/docker-compose/docker-compose.local.yml down -v
}

# Start services
Write-Host "Starting PostgreSQL and Toxi proxy services..."
docker-compose -f deployment/docker-compose/docker-compose.local.yml up -d

# wait for PostgreSQL to be ready
Write-Host "Waiting for PostgreSQL to be ready..."
$retries = 0
$maxRetries = 10
while ($retries -lt $maxRetries) {
    try {
        $env:PGPASSWORD = "postgres"
        $result = psql -h localhost -U dm_tpp_user -d dm_tpp -c "SELECT 1" 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "PostgreSQL is ready."
            break
        }
    }
    catch {
        Write-Host "PostgreSQL is not ready yet. Retrying..."
    }
    Start-Sleep -Seconds 1
    $retries++
    if ($retries -lt $maxRetries) {
        Write-Host "." -NoNewline
    }


}

if ($retries -eq $maxRetries) {
    Write-Host "PostgreSQL did not become ready in time. Exiting."
    docker compose -f deployment/docker-compose/docker-compose.local.yml logs postgres
    exit 1
}

Write-Host "Environment setup complete. "
Write-Host "================================"
Write-Host "PostgreSQL Connection Details:"
Write-Host "Host: localhost"
Write-Host "Port: 5432"
Write-Host "Database: dm_tpp"
Write-Host "User: dm_tpp_user"
Write-Host "Password: postgres"
Write-Host "nToxyproxy API http://localhost:8474"
Write-Host "Run database migrations"
