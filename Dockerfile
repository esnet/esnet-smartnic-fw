# syntax=docker/dockerfile:1

#
# Create a stage that provides a runtime base customized to select the ESnet package mirrors
# Do this only once so we can re-use it for future stages
#

ARG DOCKERHUB_PROXY
FROM ${DOCKERHUB_PROXY:-}library/ubuntu:jammy AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Configure local ubuntu mirror as package source
RUN \
    sed -i -re 's|(http://)([^/]+.*)/|\1linux.mirrors.es.net/ubuntu|g' /etc/apt/sources.list

RUN <<EOF
    set -ex
    ln -fs /usr/share/zoneinfo/UTC /etc/localtime
    apt update -y

    apt install -y --no-install-recommends \
      locales \

    locale-gen en_US.UTF-8
    update-locale LANG=en_US.UTF-8
    rm -rf /var/lib/apt/lists/*
EOF

#
# Create a stage that provides a full build environment
# Do this only once so we can re-use it for future build stages
#

FROM base AS builder

RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      build-essential \
      pkg-config \
      python3 \
      python3-pip \
      python3-setuptools \
      python3-wheel \
      wget
EOF

RUN pip3 install --no-cache-dir \
    meson \
    ninja

#
# Build DPDK
#

FROM builder AS dpdk

# Install build dependencies for DPDK
# See: https://doc.dpdk.org/guides/linux_gsg/sys_reqs.html#compilation-of-the-dpdk

RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      libbpf-dev \
      libbsd-dev \
      libnuma-dev \
      libpcap-dev \
      libssl-dev \
      python3-pyelftools \
      zlib1g-dev
EOF

COPY xilinx-qdma-for-opennic/QDMA/DPDK /QDMA/DPDK
COPY dpdk-patches /dpdk-patches

# Download build and install DPDK
ARG DPDK_BASE_URL="https://fast.dpdk.org/rel"
ARG DPDK_VER="22.11.2"
#ARG DPDK_TOPDIR="dpdk-${DPDK_VER}"
ARG DPDK_TOPDIR="dpdk-stable-${DPDK_VER}"
ARG DPDK_PLATFORM
ARG DPDK_CPU_INSTRUCTION_SET

RUN <<EOF
    set -ex
    wget -q $DPDK_BASE_URL/dpdk-$DPDK_VER.tar.xz
    tar xf dpdk-$DPDK_VER.tar.xz
    rm dpdk-$DPDK_VER.tar.xz
    cd $DPDK_TOPDIR
    ln -s /QDMA/DPDK/drivers/net/qdma ./drivers/net
    patch -p 1 < /dpdk-patches/0000-dpdk-include-xilinx-qdma-driver.patch

    meson setup build \
      -Dplatform=${DPDK_PLATFORM:-generic} \
      -Dcpu_instruction_set=${DPDK_CPU_INSTRUCTION_SET:-auto} \
      -Denable_drivers=net/af_packet,net/pcap,net/qdma,net/ring,net/tap,net/virtio \
      -Denable_apps=dumpcap,pdump,proc-info,test-cmdline,test-pmd

    cd build
    ninja
    meson install --destdir /dpdk-install-root
    cd ../../
    rm -r $DPDK_TOPDIR
EOF

#
# Build pktgen-dpdk
#

FROM builder AS pktgen

# Import build artifacts from dpdk stage
COPY --from=dpdk /dpdk-install-root/ /
RUN ldconfig

# Install the build dependencies for libdpdk
RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      libbsd-dev \
      libelf-dev \
      libnuma-dev \
      libpcap-dev
EOF

# Install the build dependencies for pktgen
RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      liblua5.3-dev \
      make
EOF

# Build and install pktgen-dpdk
ARG PKTGEN_BASE_URL="https://github.com/pktgen/Pktgen-DPDK/archive/refs/tags"
ARG PKTGEN_VER=23.03.0
ARG PKTGEN_TOPDIR="Pktgen-DPDK-pktgen-${PKTGEN_VER}"
RUN <<EOF
    wget -q $PKTGEN_BASE_URL/pktgen-$PKTGEN_VER.tar.gz
    tar xaf pktgen-$PKTGEN_VER.tar.gz
    rm pktgen-$PKTGEN_VER.tar.gz
    cd $PKTGEN_TOPDIR
    export PKTGEN_DESTDIR=/pktgen-install-root
    make buildlua
    make install
    cd ..
    rm -r $PKTGEN_TOPDIR
EOF

#
# Build the firmware tools
#

FROM builder AS firmware

# Install build deps for the firmware tools
RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      libgmp-dev \
      libgrpc++-dev \
      libmicrohttpd-dev \
      libprotobuf-dev \
      libpython3-dev \
      protobuf-compiler \
      protobuf-compiler-grpc \
      zstd
EOF

RUN pip3 install --no-cache-dir \
    grpcio-tools \
    poetry

COPY . /sn-fw/source
WORKDIR /sn-fw/source

# Build and install the Python regmap library.
WORKDIR regio
RUN poetry build
RUN pip3 install --no-cache-dir \
    --find-links \
    ./dist \
    regio[shells]

# Set up env vars that point us to the hardware artifact
ARG SN_HW_BOARD
ARG SN_HW_APP_NAME
ARG SN_HW_VER

ENV SN_HW_BOARD=${SN_HW_BOARD}
ENV SN_HW_APP_NAME=${SN_HW_APP_NAME}
ENV SN_HW_VER=${SN_HW_VER}

WORKDIR /sn-fw/source
RUN --mount=type=cache,target=/sn-fw/source/subprojects/packagecache <<EOF
    set -ex
    meson subprojects purge --confirm
    ln -fs \
      ${PWD}/sn-hw/artifacts.${SN_HW_BOARD}.${SN_HW_APP_NAME}.${SN_HW_VER}.zip \
      subprojects/packagefiles/esnet-smartnic-hwapi.zip
    meson subprojects download
    meson setup /sn-fw/build /sn-fw/source
    meson compile --clean -C /sn-fw/build
    meson install -C /sn-fw/build --destdir /firmware-install-root
EOF

# Compress the raw FPGA .bit and .mcs files to reduce the container size.
# This is a temporary step until all of the hardware builds compress these files in their artifacts
RUN <<EOF
    set -ex
    fw_dir=/firmware-install-root/usr/local/lib/firmware/esnet-smartnic
    for f in ${fw_dir}/esnet-smartnic.bit ${fw_dir}/esnet-smartnic.mcs ; do
      if [ -e ${f} ] ; then
        # found an uncompressed file, compress it to save space in the image
        zstd -9 --rm ${f}
      fi
    done
EOF

#
# Provide a functional runtime environment
#

FROM base AS runtime

# Install the downstream build dependencies for libdpdk and runtime deps for pktgen
RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      libbsd-dev \
      libelf-dev \
      libpcap-dev \
      libnuma-dev \
      \
      liblua5.3-0

    apt autoclean
    apt autoremove
    rm -rf /var/lib/apt/lists/*
EOF

# Install runtime deps for the firmware tools
RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      bash-completion \
      curl \
      kmod \
      less \
      lsof \
      net-tools \
      libgrpc++1 \
      libmicrohttpd12 \
      python3 \
      python3-pip \
      screen \
      socat \
      tree

    apt autoclean
    apt autoremove
    rm -rf /var/lib/apt/lists/*
EOF

RUN pip3 install --no-cache-dir \
    grpcio-tools

# Import the build artifacts from the firmware
# Do this as late as possible to allow the firmware build stage to run in parallel with this stage
COPY --from=firmware /firmware-install-root/ /
COPY --from=firmware /sn-fw/source/regio/dist/ /usr/local/share/esnet-smartnic/python
RUN ldconfig

# Install the generated Python regmap and configuration client.
RUN pip3 install --no-cache-dir \
    --find-links /usr/local/share/esnet-smartnic/python \
    regio[shells] \
    regmap_esnet_smartnic \
    sn_cfg \
    sn_p4

# Install bash completions.
RUN regio-esnet-smartnic -t zero -p none completions bash >/usr/share/bash-completion/completions/regio-esnet-smartnic
RUN sn-cfg completions bash >/usr/share/bash-completion/completions/sn-cfg
RUN sn-p4 completions bash >/usr/share/bash-completion/completions/sn-p4

RUN cat <<EOF >> /root/.bashrc

# Enable bash completion for non-login interactive shells.
if [ -f /etc/bash_completion ] && ! shopt -oq posix; then
    . /etc/bash_completion
fi
EOF

# RUN <<EOF
#     set -ex
#     cat <<'_EOF' >>/root/.bashrc

# # Enable bash completion for non-login interactive shells.
# if [ -f /etc/bash_completion ] && ! shopt -oq posix; then
#     . /etc/bash_completion
# fi
# _EOF
# EOF

# Setup the test automation framework.
# Install Python dependencies for the test automation libraries.
COPY ./sn-stack/test/ /test
WORKDIR /test
RUN <<EOF
    set -ex
    for req in $(find . -type f -name pip-requirements.txt); do
        pip3 install --no-cache-dir --no-deps --requirement="${req}"
    done
EOF

# Install the MinIO client command line tool.
ADD \
    --chmod=755 \
    https://dl.min.io/client/mc/release/linux-amd64/archive/mc.RELEASE.2025-05-21T01-59-54Z \
    /usr/local/bin/mc

# Install the gRPC health-check command line tool.
ADD \
    --chmod=755 \
    https://github.com/grpc-ecosystem/grpc-health-probe/releases/download/v0.4.39/grpc_health_probe-linux-amd64 \
    /usr/local/bin/grpc_health_probe

# Install gRPC debug tool (uncomment for inclusion).
# RUN <<EOF
#     set -ex
#     cd /tmp
#     wget -Ogrpcurl.deb https://github.com/fullstorydev/grpcurl/releases/download/v1.9.2/grpcurl_1.9.2_linux_amd64.deb
#     apt install ./grpcurl.deb
#     rm -f grpcurl.deb
# EOF

# Install some useful tools to have inside of this container
RUN <<EOF
    set -ex
    apt update -y

    apt install -y --no-install-recommends \
      iproute2 \
      jq \
      lsb-release \
      pciutils \
      python3 \
      python3-pip \
      tcpreplay \
      tshark \
      unzip \
      vim-tiny \
      zstd

    apt autoclean
    apt autoremove
    rm -rf /var/lib/apt/lists/*
EOF

RUN pip3 install --no-cache-dir \
    scapy \
    yq

# Import the build artifacts from both dpdk and pktgen layers
# Do this very late to allow dpdk and pktgen build stages to run in parallel to this stage
COPY --from=dpdk /dpdk-install-root/ /
COPY --from=pktgen /pktgen-install-root/ /
RUN ldconfig

# Set up the buildinfo.env file inside of the container
ARG SN_HW_GROUP
ARG SN_HW_REPO
ARG SN_HW_BRANCH
ARG SN_HW_BOARD
ARG SN_HW_APP_NAME
ARG SN_HW_VER
ARG SN_HW_COMMIT

ARG SN_FW_GROUP
ARG SN_FW_REPO
ARG SN_FW_BRANCH
ARG SN_FW_APP_NAME
ARG SN_FW_VER
ARG SN_FW_COMMIT

ENV SN_HW_GROUP=${SN_HW_GROUP}
ENV SN_HW_REPO=${SN_HW_REPO}
ENV SN_HW_BRANCH=${SN_HW_BRANCH}
ENV SN_HW_BOARD=${SN_HW_BOARD}
ENV SN_HW_APP_NAME=${SN_HW_APP_NAME}
ENV SN_HW_VER=${SN_HW_VER}
ENV SN_HW_COMMIT=${SN_HW_COMMIT}

ENV SN_FW_GROUP=${SN_FW_GROUP}
ENV SN_FW_REPO=${SN_FW_REPO}
ENV SN_FW_BRANCH=${SN_FW_BRANCH}
ENV SN_FW_APP_NAME=${SN_FW_APP_NAME}
ENV SN_FW_VER=${SN_FW_VER}
ENV SN_FW_COMMIT=${SN_FW_COMMIT}

# COPY <<EOF /sn-fw/buildinfo.env
# SN_FW_GROUP=${SN_FW_GROUP}
# SN_FW_REPO=${SN_FW_REPO}
# SN_FW_BRANCH=${SN_FW_BRANCH}
# SN_FW_APP_NAME=${SN_FW_APP_NAME}
# SN_FW_VER=${SN_FW_VER}
# SN_FW_COMMIT=${SN_FW_COMMIT}
# SN_HW_GROUP=${SN_HW_GROUP}
# SN_HW_REPO=${SN_HW_REPO}
# SN_HW_BRANCH=${SN_HW_BRANCH}
# SN_HW_BOARD=${SN_HW_BOARD}
# SN_HW_APP_NAME=${SN_HW_APP_NAME}
# SN_HW_VER=${SN_HW_VER}
# SN_HW_COMMIT=${SN_HW_COMMIT}
# EOF

WORKDIR /
CMD ["/bin/bash", "-l"]

