#!/bin/bash

##
## Generate Standard TLS Certificates for mbedTLS DoIP
## No PQC - RSA/ECDSA only (for in-vehicle network)
##

set -e

CERT_DIR="../certs"
DAYS=3650  # 10 years

mkdir -p "$CERT_DIR"
cd "$CERT_DIR"

echo "=================================================="
echo "Generating Standard TLS Certificates (mbedTLS)"
echo "=================================================="
echo ""

# ============================================================================
# 1. CA Certificate (Root CA)
# ============================================================================

echo "[1/5] Generating CA private key (RSA 4096)..."
openssl genpkey -algorithm RSA -out ca.key -pkeyopt rsa_keygen_bits:4096

echo "[2/5] Generating CA certificate..."
openssl req -new -x509 -key ca.key -out ca.crt -days $DAYS \
    -subj "/C=KR/ST=Seoul/L=Seoul/O=VMG-CA/CN=VMG Root CA"

echo ""

# ============================================================================
# 2. VMG Server Certificate
# ============================================================================

echo "[3/5] Generating VMG server private key (RSA 2048)..."
openssl genpkey -algorithm RSA -out vmg_server.key -pkeyopt rsa_keygen_bits:2048

echo "[4/5] Generating VMG server certificate..."
openssl req -new -key vmg_server.key -out vmg_server.csr \
    -subj "/C=KR/ST=Seoul/L=Seoul/O=VMG/CN=vmg.vehicle.local"

# Server certificate extensions
cat > vmg_server_ext.cnf <<EOF
basicConstraints = CA:FALSE
nsCertType = server
nsComment = "VMG DoIP Server Certificate"
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer:always
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = vmg.vehicle.local
DNS.2 = localhost
IP.1 = 192.168.1.1
IP.2 = 127.0.0.1
EOF

openssl x509 -req -in vmg_server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out vmg_server.crt -days $DAYS \
    -extfile vmg_server_ext.cnf

rm vmg_server.csr vmg_server_ext.cnf

echo ""

# ============================================================================
# 3. TC375 Client Certificate (Zonal Gateway / ECU)
# ============================================================================

echo "[5/5] Generating TC375 client private key (RSA 2048)..."
openssl genpkey -algorithm RSA -out tc375_client.key -pkeyopt rsa_keygen_bits:2048

echo "Generating TC375 client certificate..."
openssl req -new -key tc375_client.key -out tc375_client.csr \
    -subj "/C=KR/ST=Seoul/L=Seoul/O=TC375/CN=tc375.vehicle.local"

# Client certificate extensions
cat > tc375_client_ext.cnf <<EOF
basicConstraints = CA:FALSE
nsCertType = client
nsComment = "TC375 DoIP Client Certificate"
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer:always
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
EOF

openssl x509 -req -in tc375_client.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out tc375_client.crt -days $DAYS \
    -extfile tc375_client_ext.cnf

rm tc375_client.csr tc375_client_ext.cnf

echo ""
echo "=================================================="
echo "Certificate Generation Complete!"
echo "=================================================="
echo ""
echo "Generated certificates:"
echo "  CA:          ca.crt / ca.key"
echo "  VMG Server:  vmg_server.crt / vmg_server.key"
echo "  TC375 Client: tc375_client.crt / tc375_client.key"
echo ""
echo "Usage:"
echo "  VMG Server:"
echo "    ./vmg_doip_server certs/vmg_server.crt certs/vmg_server.key certs/ca.crt 13400"
echo ""
echo "  TC375 Client:"
echo "    Use tc375_client.crt / tc375_client.key for mTLS authentication"
echo ""
echo "Validity: $DAYS days (~10 years)"
echo ""

# Verify certificates
echo "Verifying certificates..."
openssl verify -CAfile ca.crt vmg_server.crt
openssl verify -CAfile ca.crt tc375_client.crt

echo ""
echo "Done!"

