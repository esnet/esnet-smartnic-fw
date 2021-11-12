ARG OS_CODENAME=focal
FROM ubuntu:$OS_CODENAME

ENV DEBIAN_FRONTEND noninteractive
ENV LC_ALL C.UTF-8
ENV LANG C.UTF-8

# Repeat OS_CODENAME here since "FROM" clears all args
ARG OS_CODENAME

# Configure local ubuntu mirror as package source
COPY sources.list /etc/apt/sources.list

RUN \
  ln -fs /usr/share/zoneinfo/America/Los_Angeles /etc/localtime && \
  apt-get update -y && \
  apt-get upgrade -y && \
  apt-get install -y --no-install-recommends \
    build-essential \
    cdbs \
    devscripts \
    equivs \
    fakeroot \
    libdistro-info-perl \
  && \
  apt install -y --no-install-recommends \
    python3-pip \
    python3-setuptools \
  && \
  pip3 install \
    meson \
    pyyaml-include \
    yq
