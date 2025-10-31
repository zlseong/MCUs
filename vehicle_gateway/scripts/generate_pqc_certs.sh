#!/bin/bash
# Generate PQC certificates for VMG
# Based on Benchmark_mTLS_with_PQC/generate_certs.sh

set -e

CERTS_DIR="certs"
OPENSSL="openssl"

# Check OpenSSL version
$OPENSSL version | grep -q "OpenSSL 3" || {
    echo "Error: OpenSSL 3.x required for PQC support"
    exit 1
}

mkdir -p $CERTS_DIR
cd $CERTS_DIR

# Generate PQC CA
echo "[1/8] Generating PQC CA..."
$OPENSSL req -new -x509 -days 3650 -nodes \
    -newkey rsa:2048 \
    -keyout ca_pqc.key \
    -out ca_pqc.crt \
    -subj "/C=KR/O=VMG/CN=VMG-PQC-CA"

# KEM types (All combinations for Hybrid TLS)
KEMS=("x25519" "mlkem512" "mlkem768" "mlkem1024")

# Signature types (ECDSA + ML-DSA)
SIGS=("ecdsa_secp256r1_sha256" "mldsa44" "mldsa65" "mldsa87")

echo "[2/8] Generating server certificates..."

for kem in "${KEMS[@]}"; do
    for sig in "${SIGS[@]}"; do
        name="${kem}_${sig}"
        
        # Skip non-sensible combinations
        if [ "$kem" = "x25519" ] && [ "$sig" != "ecdsa_secp256r1_sha256" ]; then
            continue  # X25519 only pairs with ECDSA
        fi
        
        # Map to OpenSSL sig algorithm
        case $sig in
            "ecdsa_secp256r1_sha256")
                openssl_sig="ec"
                ;;
            "mldsa44")
                openssl_sig="dilithium2"
                ;;
            "mldsa65")
                openssl_sig="dilithium3"
                ;;
            "mldsa87")
                openssl_sig="dilithium5"
                ;;
        esac
        
        # Server key
        if [ "$openssl_sig" = "ec" ]; then
            $OPENSSL ecparam -genkey -name prime256v1 -out ${name}_server.key
        else
            $OPENSSL genpkey -algorithm $openssl_sig -out ${name}_server.key
        fi
        
        # Server CSR
        $OPENSSL req -new -key ${name}_server.key \
            -out ${name}_server.csr \
            -subj "/C=KR/O=VMG/CN=vmg-server"
        
        # Sign with PQC CA
        $OPENSSL x509 -req -in ${name}_server.csr \
            -CA ca_pqc.crt -CAkey ca_pqc.key -CAcreateserial \
            -days 365 -out ${name}_server.crt
        
        rm ${name}_server.csr
        
        echo "  Generated: ${name}_server.{crt,key}"
    done
done

echo "[3/8] Generating client certificates..."

for kem in "${KEMS[@]}"; do
    for sig in "${SIGS[@]}"; do
        name="${kem}_${sig}"
        
        # Skip non-sensible combinations
        if [ "$kem" = "x25519" ] && [ "$sig" != "ecdsa_secp256r1_sha256" ]; then
            continue  # X25519 only pairs with ECDSA
        fi
        
        case $sig in
            "ecdsa_secp256r1_sha256")
                openssl_sig="ec"
                ;;
            "mldsa44")
                openssl_sig="dilithium2"
                ;;
            "mldsa65")
                openssl_sig="dilithium3"
                ;;
            "mldsa87")
                openssl_sig="dilithium5"
                ;;
        esac
        
        # Client key
        if [ "$openssl_sig" = "ec" ]; then
            $OPENSSL ecparam -genkey -name prime256v1 -out ${name}_client.key
        else
            $OPENSSL genpkey -algorithm $openssl_sig -out ${name}_client.key
        fi
        
        # Client CSR
        $OPENSSL req -new -key ${name}_client.key \
            -out ${name}_client.csr \
            -subj "/C=KR/O=VMG/CN=tc375-client"
        
        # Sign with PQC CA
        $OPENSSL x509 -req -in ${name}_client.csr \
            -CA ca_pqc.crt -CAkey ca_pqc.key -CAcreateserial \
            -days 365 -out ${name}_client.crt
        
        rm ${name}_client.csr
        
        echo "  Generated: ${name}_client.{crt,key}"
    done
done

cd ..

echo ""
echo "=========================================="
echo "PQC Certificate Generation Complete"
echo "=========================================="
echo "CA:      certs/ca_pqc.crt"
echo "Server:  certs/*_server.{crt,key}"
echo "Client:  certs/*_client.{crt,key}"
echo ""
echo "Generated 13 combinations:"
echo ""
echo "  [0] X25519 + ECDSA-P256 (Classical, baseline)"
echo ""
echo "  [1-3] ML-KEM + ECDSA-P256 (Hybrid, lighter signature):"
echo "    [2] mlkem768_ecdsa_secp256r1_sha256_* - RECOMMENDED (DEFAULT)"
echo "    mlkem512/1024_ecdsa_secp256r1_sha256_*"
echo ""
echo "  [4-6] ML-KEM-512 + ML-DSA (Pure PQC, 128-bit):"
echo "    mlkem512_mldsa44/65/87_*"
echo ""
echo "  [7-9] ML-KEM-768 + ML-DSA (Pure PQC, 192-bit):"
echo "    mlkem768_mldsa44/65/87_*"
echo ""
echo "  [10-12] ML-KEM-1024 + ML-DSA (Pure PQC, 256-bit):"
echo "    mlkem1024_mldsa44/65/87_*"
echo ""
echo "Note: Use generate_standard_certs.sh for DoIP (mbedTLS)"
echo "=========================================="

