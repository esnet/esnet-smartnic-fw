# Copyright Notice

ESnet SmartNIC Copyright (c) 2022, The Regents of the University of
California, through Lawrence Berkeley National Laboratory (subject to
receipt of any required approvals from the U.S. Dept. of Energy),
12574861 Canada Inc., Malleable Networks Inc., and Apical Networks, Inc.
All rights reserved.

If you have questions about your rights to use or distribute this software,
please contact Berkeley Lab's Intellectual Property Office at
IPO@lbl.gov.

NOTICE.  This Software was developed under funding from the U.S. Department
of Energy and the U.S. Government consequently retains certain rights.  As
such, the U.S. Government has been granted for itself and others acting on
its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
Software to reproduce, distribute copies to the public, prepare derivative
works, and perform publicly and display publicly, and to permit others to do so.


# Support

The ESnet SmartNIC platform is made available in the hope that it will
be useful to the networking community. Users should note that it is
made available on an "as-is" basis, and should not expect any
technical support or other assistance with building or using this
software. For more information, please refer to the LICENSE.md file in
each of the source code repositories.

The developers of the ESnet SmartNIC platform can be reached by email
at smartnic@es.net.


Setting up the build environment
================================

The smartnic firmware build depends on `docker` and the `docker compose` plugin.

Docker
------

Install Docker on your system following the instructions found here for the **linux** variant that you are using
* https://docs.docker.com/engine/install/

Ensure that you follow the post-install instructions here so that you can run docker **without sudo**
* https://docs.docker.com/engine/install/linux-postinstall/

Verify your docker setup by running this as an ordinary (non-root) user without using `sudo`
```
docker run hello-world
```

Docker Compose
==============

The `docker-compose.yml` file for the smartnic build and the sn-stack depends on features that are only supported in the compose v2 plugin.

Install the `docker compose` plugin like this for a single user:

```
mkdir -p ~/.docker/cli-plugins/
curl -SL https://github.com/docker/compose/releases/download/v2.17.2/docker-compose-linux-x86_64 -o ~/.docker/cli-plugins/docker-compose
chmod +x ~/.docker/cli-plugins/docker-compose
```

Alternatively, you can install the `docker compose` plugin system-wide like this:
```
sudo mkdir -p /usr/local/lib/docker/cli-plugins
sudo curl  -o /usr/local/lib/docker/cli-plugins/docker-compose -SL https://github.com/docker/compose/releases/download/v2.17.2/docker-compose-linux-x86_64
sudo chmod +x /usr/local/lib/docker/cli-plugins/docker-compose
```

Verify your docker compose installation by running this as an ordinary (non-root) user without using `sudo`.  For this install, the version output should be
```
$ docker compose version
Docker Compose version v2.17.2
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

This file will be called `artifacts.<board>.<app_name>.0.zip` and should be placed in the `sn-hw` directory in your source tree before starting the firmware build.


Create 'smartnic-dpdk-docker' Image
-----------------------------------

This docker image can be built locally by cloning the https://github.com/esnet/smartnic-dpdk-docker.git repo and following the README instructions.
Alternatively, this image can be retrieved from a remote registry by setting the SMARTNIC_DPDK_IMAGE_URI variable in the .env file (see below).


Create 'xilinx-labtools-docker' Image
-------------------------------------

This docker image can be built locally by cloning the https://github.com/esnet/xilinx-labtools-docker.git repo and following the README instructions.
Alternatively, this image can be retrieved from a remote registry by setting the LABTOOLS_IMAGE_URI variable in the .env file (see below).


Set up your .env file for building a new firmware image
-------------------------------------------------------

The `.env` file tells the build about its inputs and outputs.

There is an `example.env` file in top level directory of this repo that will provide documentation and examples for the values you need to set.

```
cd $(git rev-parse --show-toplevel)
cp example.env .env
```

Since the values in the .env file are used to locate the correct hardware artifact, you will need to (at least) set these values in the `.env` file to match the exact naming of the .zip file you installed in the previous step:
```
SN_HW_BOARD=<board>
SN_HW_APP_NAME=<app_name>
SN_HW_VER=0
```

Build the firmware
------------------

The firmware build creates a docker container with everything needed to interact with your FPGA image.  Without any parameters, the newly built firmware container will be named/tagged `esnet-smartnic-fw:${USER}-dev` and will be available only on the local system.
```
cd $(git rev-parse --show-toplevel)
./build.sh
```

**Optionally** you can use any alternative name by specifying the full container URI as an optional parameter to the build script like this.  Using a fully specified URI here can be useful if you are planning to push the newly built container to a remote docker registry.
```
./build.sh wharf.es.net/ht/esnet-smartnic-fw:${USER}-dev
```

The build script also automatically customizes the `sn-stack/.env` file to refer exactly to the firmware image that you just built.


The entire `sn-stack` directory will need to be transferred to the runtime system.

```
cd $(git rev-parse --show-toplevel)
zip -r artifacts.esnet-smartnic-fw.package.0.zip sn-stack
```
