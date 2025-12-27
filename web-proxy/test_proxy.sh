#!/bin/bash

# Simple test script for proxy server
# Usage: ./test_proxy.sh [proxy_host] [proxy_port]

PROXY_HOST=${1:-localhost}
PROXY_PORT=${2:-8080}

echo "Testing proxy server at $PROXY_HOST:$PROXY_PORT"
echo ""

# Test 1: Simple GET request
echo "Test 1: GET request to httpbin.org/get"
curl -x "$PROXY_HOST:$PROXY_PORT" -v "http://httpbin.org/get" 2>&1 | head -20
echo ""
echo "---"
echo ""

# Test 2: Check if proxy is running
echo "Test 2: Check proxy connectivity"
if curl -x "$PROXY_HOST:$PROXY_PORT" --connect-timeout 5 "http://httpbin.org/get" > /dev/null 2>&1; then
    echo "✓ Proxy is working!"
else
    echo "✗ Proxy is not responding"
    echo "Make sure proxy server is running: ./proxy_server $PROXY_PORT"
fi

