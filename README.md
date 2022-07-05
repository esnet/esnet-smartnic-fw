
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

Set up your .env file for building a new firmware image
-------------------------------------------------------

The `.env` file tells the build about its inputs and outputs.

There is an `example.env` file in top level directory of this repo that will provide documentation and examples for the values you need to set.

```
cd $(git rev-parse --show-toplevel)
cp example.env .env
```

Build the firmware
------------------

The firmware build happens inside of a docker container which manages all of the build-time dependencies and tools.

```
cd $(git rev-parse --show-toplevel)
docker compose down -v
docker compose build
docker compose run --rm sn-fw-pkg
docker compose down -v
```

Note: to pick up the very latest Ubuntu packages used in the build environment, you may wish to occasionally run `docker compose build --no-cache` to force the build env to be refreshed rather than (potentially) using the previously cached docker image.

The firmware build produces its output files in the `sn-stack/debs` directory where you'll find files similar to these
```
cd $(git rev-parse --show-toplevel)
$ tree sn-stack/debs/
sn-stack/debs/
└── focal
    ├── esnet-smartnic_1.0.0-user.001_amd64.buildinfo
    ├── esnet-smartnic_1.0.0-user.001_amd64.changes
    ├── esnet-smartnic1_1.0.0-user.001_amd64.deb
    ├── esnet-smartnic1-dbgsym_1.0.0-user.001_amd64.ddeb
    └── esnet-smartnic-dev_1.0.0-user.001_amd64.deb
```

These files will be used to customize the smartnic runtime environment and the `esnet-smartnic-dev_*` packages can also be used in your application software development environment.  For further details about the contents of these `.deb` files, see `README.fw.artifacts`.

The entire `sn-stack` directory will need to be transferred to the runtime system.

```
cd $(git rev-parse --show-toplevel)
zip -r artifacts.esnet-smartnic-fw.package_focal.user.001.zip sn-stack
```
