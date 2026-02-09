# Toxiproxy Pofiles for DM-TPP Experiements
function Clear-AllToxics {
    $toxics = Invoke-RestMethod -Method Get -Uri "http://localhost:8474/proxies/postgres_proxy/toxics" -UserAgent "PowerShell"
    foreach ($toxic in $toxics) {
        Invoke-RestMethod -Method Delete -Uri "http://localhost:8474/proxies/postgres_proxy/toxics/$($toxic.name)" -UserAgent "PowerShell" | Out-Null
    }

    Write-Host "Cleared all toxics "

}

function Set-ConcurrentWriterProfile {
    Clear-AllToxics
    Write-Host "Setting profile: Concurrent Writers (50ms latency)"

    Invoke-RestMethod -Method Post -Uri "http://localhost:8474/proxies/postgres_proxy/toxics" -UserAgent "PowerShell" -ContentType "application/json" -Body (@{
        name       = "concurrent_delay"
        type       = "latency"
        stream     = "downstream"
        toxicity   = 1.0
        attributes = @{
            latency = 50
            jitter  = 10
        }
    } | ConvertTo-Json) | Out-Null
    Write-Host "Concurrent Writers profile applied."
}

function Set-SerializableAnomalyProfile {
    Clear-AllToxics
    Write-Host "Setting profile: Seriablizable Anomaly (200ms latency)"
    Invoke-RestMethod -Method Post -Uri "http://localhost:8474/proxies/postgres_proxy/toxics" -UserAgent "PowerShell" -ContentType "application/json" -Body (@{
        name       = "seriablizable_latency_downstream"
        type       = "latency"
        stream     = "downstream"
        toxicity   = 1.0
        attributes = @{
            latency = 200
            jitter  = 50
        }
    } | ConvertTo-Json) | Out-Null
    Write-Host "Serializable Anomaly profile applied."
}

function Set-CrashRecoveryProfile {
    Clear-AllToxics
    Write-Host "Setting profile: Crash Recovery (10% timeout)"

    Invoke-RestMethod -Method Post -Uri "http://localhost:8474/proxies/postgres_proxy/toxics" -UserAgent "PowerShell" -ContentType "application/json" -Body (@{
        name       = "crash_timeout"
        type       = "latency"
        stream     = "downstream"
        toxicity   = 0.1
        attributes = @{
            timeout = 1000
        }
    } | ConvertTo-Json) | Out-Null

    Write-Host "Crash Recovery profile applied."
}

