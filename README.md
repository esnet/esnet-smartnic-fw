
Setting up the build environment
================================

Docker and Docker Compose
-------------------------

The smartnic firmware build depends on `docker` and the `docker compose` plugin.

Install Docker on your system following the instructions found here for the **linux** variant that you are using
* https://docs.docker.com/engine/install/

Ensure that you follow the post-install instructions here so that you can run docker **without sudo**
* https://docs.docker.com/engine/install/linux-postinstall/

Verify your docker setup by running this as an ordinary (non-root) user without using `sudo`
```
docker run hello-world
```

Install Docker Compose on your system following the instructions found here
* https://docs.docker.com/compose/cli-command/#install-on-linux

Verify your docker compose installation by running this as an ordinary (non-root) user without using `sudo`
```
docker compose version
```

Git Submodules
--------------
Ensure that all submodules have been pulled.

```
git submodule init
git submodule update
```

Building a new firmware image
=============================


Install Smartnic Hardware Build Artifact
----------------------------------------

The firmware build depends on the result of a smartnic hardware (FPGA) build.  This file must be available prior to invoking the firmware build.

This file will be called `artifacts.esnet-smartnic-hw.export_hwapi.<version>.zip` and should be placed in the `sn-hw` directory in your source tree before starting the firmware build.

Set up your .env file
---------------------

The `.env` file tells the build about its inputs and outputs.

```
cd $(git rev-parse --show-toplevel)
cat <<_EOF > .env
# The <version> (or pipeline ID for CI builds) of the hardware build
SN_HW_VER=15479
 
# The version number which will be assigned to the result of the firmware build
# Must only contain the characters in [a-zA-Z0-9.+~] (excluding the [])
SN_FW_VER=manual~001
 
# The runtime operating system that will be running the firmware
# If unsure, leave this unchanged
OS_CODENAME=focal
EOF
```

Build the firmware
------------------

The firmware build happens inside of a docker container which manages all of the build-time dependencies and tools.

```
cd $(git rev-parse --show-toplevel)
docker compose build
docker compose run --rm sn-fw-pkg
```

The firmware build produces its output files in the `sn-stack/debs` directory where you'll find files similar to these
```
cd $(git rev-parse --show-toplevel)
$ tree sn-stack/debs/
sn-stack/debs/
└── focal
    ├── esnet-smartnic_1.0.0-manual~001_amd64.buildinfo
    ├── esnet-smartnic_1.0.0-manual~001_amd64.changes
    ├── esnet-smartnic1_1.0.0-manual~001_amd64.deb
    ├── esnet-smartnic1-dbgsym_1.0.0-manual~001_amd64.ddeb
    └── esnet-smartnic-dev_1.0.0-manual~001_amd64.deb
```

These files will be used to customize the smartnic runtime environment and the `esnet-smartnic-dev_*` packages can also be used in your application software development environment.  For further details about the contents of these `.deb` files, see `README.fw.artifacts`.

The entire `sn-stack` directory will need to be transferred to the runtime system.

```
cd $(git rev-parse --show-toplevel)
zip artifacts.esnet-smartnic-fw.package_focal.manual~001.zip sn-stack
```


Setting up the Runtime Environment
==================================

The smartnic runtime environment also requires `docker` and `docker compose` as described above.

Set up Xilinx Labtools
----------------------

In order to load a smartnic FPGA bitfile into the Xilinx U280 card, we need to make use of the Xilinx Labtools.  The instructions for setting up labtools can be found in a separate repository.  That repository will provide us with a docker container populated with the xilinx labtools.  That docker container will be used to program the FPGA bitfile.

Running the firmware
--------------------

```
unzip artifacts.esnet-smartnic-fw.package_focal.manual~001.zip
cd sn-stack
# edit the .env file to provide sane values for
#    FPGA_PCIE_DEV=0000:65:00
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
