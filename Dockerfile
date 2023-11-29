# syntax=docker/dockerfile:1

ARG SMARTNIC_DPDK_IMAGE_URI
FROM ${SMARTNIC_DPDK_IMAGE_URI}

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL C.UTF-8
ENV LANG C.UTF-8

# Configure local ubuntu mirror as package source
#COPY sources.list /etc/apt/sources.list

RUN <<EOT
    set -ex
    ln -fs /usr/share/zoneinfo/UTC /etc/localtime
    apt update -y
    apt upgrade -y

    apt install -y --no-install-recommends \
      build-essential \
      cdbs \
      devscripts \
      equivs \
      fakeroot \
      libdistro-info-perl \
      ninja-build \
      python3-click \
      python3-yaml \
      python3-jinja2 \
      libgmp-dev \
      libprotobuf-dev \
      libgrpc++-dev \
      libgrpc++1 \
      protobuf-compiler \
      protobuf-compiler-grpc \
      pkg-config

    apt install -y --no-install-recommends \
      python3-pip \
      python3-setuptools

    pip3 install \
      meson \
      pyyaml-include \
      yq
EOT

ARG SN_HW_GROUP
ARG SN_HW_REPO
ARG SN_HW_BRANCH
ARG SN_HW_BOARD
ARG SN_HW_APP_NAME
ARG SN_HW_VER

ARG SN_FW_GROUP
ARG SN_FW_REPO
ARG SN_FW_BRANCH
ARG SN_FW_APP_NAME
ARG SN_FW_VER

ENV SN_HW_GROUP ${SN_HW_GROUP}
ENV SN_HW_REPO ${SN_HW_REPO}
ENV SN_HW_BRANCH ${SN_HW_BRANCH}
ENV SN_HW_BOARD ${SN_HW_BOARD}
ENV SN_HW_APP_NAME ${SN_HW_APP_NAME}
ENV SN_HW_VER ${SN_HW_VER}

ENV SN_FW_GROUP ${SN_FW_GROUP}
ENV SN_FW_REPO ${SN_FW_REPO}
ENV SN_FW_BRANCH ${SN_FW_BRANCH}
ENV SN_FW_APP_NAME ${SN_FW_APP_NAME}
ENV SN_FW_VER ${SN_FW_VER}

COPY . /sn-fw/source
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
    meson install -C /sn-fw/build
    ldconfig
EOF

COPY <<EOF /sn-fw/buildinfo.env
SN_FW_GROUP=${SN_FW_GROUP}
SN_FW_REPO=${SN_FW_REPO}
SN_FW_BRANCH=${SN_FW_BRANCH}
SN_FW_APP_NAME=${SN_FW_APP_NAME}
SN_FW_VER=${SN_FW_VER}
SN_HW_GROUP=${SN_HW_GROUP}
SN_HW_REPO=${SN_HW_REPO}
SN_HW_BRANCH=${SN_HW_BRANCH}
SN_HW_BOARD=${SN_HW_BOARD}
SN_HW_APP_NAME=${SN_HW_APP_NAME}
SN_HW_VER=${SN_HW_VER}
EOF

# Install test automation packages.
RUN pip3 install --no-deps --requirement=sn-stack/test/pip-requirements.txt

WORKDIR /
CMD ["/bin/bash", "-l"]
