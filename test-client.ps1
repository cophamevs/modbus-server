#!/usr/bin/env powershell
# Test script for Modbus JSON Server

function Send-Command {
    param(
        [string]$command,
        [string]$serverHost = "localhost",
        [int]$serverPort = 1502
    )
    
    try {
        $socket = New-Object System.Net.Sockets.TcpClient($serverHost, $serverPort)
        $stream = $socket.GetStream()
        $writer = New-Object System.IO.StreamWriter($stream)
        $reader = New-Object System.IO.StreamReader($stream)
        
        Write-Host ">>> Sending: $command"
        $writer.WriteLine($command)
        $writer.Flush()
        
        Start-Sleep -Milliseconds 200
        
        Write-Host "<<< Response:"
        while ($reader.Peek() -ge 0) {
            $line = $reader.ReadLine()
            Write-Host "    $line"
        }
        
        $writer.Close()
        $socket.Close()
    }
    catch {
        Write-Error "Failed: $_"
    }
    
    Write-Host ""
}

Write-Host "========== Modbus Server Test ==========" -ForegroundColor Green
Write-Host ""

# Test 1: Get status
Write-Host "Test 1: Get server status" -ForegroundColor Yellow
Send-Command '{"cmd":"status"}'

# Test 2: Update register
Write-Host "Test 2: Update holding register 0 to 1234" -ForegroundColor Yellow
Send-Command '{"type":"holding_registers","address":0,"datatype":"uint16","byte_order":"LE","value":1234}'

# Test 3: Stop server
Write-Host "Test 3: Stop server" -ForegroundColor Yellow
Send-Command '{"cmd":"stop"}'

Write-Host "========== Tests Complete ==========" -ForegroundColor Green
