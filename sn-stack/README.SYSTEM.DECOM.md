# Purpose and context for this document

This document is intended to provide a set of instructions that can unwind the steps executed in the `README.SYSTEM.SETUP.md` file.

You can use this document to return the FPGA cards to a factory image and to remove/deactivate any ESnet SmartNIC specific setup from the system.  You may also be asked to follow these steps when major disruptive changes have been introduced between versions of the ESnet SmartNIC platform where FPGA flash reprogramming is required.

# Stop all running SmartNIC Application Stacks

Any running SmartNIC Application stacks on the system will be holding kernel references to the FPGA card PCIe devices.  This can obstruct our ability to uninstall and reflash devices.

There are broadly two kinds of SmartNIC Application stacks that might be running on a server
1. Officially managed, stacks
   * These are typically installed and managed under `/usr/local/smartnic/{0..9}`
   * Stopping these is a well-defined operation and can be done by becoming the `smartnic` user
2. Unofficial, user-run stacks
   * If this system also hosts development or unofficial stacks, they can be installed and running out of a user's home directory
   * Stopping these may require coordination with other users or becoming those users and stopping them directly

During this step, we will need *all* stacks to be stopped, regardless of whether they are offically managed or unmanaged stacks.

## Stopping all officially managed stacks

Officially managed SmartNIC application stacks all run out of `/usr/local/smartnic/{0..9}` which should be owned by the `smartnic` user.  Become the `smartnic` user and bring down all configured application stacks.
``` bash
sudo -i -u smartnic
for i in {0..9} ; do
  d=/usr/local/smartnic/${i}/active
  [ -d ${d} ] || continue
  echo "== Stopping stack ${i} =="
  docker compose --project-directory ${d} down --remove-orphans
done
exit
```

## Disable all systemd on-boot automatic stack restart jobs for managed stacks

In the previous step, we stopped any *currently* running, officially managed application stacks.  In this step, we will additionally prevent these stacks from being automatically restarted on the next boot.

List all automatic on-boot stack restart jobs configured on this system
``` bash
$ sudo systemctl list-units --plain --legend=no --all 'smartnic-stack-restart@*'
smartnic-stack-restart@1.service loaded inactive dead Restart ESnet SmartNIC stack 1
smartnic-stack-restart@2.service loaded inactive dead Restart ESnet SmartNIC stack 2
smartnic-stack-restart@4.service loaded inactive dead Restart ESnet SmartNIC stack 4
smartnic-stack-restart@5.service loaded inactive dead Restart ESnet SmartNIC stack 5
```
(example from a system with 4 active SmartNIC stacks running in slots 1,2,4,5 -- your output may vary)

Disable all `smartnic-stack-restart@` on-boot jobs so that the managed stacks are not restarted on the next boot.
``` bash
sudo systemctl disable smartnic-stack-restart@
```

## Identifying any remaining, unofficial user-run stacks

List all remaining application stacks started by `docker compose`
``` bash
docker compose ls
```

This should print one line for each application stack that is running on this host.  These will typically be in the form
```
NAME                 STATUS          CONFIG FILES
sn-stack-<userid>    running(8)      /home/<userid>/some/path/docker-compose.yml,...
```
Where `<userid>` here is generally the userid of whoever started the stack.  The path under `CONFIG FILES` indicates the directory where this stack was started.

You can either ask this user to stop the stack on their own, or become that user and stop it yourself if that is an appropriate step.

``` bash
sudo -i -u userid docker compose --project-directory /home/userid/some/path down --remove-orphans
```

## Confirm that all SmartNIC application stacks are down

List all remaining application stacks started by `docker compose`
``` bash
docker compose ls
```

**NOTE** Do NOT proceed until all running application stacks have been stopped.

# Wipe all FPGA cards to factory image

Unbind all FPGA cards from the `vfio-pci` driver to allow them to come out of reset (in case they were previously managed by a smartnic stack)
``` bash
for i in $(lspci -Dd 10ee: | awk -F' ' '{ print $1 }' | grep '0$') ; do echo "$i" | sudo tee /sys/bus/pci/drivers/vfio-pci/unbind ; done
```

Wipe the user image from all FPGA cards using `xbflash2`

This step requires that you obtain the `xbflash2` program.  This is typically provided as part of the bootstrap zip file that was used during initial server commissioning.  Refer to the "Obtain SmartNIC Bootstrap Zip file" section of `README.SYSTEM.SETUP.md`.
``` bash
#unzip artifacts.esnet-smartnic-fw.bootstrap.0.zip
#unzip artifacts.esnet-smartnic-fw.[au280|au55c].ejfat.bootstrap.<build#>.zip
cd sn-bootstrap
for card_addr in $(lspci -Dd 10ee: | awk -F' ' '{ print $1 }' | grep '0$') ; do
  printf "\n" | sudo ./xbflash2 program --spi --revert-to-golden --bar 2 --bar-offset 0x20000 --device $card_addr
done
```

# Purge the `smartnic-system-setup` deb package

Purge the `smartnic-system-setup` deb package
``` bash
sudo apt purge smartnic-system-setup
```

# Remove `/usr/local/smartnic` directory tree

This step is **optional** and should only be done after confirming that any important data and configuration files have been safely backed up.

**NOTE** No command is provided in this section to avoid accidental file wiping.

# Remove the `smartnic` user
``` bash
sudo deluser --remove-home smartnic
```

# Cold Boot the Server

``` bash
sudo sync ; sleep 10 ; sudo ipmitool chassis power cycle
```

# Conclusion

Congratulations, you're all done decommissioning this server.  The FPGA cards should now be back to their factory flash image, and all active components of the ESnet SmartNIC platorm setup files should be removed.  At your discretion, you may have left some user-installed files under `/usr/local/smartnic`, but the system should be back to as close to a pristine state as possible.
