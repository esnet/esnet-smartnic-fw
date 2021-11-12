FROM xilinx-labtools:v2021.1

ENV DEBIAN_FRONTEND=noninteractive

# Configure local ubuntu mirror as package source
COPY sources.list /etc/apt/sources.list

# Install packages required by FPGA loading scripts
RUN \
  ln -fs /usr/share/zoneinfo/America/Los_Angeles /etc/localtime && \
  apt-get update -y && \
  apt-get upgrade -y && \
  apt-get install -y --no-install-recommends \
    jq \
    file \
    pciutils \
    && \
  apt-get autoclean && \
  apt-get autoremove && \
  locale-gen en_US.UTF-8 && \
  update-locale LANG=en_US.UTF-8 && \
  rm -rf /var/lib/apt/lists/*

# Install FPGA loading scripts
COPY smartnic-hw/scripts/program_card.sh /usr/local/bin/
COPY smartnic-hw/scripts/program_card.tcl /opt/Xilinx/tcl/
COPY smartnic-hw/scripts/read_jtag_registers.tcl /opt/Xilinx/tcl/
COPY smartnic-hw/scripts/check_fpga_version.sh /usr/local/bin/

ARG SN_FW_VER
COPY debs/focal/*-${SN_FW_VER}_*.deb /debs/
RUN dpkg -i \
    /debs/esnet-smartnic1_1.0.0-${SN_FW_VER}_amd64.deb

CMD ["/bin/bash", "-l"]
