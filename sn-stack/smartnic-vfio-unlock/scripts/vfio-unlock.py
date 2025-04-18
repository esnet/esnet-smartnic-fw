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
import uuid

SYSFS_BUS_PCI_DEVICES = "/sys/bus/pci/devices"

@click.command()
@click.option('--pf',
              help="Select a specific PCIe device physical function (domain:bus:device.function)",
              default=["0000:d8:00.0", "0000:d8:00.1"],
              multiple=True,
              show_default=True)
@click.option('--vf',
              help="Select a specific PCIe device virtual function (domain:bus:device.function)",
              default=[],
              multiple=True,
              show_default=True)
@click.option('-b', '--bar',
              help="Select which PCIe bar to use for IO",
              default=2,
              show_default=True)
@click.option('--vf-token',
              help="VF token to be used to authenticate VFs with vfio-pci driver.  Autogenerated if not provided.")
def main(pf, vf, bar, vf_token):
    # Build up the mapping between device address and its associated IOMMU group name
    print("Identifying IOMMU group for each device...")
    device_addr_to_iommu_group_number = {}
    for device in pf + vf:
        # Make sure the device exists
        device_path = Path(SYSFS_BUS_PCI_DEVICES) / device
        if not device_path.exists():
            print("ERROR: Device", device, "does not exist at", device_path, ".  Is the FPGA loaded and the PCIe bus rescanned?")
            sys.exit(1)
        # Make sure the selected bar is valid for the device
        resource_path = device_path / "resource{:d}".format(bar)
        if not resource_path.exists():
            print("ERROR: Selected bar does not exist for the device", device, ".  Is the FPGA loaded?")
            sys.exit(1)

        # Identify which vfio group this device is in
        iommu_group_link = device_path / "iommu_group"
        if not iommu_group_link.exists():
            print("ERROR: Device", device, "does not appear to be in an iommu_group.  Is the IOMMU enabled on your system?")
            sys.exit(1)
        if not iommu_group_link.is_symlink():
            print("ERROR: The iommu_group for the device", device, "does not appear to link to an iommu_group")
            sys.exit(1)
        iommu_group_number  = Path(os.readlink(iommu_group_link)).name
        print("\tDevice", device, "is a member of IOMMU group #", iommu_group_number)

        device_addr_to_iommu_group_number[device] = iommu_group_number
    print()

    # Compute the set of unique IOMMU groups that covers all selected devices by inverting the addr -> group map
    iommu_group_number_to_devices = {}
    for addr,group in device_addr_to_iommu_group_number.items():
        if group in iommu_group_number_to_devices:
            iommu_group_number_to_devices[group].append(addr)
        else:
            iommu_group_number_to_devices[group] = [addr]

    # Ensure that the vfio module's device exists -- confirms that the module is loaded/active
    vfio_dev_path = Path("/dev/vfio/vfio")
    if not vfio_dev_path.exists():
        print("ERROR: The vfio device driver does not appear to be loaded.")
        sys.exit(1)

    # Open fds for all of the required groups
    vfio_group_number_to_group_fd = {}
    noiommu_fallback_required = False
    print("Checking viability of all IOMMU groups...")
    for iommu_group_number in iommu_group_number_to_devices.keys():
        print("\tChecking IOMMU group", iommu_group_number)
        vfio_group_path = Path("/dev/vfio") / iommu_group_number
        if not vfio_group_path.exists():
            print("\t\tNo vfio device at", vfio_group_path, "attempting fallback to NOIOMMU vfio device name")
            vfio_group_path = Path("/dev/vfio") / 'noiommu-{}'.format(iommu_group_number)
            if not vfio_group_path.exists():
                print("ERROR: No vfio device exists for this device's group.")
                sys.exit(1)
            noiommu_fallback_required = True

        group_fd = vfio_group_path.open('wb')
        vfio_group_number_to_group_fd[iommu_group_number] = group_fd

        group_status = VfioGroupStatus(argsz=ctypes.sizeof(VfioGroupStatus))
        fcntl.ioctl(group_fd, vfio.VFIO_GROUP_GET_STATUS, group_status)
        print("\t\tVFIO_GROUP_STATUS: {}".format(group_status.flags))
        if not group_status.flags & vfio.VFIO_GROUP_FLAGS_VIABLE:
            print("\t\tVFIO_GROUP is not viable")
        else:
            print("\t\tVFIO_GROUP is viable")
    print()

    # Create a new vfio container and add all of the required groups to it
    print("Configuring VFIO container...")
    container_fd = vfio_dev_path.open('wb')
    vfio_api_version = fcntl.ioctl(container_fd, vfio.VFIO_GET_API_VERSION)
    print("\tVFIO_API_VERSION: {}".format(vfio_api_version))

    if not fcntl.ioctl(container_fd, vfio.VFIO_CHECK_EXTENSION, vfio.VFIO_TYPE1_IOMMU):
        print("\tVFIO_TYPE1_IOMMU is not supported.")
    else:
        print("\tVFIO_TYPE1_IOMMU is supported.")

    if not fcntl.ioctl(container_fd, vfio.VFIO_CHECK_EXTENSION, vfio.VFIO_NOIOMMU_IOMMU):
        print("\tVFIO_NOIOMMU_IOMMU is not supported.")
    else:
        print("\tVFIO_NOIOMMU_IOMMU is supported.")
    print()

    # For each referenced IOMMU group, attempt to add it to our vfio container
    print("\tAssembling all IOMMU groups into VFIO container...")
    for iommu_group_number, device_list in iommu_group_number_to_devices.items():
        print("\t\tAdding group", iommu_group_number)
        group_fd = vfio_group_number_to_group_fd[iommu_group_number]

        # Set the CONTAINER for the GROUP to unlock the SET_IOMMU ioctl
        fcntl.ioctl(group_fd, vfio.VFIO_GROUP_SET_CONTAINER, ctypes.c_int(container_fd.fileno()))

    # Set the IOMMU type for the CONTAINER to unlock the device ioctls
    if noiommu_fallback_required:
        # At least one of the groups is a noiommu group, force IOMMU type to "NOIOMMU"
        print("\tSetting container IOMMU type to: NOIOMMU")
        fcntl.ioctl(container_fd, vfio.VFIO_SET_IOMMU, VFIO_NOIOMMU_IOMMU)
    else:
        # All of the groups are regular IOMMU groups, use the TYPE1 IOMMU
        print("\tSetting container IOMMU type to: TYPE1")
        fcntl.ioctl(container_fd, vfio.VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU)
    print()

    # Ensure that we are ready with a VF Token when required
    print("Setting up VF Token")
    if vf_token is not None:
        # User is expected to pass a UUID4 formatted UUID
        vf_token_uuid = uuid.UUID(vf_token)
    else:
        print("\tNo VF token provided, autogenerating one.")
        vf_token_uuid = uuid.uuid4()
    print("\tUsing VF Token UUID: {}".format(vf_token_uuid))
    print()

    # Open and configure each device, then dump out all available info
    #
    # NOTE: Order matters here.  Process PFs *first* in the list before VFs to ensure that the VF Token has been set on PFs first.
    for device_addr in pf + vf:
        iommu_group_number = device_addr_to_iommu_group_number[device_addr]

        # get the related group fd for this device
        group_fd = vfio_group_number_to_group_fd[iommu_group_number]

        if device_addr in pf:
            # This is a PF, set the VF token IFF the user has passed in VFs
            print("Opening PF Device {} in IOMMU group {}".format(device_addr, iommu_group_number))

            device_fd = fcntl.ioctl(group_fd,
                                    vfio.VFIO_GROUP_GET_DEVICE_FD,
                                    ctypes.create_string_buffer(bytes(device_addr, 'utf-8')))
            if len(vf) > 0:
                # Only try to set the VF Token if the user has passed in VFs
                # This is checked so that we don't try this ioctl on systems that don't support it
                # e.g. before linux 5.6
                print("\tConfiguring VF Token {}".format(vf_token_uuid))
                device_feature = VfioDeviceFeature(argsz=ctypes.sizeof(VfioDeviceFeature),
                                                   flags = (VFIO_DEVICE_FEATURE_SET |
                                                            VFIO_DEVICE_FEATURE_PCI_VF_TOKEN),
                                                   data = VfioDeviceFeatureData(*vf_token_uuid.bytes))
                fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_FEATURE, device_feature)
        else:
            # This is a VF, use the VF token when requesting the device fd
            print("Opening VF Device {} in IOMMU group {} Using VF Token {}".format(device_addr, iommu_group_number, vf_token_uuid))

            device_and_token = device_addr + " vf_token=" + str(vf_token_uuid)
            device_fd = fcntl.ioctl(group_fd,
                                    vfio.VFIO_GROUP_GET_DEVICE_FD,
                                    ctypes.create_string_buffer(bytes(device_and_token, 'utf-8')))

        device_info = VfioDeviceInfo(argsz=ctypes.sizeof(VfioDeviceInfo))
        fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_INFO, device_info)
        print("\targsz = {}".format(device_info.argsz))
        print("\tflags = {}".format(device_info.flags))
        print("\tnum_regions = {}".format(device_info.num_regions))
        print("\tnum_irqs = {}".format(device_info.num_irqs))

        print("\tFlags")
        flags = (
            ("Reset",         vfio.VFIO_DEVICE_FLAGS_RESET),
            ("vfio-pci",      vfio.VFIO_DEVICE_FLAGS_PCI),
            ("vfio-platform", vfio.VFIO_DEVICE_FLAGS_PLATFORM),
            ("vfio-amba",     vfio.VFIO_DEVICE_FLAGS_AMBA),
            ("vfio-ccw",      vfio.VFIO_DEVICE_FLAGS_CCW),
            ("vfio-ap",       vfio.VFIO_DEVICE_FLAGS_AP),
            ("vfio-fsl-mc",   vfio.VFIO_DEVICE_FLAGS_FSL_MC),
            ("Capabilities",  vfio.VFIO_DEVICE_FLAGS_CAPS),
            ("vfio-cdx",      vfio.VFIO_DEVICE_FLAGS_CDX),
        )
        for label, flag in flags:
            print("\t\t{:14s}: {}".format(label, device_info.flags & flag == flag))

        print("\tRegions")
        for region_index in range(0,device_info.num_regions-1):
            region_info = VfioRegionInfo(argsz=ctypes.sizeof(VfioRegionInfo), index=region_index)
            fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_REGION_INFO, region_info)
            print("\t\t[{}] argsz = {}   flags = {}".format(region_info.index, region_info.argsz, region_info.flags))
            print("\t\t\tcap_offset = {}".format(region_info.cap_offset))
            print("\t\t\tsize = {}".format(region_info.size))
            print("\t\t\toffset = {}".format(region_info.offset))
            if region_info.flags & vfio.VFIO_REGION_INFO_FLAG_CAPS and region_info.cap_offset != 0:
                cap_offset = region_info.cap_offset - 32
                cap_p = ctypes.cast(region_info.cap_raw, ctypes.POINTER(VfioCapability))
                print("\t\t\tCapabilities")
                while True:
                    cap = cap_p[0]
                    print("\t\t\t\tid = {}".format(cap.id))
                    print("\t\t\t\tversion = {}".format(cap.version))
                    print("\t\t\t\tnext = {}".format(cap.next))
                    if cap.id == vfio.VFIO_REGION_INFO_CAP_SPARSE_MMAP:
                        sparse_p = ctypes.cast(cap.data, ctypes.POINTER(VfioRegionInfoCapSparseMmap))
                        sparse = sparse_p[0]
                        print(sparse.nr_areas)
                        for i in range(0, sparse.nr_areas):
                            print(sparse.areas[i].offset, sparse.areas[i].size)
                    if cap.next == 0:
                        # end of the capability chain
                        break

        print("\tIRQs")
        for irq_index in range(0, device_info.num_irqs):
            irq_info = VfioIrqInfo(argsz=ctypes.sizeof(VfioIrqInfo), index=irq_index)
            fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_IRQ_INFO, irq_info)
            print("\t\t[{}] argsz = {}   flags = {}".format(irq_info.index, irq_info.argsz, irq_info.flags))
            print("\t\t\tcount={}".format(irq_info.count))

        # Read from config memory
        cfgmem_info = VfioRegionInfo(argsz=ctypes.sizeof(VfioRegionInfo), index=VFIO_PCI_CONFIG_REGION_INDEX)
        fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_REGION_INFO, cfgmem_info)

        vendor_id, device_id = struct.unpack_from("<HH", os.pread(device_fd, 4, cfgmem_info.offset + 0))
        print("\tPCIe Device Config Space")
        print("\t\tVendor ID = {:02x}".format(vendor_id))
        print("\t\tDevice ID = {:02x}".format(device_id))

        # Read out smartnic version info from bar2
        bar2_info = VfioRegionInfo(argsz=ctypes.sizeof(VfioRegionInfo), index=VFIO_PCI_BAR2_REGION_INDEX)
        fcntl.ioctl(device_fd, vfio.VFIO_DEVICE_GET_REGION_INFO, bar2_info)
        m = mmap.mmap(device_fd, bar2_info.size, prot=mmap.PROT_READ | mmap.PROT_WRITE, offset=bar2_info.offset)
        mv = memoryview(m)

        # Read the entire syscfg block
        syscfg = mv[0:44].cast("@I")
        print("\tDevice Version Info")
        print("\t\tDNA          = 0x{:08x}{:08x}{:08x}".format(syscfg[10],syscfg[9],syscfg[8]))
        print("\t\tUSR_ACCESS   = 0x{:08x} ({:d})".format(syscfg[7], syscfg[7]))
        print("\t\tBUILD_STATUS = 0x{:08x}".format(syscfg[0]))
        print()

    while True:
        time.sleep(60)

if __name__ == "__main__":
    main(auto_envvar_prefix='VFIO_UNLOCK')
