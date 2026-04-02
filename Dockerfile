# syntax=docker/dockerfile:1

#
# Create a stage that provides a runtime base customized to select the ESnet package mirrors
# Do this only once so we can re-use it for future stages
#

ARG DOCKERHUB_PROXY
FROM ${DOCKERHUB_PROXY:-}library/ubuntu:noble AS base

SHELL ["bash", "-c"]

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Configure local ubuntu mirror as package source
RUN \
    sed -i -re 's|(http://)([^/]+.*)/|\1linux.mirrors.es.net/ubuntu|g' /etc/apt/sources.list.d/ubuntu.sources

RUN <<EOF
    set -ex
    ln -fs /usr/share/zoneinfo/UTC /etc/localtime
    apt update -y
    apt upgrade -y

    apt install -y --no-install-recommends \
      ca-certificates \
      locales

    locale-gen en_US.UTF-8
    update-locale LANG=en_US.UTF-8
    rm -rf /var/lib/apt/lists/*
EOF

# Setup a Python virtualenv for managing Python-based build tools/libraries
# separately from system packages.
ADD \
    --unpack=true \
    --chown=root:root \
    --checksum=sha256:5a360b0de092ddf4131f5313d0411b48c4e95e8107e40c3f8f2e9fcb636b3583 \
    https://releases.astral.sh/github/uv/releases/download/0.10.11/uv-x86_64-unknown-linux-gnu.tar.gz \
    /root

RUN <<EOF
    set -ex

    # Install a Python virtualenv manager.
    chown root:root /root/uv-x86_64-unknown-linux-gnu/*
    mv /root/uv-x86_64-unknown-linux-gnu/uv{,x} /usr/local/bin/.
    rm -r /root/uv-x86_64-unknown-linux-gnu

    # Create a new virtualenv for the root user.
    uv venv --directory / --no-project --no-config --clear
EOF

# Make sure the Python virtualenv is always active.
ENV VIRTUAL_ENV="/.venv"
ENV PATH="${VIRTUAL_ENV}/bin:${PATH}"

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
      python3
EOF

RUN uv pip install --no-cache \
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
      zlib1g-dev
EOF

RUN uv pip install --no-cache \
    pyelftools

COPY xilinx-qdma-for-opennic/QDMA/DPDK /QDMA/DPDK
COPY dpdk-patches /dpdk-patches

# Download build and install DPDK
ARG DPDK_BASE_URL="https://fast.dpdk.org/rel"
ARG DPDK_VER="22.11.2"
ARG DPDK_TOPDIR="/root/dpdk-stable-${DPDK_VER}"
ARG DPDK_PLATFORM
ARG DPDK_CPU_INSTRUCTION_SET

ADD \
    --unpack=true \
    --chown=root:root \
    --checksum=sha256:af64bdda15087ff8d429894b9ea6cbbbb6ee7a932bdb344f82b0dc366379a2d4 \
    $DPDK_BASE_URL/dpdk-$DPDK_VER.tar.xz \
    /root

RUN <<EOF
    set -ex
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
COPY pktgen-patches /pktgen-patches
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
ARG PKTGEN_TOPDIR="/root/Pktgen-DPDK-pktgen-${PKTGEN_VER}"

ADD \
    --unpack=true \
    --chown=root:root \
    --checksum=sha256:bc26afd81d25d4ba4f8307edaf59e29f23525af6bc10de77472cb41390412df9 \
    $PKTGEN_BASE_URL/pktgen-$PKTGEN_VER.tar.gz \
    /root

RUN <<EOF
    set -ex
    cd $PKTGEN_TOPDIR
    patch -p 1 < /pktgen-patches/0000-pktgen-compile-errors.patch
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

RUN uv pip install --no-cache \
    grpcio-tools \
    poetry

COPY . /sn-fw/source
WORKDIR /sn-fw/source

# Build and install the Python regmap library.
WORKDIR regio
RUN poetry build
RUN uv pip install --no-cache \
    --find-links ./dist \
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
      openssl \
      libgrpc++1.51t64 \
      libmicrohttpd12 \
      python3 \
      screen \
      socat \
      tree \
      udev

    apt autoclean
    apt autoremove
    rm -rf /var/lib/apt/lists/*
EOF

RUN uv pip install --no-cache \
    grpcio-tools

# Import the build artifacts from the firmware
# Do this as late as possible to allow the firmware build stage to run in parallel with this stage
COPY --from=firmware /firmware-install-root/ /
COPY --from=firmware /sn-fw/source/regio/dist/ /usr/local/share/esnet-smartnic/python
RUN ldconfig

# Install the generated Python regmap and configuration client.
RUN uv pip install --no-cache \
    --find-links /usr/local/share/esnet-smartnic/python \
    regio[shells] \
    regmap_esnet_smartnic \
    sn_cfg \
    sn_p4

# Install bash completions.
RUN uv run \
    regio-esnet-smartnic -t zero -p none completions bash >/usr/share/bash-completion/completions/regio-esnet-smartnic
RUN uv run \
    sn-cfg completions bash >/usr/share/bash-completion/completions/sn-cfg
RUN uv run \
    sn-p4 completions bash >/usr/share/bash-completion/completions/sn-p4

RUN cat <<"EOF" >>/root/.bashrc

# Enable bash completion for non-login interactive shells.
if [ -f /etc/bash_completion ] && ! shopt -oq posix; then
    . /etc/bash_completion
fi

EOF

# Setup the test automation framework.
# Install Python dependencies for the test automation libraries.
COPY ./sn-stack/test/ /test
RUN <<EOF
    set -ex
    for req in $(find /test -type f -name pip-requirements.txt); do
        uv pip install --no-cache --no-deps --requirement="${req}"
    done
EOF

# Install the gRPC health-check command line tool.
ADD \
    --chmod=755 \
    https://github.com/grpc-ecosystem/grpc-health-probe/releases/download/v0.4.45/grpc_health_probe-linux-amd64 \
    /usr/local/bin/grpc_health_probe

# Install gRPC debug tool (uncomment for inclusion).

#ARG GRPCURL_VERSION="1.9.2"
#ARG GRPCURL_DEB="grpcurl_${GRPCURL_VERSION}_linux_amd64.deb"
#ADD \
#    --chown=root:root \
#    --checksum=sha256:be1a79d01b1dd7c42f06a76eb497964be731ca76f4f3ff2b2c471fdc01dfac60 \
#    https://github.com/fullstorydev/grpcurl/releases/download/v${GRPCURL_VERSION}/${GRPCURL_DEB} \
#    /root
#
#RUN <<EOF
#    set -ex
#    cd /root
#    apt install ./${GRPCURL_DEB}
#    rm -f ${GRPCURL_DEB}
#EOF

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
      rclone \
      tcpreplay \
      tshark \
      unzip \
      vim-tiny \
      wget \
      zstd

    apt autoclean
    apt autoremove
    rm -rf /var/lib/apt/lists/*
EOF

RUN uv pip install --no-cache \
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

