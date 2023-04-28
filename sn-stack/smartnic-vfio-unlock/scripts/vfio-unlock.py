#!/usr/bin/env python3

import click
from pathlib import Path
import mmap
import sys
import struct
import os
import fcntl
from vfio import *
import time
import ctypes

SYSFS_BUS_PCI_DEVICES = "/sys/bus/pci/devices"

@click.command()
@click.option('-s', '--select',
              help="Select a specific PCIe device (domain:bus:device.function)",
              default="0000:d8:00.0",
              show_default=True)
@click.option('-b', '--bar',
              help="Select which PCIe bar to use for IO",
              default=2,
              show_default=True)
def main(select, bar):
    # Locate our target device
    device_path = Path(SYSFS_BUS_PCI_DEVICES) / select
    if not device_path.exists():
        print("ERROR: Selected device does not exist.  Is the FPGA loaded and the PCIe bus rescanned?")
        sys.exit(1)

    # Identify which vfio group this device is in
    iommu_group_link = device_path / "iommu_group"
    if not iommu_group_link.exists():
        print("ERROR: Selected device does not appear to be in an iommu_group.  Is the IOMMU enabled on your system?")
        sys.exit(1)
    if not iommu_group_link.is_symlink():
        print("ERROR: The iommu_group for the selected device does not appear to link to an iommu_group")
        sys.exit(1)
    iommu_group_num  = Path(os.readlink(iommu_group_link)).name

    # Ensure that the vfio module's device exists -- confirms that the module is loaded/active
    vfio_dev_path = Path("/dev/vfio/vfio")
    if not vfio_dev_path.exists():
        print("ERROR: The vfio device driver does not appear to be loaded.")
        sys.exit(1)

    vfio_group_path = Path("/dev/vfio") / iommu_group_num
    if not vfio_group_path.exists():
        print("ERROR: No vfio device exists for this device's group.")
        sys.exit(1)

    resource_path = device_path / "resource{:d}".format(bar)
    if not resource_path.exists():
        print("ERROR: Selected bar does not exist for the selected device.  Is the FPGA loaded?")
        sys.exit(1)

    # Create a new vfio container
    with vfio_dev_path.open('wb') as container_fd:
        vfio_api_version = fcntl.ioctl(container_fd, vfio.VFIO_GET_API_VERSION)
        print("VFIO_API_VERSION: {}".format(vfio_api_version))

        if not fcntl.ioctl(container_fd, vfio.VFIO_CHECK_EXTENSION, vfio.VFIO_TYPE1_IOMMU):
            print("VFIO_TYPE1_IOMMU is not supported!")
        else:
            print("VFIO_TYPE1_IOMMU is supported.")

        with vfio_group_path.open('wb') as group_fd:
            group_status = VfioGroupStatus(argsz=ctypes.sizeof(VfioGroupStatus))
            fcntl.ioctl(group_fd, vfio.VFIO_GROUP_GET_STATUS, group_status)
            print("VFIO_GROUP_STATUS: {}".format(group_status.flags))
            if not group_status.flags & vfio.VFIO_GROUP_FLAGS_VIABLE:
                print("VFIO_GROUP is not viable")
            else:
                print("VFIO_GROUP is viable")

            # Set the CONTAINER for the GROUP to unlock the SET_IOMMU ioctl
            fcntl.ioctl(group_fd, vfio.VFIO_GROUP_SET_CONTAINER, ctypes.c_int(container_fd.fileno()))
            # Set the IOMMU type for the CONTAINER to unlock the device ioctls
            fcntl.ioctl(container_fd, vfio.VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)

            # Open a DEVICE fd
            device_fd = fcntl.ioctl(group_fd, vfio.VFIO_GROUP_GET_DEVICE_FD, ctypes.create_string_buffer(bytes(select,'utf-8')))

            device_info = VfioDeviceInfo(argsz=ctypes.sizeof(VfioDeviceInfo))
            fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_INFO, device_info)
            print("Device Info")
            print("\targsz = {}".format(device_info.argsz))
            print("\tflags = {}".format(device_info.flags))
            print("\tnum_regions = {}".format(device_info.num_regions))
            print("\tnum_irqs = {}".format(device_info.num_irqs))
            print()
            print("Regions")
            for region_index in range(0,device_info.num_regions-1):
                region_info = VfioRegionInfo(argsz=ctypes.sizeof(VfioRegionInfo), index=region_index)
                fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_REGION_INFO, region_info)
                print("\t[{}] argsz = {}   flags = {}".format(region_info.index, region_info.argsz, region_info.flags))
                print("\t\tcap_offset = {}".format(region_info.cap_offset))
                print("\t\tsize = {}".format(region_info.size))
                print("\t\toffset = {}".format(region_info.offset))
                if region_info.flags & vfio.VFIO_REGION_INFO_FLAG_CAPS and region_info.cap_offset != 0:
                    cap_offset = region_info.cap_offset - 32
                    cap_p = ctypes.cast(region_info.cap_raw, ctypes.POINTER(VfioCapability))
                    print("\t\tCapabilities")
                    while True:
                        cap = cap_p[0]
                        print("\t\t\tid = {}".format(cap.id))
                        print("\t\t\tversion = {}".format(cap.version))
                        print("\t\t\tnext = {}".format(cap.next))
                        if cap.id == vfio.VFIO_REGION_INFO_CAP_SPARSE_MMAP:
                            sparse_p = ctypes.cast(cap.data, ctypes.POINTER(VfioRegionInfoCapSparseMmap))
                            sparse = sparse_p[0]
                            print(sparse.nr_areas)
                            for i in range(0, sparse.nr_areas):
                                print(sparse.areas[i].offset, sparse.areas[i].size)
                        if cap.next == 0:
                            # end of the capability chain
                            break
            print()
            print("IRQs")
            for irq_index in range(0, device_info.num_irqs):
                irq_info = VfioIrqInfo(argsz=ctypes.sizeof(VfioIrqInfo), index=irq_index)
                fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_IRQ_INFO, irq_info)
                print("\t[{}] argsz = {}   flags = {}".format(irq_info.index, irq_info.argsz, irq_info.flags))
                print("\t\tcount={}".format(irq_info.count))
            print()

            # Read from config memory
            cfgmem_info = VfioRegionInfo(argsz=ctypes.sizeof(VfioRegionInfo), index=VFIO_PCI_CONFIG_REGION_INDEX)
            fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_REGION_INFO, cfgmem_info)

            vendor_id, device_id = struct.unpack_from("<HH", os.pread(device_fd, 4, cfgmem_info.offset + 0))
            print("PCIe Device Config Space")
            print("\tVendor ID = {:02x}".format(vendor_id))
            print("\tDevice ID = {:02x}".format(device_id))
            print()

            # Read out smartnic version info from bar2
            bar2_info = VfioRegionInfo(argsz=ctypes.sizeof(VfioRegionInfo), index=VFIO_PCI_BAR2_REGION_INDEX)
            fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_REGION_INFO, bar2_info)
            m = mmap.mmap(device_fd, bar2_info.size, prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=bar2_info.offset)
            mv = memoryview(m)

            # Read the entire syscfg block
            syscfg = mv[0:44].cast("@I")
            print("Device Version Info")
            print("\tDNA          = 0x{:08x}{:08x}{:08x}".format(syscfg[10],syscfg[9],syscfg[8]))
            print("\tUSR_ACCESS   = 0x{:08x} ({:d})".format(syscfg[7], syscfg[7]))
            print("\tBUILD_STATUS = 0x{:08x}".format(syscfg[0]))
            print()

            time.sleep(1000)

if __name__ == "__main__":
    main(auto_envvar_prefix='VFIO_UNLOCK')
