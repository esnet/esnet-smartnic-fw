# syntax=docker/dockerfile:1

ARG SMARTNIC_DPDK_IMAGE_URI
FROM ${SMARTNIC_DPDK_IMAGE_URI}

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

# Configure local ubuntu mirror as package source
#COPY sources.list /etc/apt/sources.list

RUN <<EOT
    set -ex
    ln -fs /usr/share/zoneinfo/UTC /etc/localtime
    apt update -y
    apt upgrade -y

    apt install -y --no-install-recommends \
      bash-completion \
      build-essential \
      cdbs \
      curl \
      devscripts \
      equivs \
      fakeroot \
      less \
      libdistro-info-perl \
      libmicrohttpd-dev \
      libpython3-dev \
      lsof \
      net-tools \
      ninja-build \
      libgmp-dev \
      libprotobuf-dev \
      libgrpc++-dev \
      libgrpc++1 \
      protobuf-compiler \
      protobuf-compiler-grpc \
      pkg-config \
      screen \
      socat \
      tree

    apt install -y --no-install-recommends \
      python3-pip \
      python3-setuptools

    pip3 install \
      grpcio-tools \
      meson \
      poetry \
      yq
EOT

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

COPY . /sn-fw/source
WORKDIR /sn-fw/source

# Build and install the Python regmap library.
RUN <<EOF
    set -ex
    cd regio
    poetry build
    pip3 install --find-links ./dist regio[shells]
EOF

RUN --mount=type=cache,target=/sn-fw/source/subprojects/packagecache <<EOF
    set -ex
    meson subprojects purge --confirm
    ln -fs \
      ${PWD}/sn-hw/artifacts.${SN_HW_BOARD}.${SN_HW_APP_NAME}.${SN_HW_VER}.zip \
      subprojects/packagefiles/esnet-smartnic-hwapi.zip
    meson subprojects download
    meson setup /sn-fw/build /sn-fw/source
    meson compile --clean -C /sn-fw/build
    meson install -C /sn-fw/build
    ldconfig

    # Install the generated Python regmap and configuration client.
    pip3 install \
      --find-links /usr/local/share/esnet-smartnic/python \
      regmap_esnet_smartnic \
      sn_cfg \
      sn_p4

    # Install bash completions.
    regio-esnet-smartnic -t zero -p none completions bash >/usr/share/bash-completion/completions/regio-esnet-smartnic
    sn-cfg completions bash >/usr/share/bash-completion/completions/sn-cfg
    sn-p4 completions bash >/usr/share/bash-completion/completions/sn-p4

    cat <<'_EOF' >>/root/.bashrc

# Enable bash completion for non-login interactive shells.
if [ -f /etc/bash_completion ] && ! shopt -oq posix; then
    . /etc/bash_completion
fi
_EOF
EOF

COPY <<EOF /sn-fw/buildinfo.env
SN_FW_GROUP=${SN_FW_GROUP}
SN_FW_REPO=${SN_FW_REPO}
SN_FW_BRANCH=${SN_FW_BRANCH}
SN_FW_APP_NAME=${SN_FW_APP_NAME}
SN_FW_VER=${SN_FW_VER}
SN_FW_COMMIT=${SN_FW_COMMIT}
SN_HW_GROUP=${SN_HW_GROUP}
SN_HW_REPO=${SN_HW_REPO}
SN_HW_BRANCH=${SN_HW_BRANCH}
SN_HW_BOARD=${SN_HW_BOARD}
SN_HW_APP_NAME=${SN_HW_APP_NAME}
SN_HW_VER=${SN_HW_VER}
SN_HW_COMMIT=${SN_HW_COMMIT}
EOF

# Setup the test automation framework.
WORKDIR sn-stack/test
RUN <<EOF
    set -ex
    mkdir /test
    cp entrypoint.sh /test/.

    # Install Python dependencies for the test automation libraries.
    for req in $(find . -type f -name pip-requirements.txt); do
        pip3 install --no-deps --requirement="${req}"
    done
EOF

# Install the MinIO client command line tool.
ADD \
    --chmod=755 \
    https://dl.min.io/client/mc/release/linux-amd64/archive/mc.RELEASE.2023-11-20T16-30-59Z \
    /usr/local/bin/mc

WORKDIR /
CMD ["/bin/bash"]
