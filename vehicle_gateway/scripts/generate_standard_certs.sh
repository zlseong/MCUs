#!/bin/bash
# Generate standard TLS certificates for DoIP (mbedTLS)
# No PQC - just RSA or ECDSA

set -e

CERTS_DIR="certs"
OPENSSL="openssl"

mkdir -p $CERTS_DIR
cd $CERTS_DIR

echo "=========================================="
echo "Generating Standard TLS Certificates"
echo "For DoIP (VMG ↔ TC375)"
echo "=========================================="

# Generate CA
echo "[1/3] Generating CA..."
$OPENSSL req -new -x509 -days 3650 -nodes \
    -newkey rsa:2048 \
    -keyout ca.key \
    -out ca.crt \
    -subj "/C=KR/O=VMG/CN=VMG-CA"

# Generate VMG server certificate
echo "[2/3] Generating VMG server certificate..."
$OPENSSL ecparam -genkey -name prime256v1 -out vmg_server.key

$OPENSSL req -new -key vmg_server.key \
    -out vmg_server.csr \
    -subj "/C=KR/O=VMG/CN=vmg-server"

$OPENSSL x509 -req -in vmg_server.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -days 365 -out vmg_server.crt

rm vmg_server.csr

# Generate TC375 client certificate
echo "[3/3] Generating TC375 client certificate..."
$OPENSSL ecparam -genkey -name prime256v1 -out tc375_client.key

$OPENSSL req -new -key tc375_client.key \
    -out tc375_client.csr \
    -subj "/C=KR/O=VMG/CN=tc375-client"

$OPENSSL x509 -req -in tc375_client.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -days 365 -out tc375_client.crt

rm tc375_client.csr

cd ..

echo ""
echo "=========================================="
echo "Standard TLS Certificate Generation Complete"
echo "=========================================="
echo "CA:          certs/ca.crt"
echo "VMG Server:  certs/vmg_server.{crt,key}"
echo "TC375 Client: certs/tc375_client.{crt,key}"
echo ""
echo "Use these for DoIP communication (mbedTLS)"
echo "=========================================="

