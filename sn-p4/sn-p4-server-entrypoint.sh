#!/usr/bin/env bash

set -e -o pipefail

#---------------------------------------------------------------------------------------------------
rm -rf /certs
mkdir /certs
chmod 600 /certs

openssl ecparam -name prime256v1 -genkey -out /certs/key.pem
openssl req \
        -x509 -new -days 3650 \
        -nodes -key /certs/key.pem \
        -subj "/CN=${HOSTNAME}" \
        -addext "subjectAltName=DNS:${HOSTNAME}" \
        -out /certs/cert.pem

echo '================================================================================'
echo "Created self-signed TLS certificate."
openssl x509 -noout -issuer -subject -dates -ext subjectAltName -in /certs/cert.pem
echo '================================================================================'

#---------------------------------------------------------------------------------------------------
# Build up the command line to invoke the gRPC server.
cmd=( sn-p4-agent server )

cmd+=( "--tls-cert-chain=/certs/cert.pem" )
cmd+=( "--tls-key=/certs/key.pem" )

# Specify the devices for the server to attach to.
cmd+=( ${SN_P4_SERVER_DEVICES} )

# Extra options passed to the docker command line.
cmd+=( "$@" )

#---------------------------------------------------------------------------------------------------
# Wait for the FPGA to come out of reset before running the server.
sn-p4-check-for-fpga-ready

# Start the server.
echo "Starting server: ${cmd[@]}"
eval "exec ${cmd[@]}"
