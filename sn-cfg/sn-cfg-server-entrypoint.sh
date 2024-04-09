#!/usr/bin/env bash

set -e -o pipefail

#---------------------------------------------------------------------------------------------------
certs_dir='/etc/letsencrypt'
certs_mnt_dir="${certs_dir}-host"
cert="${certs_dir}/${SN_TLS_PATH_PREFIX}${SN_TLS_CERT}"
cert_fullchain="${certs_dir}/${SN_TLS_PATH_PREFIX}${SN_TLS_FULLCHAIN}"
key="${certs_dir}/${SN_TLS_PATH_PREFIX}${SN_TLS_KEY}"

# Copy the server's certificate and private key from the host.
rm -rf "${certs_dir}"
if [[ -d "${certs_mnt_dir}" ]]; then
    cp -r "${certs_mnt_dir}" "${certs_dir}"
fi

# Generate a self-signed certificate for the server if an external one isn't provided.
if ! [[ -r "${cert}" ]]; then
    fqdn=$(hostname --fqdn)
    sans="DNS:${fqdn}"
    sans+=",DNS:${SN_CFG_SERVER_HOST}"
    sans+=",DNS:localhost,IP:127.0.0.1"
    sans+=",DNS:ip6-localhost,IP:::1"

    mkdir -p $(dirname "${cert}")
    openssl req \
        -x509 -new -days 365 \
        -nodes -newkey rsa:4096 -keyout "${key}" \
        -subj "/CN=${SN_CFG_SERVER_HOST}" \
        -addext "subjectAltName=${sans}" \
        -out "${cert}"
    ln -s $(basename "${cert}") "${cert_fullchain}"

    msg='Created self-signed TLS certificate.'
else
    msg='Using external TLS certificate.'
fi

echo '================================================================================'
echo "${msg}"
openssl x509 -noout -issuer -subject -dates -ext subjectAltName -in "${cert}"
echo '================================================================================'

#---------------------------------------------------------------------------------------------------
# Build up the command line to invoke the gRPC server.
cmd=( sn-cfg-agent server )

cmd+=( "--tls-cert-chain=${cert_fullchain}" )
cmd+=( "--tls-key=${key}" )

# Specify the devices for the server to attach to.
cmd+=( ${SN_CFG_SERVER_DEVICES} )

# Extra options passed to the docker command line.
cmd+=( "$@" )

#---------------------------------------------------------------------------------------------------
# Start the server.
eval "exec ${cmd[@]}"
