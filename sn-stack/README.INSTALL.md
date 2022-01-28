Setting up the Runtime Environment
==================================

The smartnic runtime environment also requires `docker` and `docker compose` as described in README.md at the top of this repo.

Set up Xilinx Labtools
----------------------

In order to load a smartnic FPGA bitfile into the Xilinx U280 card, we need to make use of the Xilinx Labtools.  The instructions for setting up labtools can be found in a separate repository.  That repository will provide us with a docker container populated with the xilinx labtools.  That docker container will be used to program the FPGA bitfile.

Running the firmware
--------------------

The firmware artifact produced by the build (see README.md at the top of this repo) should be transferred to the runtime system that hosts an FPGA card.

```
unzip artifacts.esnet-smartnic-fw.package_focal.user.001.zip
cd sn-stack
cp example.env .env
# edit the .env file to provide sane values for
#    FPGA_PCIE_DEV=0000:d8:00
# and IFF you have more than one JTAG you also need a line like this
#    HW_TARGET_SERIAL=21760204S029A
docker compose build
docker compose up -d
```

Verify that the stack is running like this

```
docker compose ps
```

Verifying the bitfile download
------------------------------

```
docker compose logs smartnic-hw
```

Inspecting registers and interacting with the firmware
------------------------------------------------------

The firmware runtime environment exists inside of the `smartnic-fw` container.  Here, we exec a shell inside of that container and have a look around.

```
docker compose exec smartnic-fw bash
regio syscfg
```

Stopping the runtime environment
--------------------------------

When we're finished using the smartnic runtime environment, we can stop and remove our docker containers.

```
docker compose down -v
```