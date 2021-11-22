FROM ubuntu:focal

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL C.UTF-8
ENV LANG C.UTF-8

# Configure local ubuntu mirror as package source
COPY sources.list /etc/apt/sources.list

RUN \
  ln -fs /usr/share/zoneinfo/America/Los_Angeles /etc/localtime && \
  apt-get update -y && \
  apt-get upgrade -y && \
  apt-get install -y --no-install-recommends \
    python3 \
    python3-pip \
    python3-click \
    python3-yaml \
    python3-jinja2 \
    libgmp10 \
    pciutils \
    coreutils \
    jq \
    vim-tiny \
    lsb-release \
    locales \
    && \
  pip3 install \
    pyyaml-include \
    yq \
    && \
  apt-get autoclean && \
  apt-get autoremove && \
  locale-gen en_US.UTF-8 && \
  update-locale LANG=en_US.UTF-8 && \
  rm -rf /var/lib/apt/lists/*

ARG SN_FW_VER
COPY debs/focal/*-${SN_FW_VER}_*.deb /debs/
COPY debs/focal/*-${SN_FW_VER}_*.ddeb /debs/
RUN dpkg -i \
    /debs/esnet-smartnic1_1.0.0-${SN_FW_VER}_amd64.deb \
    /debs/esnet-smartnic1-dbgsym_1.0.0-${SN_FW_VER}_amd64.ddeb \
    /debs/esnet-smartnic-dev_1.0.0-${SN_FW_VER}_amd64.deb
CMD ["/bin/bash", "-l"]
