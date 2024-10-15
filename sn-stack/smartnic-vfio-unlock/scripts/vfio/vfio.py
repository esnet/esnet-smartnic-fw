import ctypes
from ioctl_util import IO

# Definitions transcribed from include/uapi/linux/vfio.h in linux
# https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/vfio.h

# API versions
VFIO_API_VERSION = 0

# Extensions
VFIO_TYPE1_IOMMU         = 1
VFIO_SPAPR_TCE_IOMMU     = 2
VFIO_TYPE1v2_IOMMU       = 3
VFIO_DMA_CC_IOMMU        = 4
VFIO_EEH                 = 5
VFIO_TYPE1_NESTING_IOMMU = 6
VFIO_SPAPR_TCE_v2_IOMMU  = 7
VFIO_NOIOMMU_IOMMU       = 8

VFIO_TYPE = ';'
VFIO_BASE = 100

# IOCTLs for VFIO CONTAINER file descriptor (/dev/vfio/vfio)
VFIO_GET_API_VERSION               = IO(VFIO_TYPE, VFIO_BASE + 0)
VFIO_CHECK_EXTENSION               = IO(VFIO_TYPE, VFIO_BASE + 1)
VFIO_SET_IOMMU                     = IO(VFIO_TYPE, VFIO_BASE + 2)

# IOCTLs for GROUP file descriptors (/dev/vfio/$GROUP)
VFIO_GROUP_GET_STATUS              = IO(VFIO_TYPE, VFIO_BASE + 3)
VFIO_GROUP_SET_CONTAINER           = IO(VFIO_TYPE, VFIO_BASE + 4)
VFIO_GROUP_UNSET_CONTAINER         = IO(VFIO_TYPE, VFIO_BASE + 5)
VFIO_GROUP_GET_DEVICE_FD           = IO(VFIO_TYPE, VFIO_BASE + 6)

# IOCTLs for DEVICE file descriptors
VFIO_DEVICE_GET_INFO               = IO(VFIO_TYPE, VFIO_BASE + 7)
VFIO_DEVICE_GET_REGION_INFO        = IO(VFIO_TYPE, VFIO_BASE + 8)
VFIO_DEVICE_GET_IRQ_INFO           = IO(VFIO_TYPE, VFIO_BASE + 9)
VFIO_DEVICE_SET_IRQS               = IO(VFIO_TYPE, VFIO_BASE + 10)
VFIO_DEVICE_RESET                  = IO(VFIO_TYPE, VFIO_BASE + 11)
VFIO_DEVICE_GET_PCI_HOT_RESET_INFO = IO(VFIO_TYPE, VFIO_BASE + 12)
VFIO_DEVICE_PCI_HOT_RESET          = IO(VFIO_TYPE, VFIO_BASE + 13)
VFIO_DEVICE_QUERY_GFX_PLANE        = IO(VFIO_TYPE, VFIO_BASE + 14)
VFIO_DEVICE_GET_GFX_DMABUF         = IO(VFIO_TYPE, VFIO_BASE + 15)
VFIO_DEVICE_IOEVENTFD              = IO(VFIO_TYPE, VFIO_BASE + 16)
VFIO_DEVICE_FEATURE                = IO(VFIO_TYPE, VFIO_BASE + 17)

# IOCTLs for Type1 VFIO IOMMU (on VFIO CONTAINER file descriptor)
VFIO_IOMMU_GET_INFO                = IO(VFIO_TYPE, VFIO_BASE + 12)
VFIO_IOMMU_MAP_DMA                 = IO(VFIO_TYPE, VFIO_BASE + 13)
VFIO_IOMMU_UNMAP_DMA               = IO(VFIO_TYPE, VFIO_BASE + 14)
VFIO_IOMMU_ENABLE                  = IO(VFIO_TYPE, VFIO_BASE + 15)
VFIO_IOMMU_DISABLE                 = IO(VFIO_TYPE, VFIO_BASE + 16)
VFIO_IOMMU_DIRTY_PAGES             = IO(VFIO_TYPE, VFIO_BASE + 17)

class VfioDeviceInfo(ctypes.Structure):
    _fields_ = [
            ('argsz', ctypes.c_uint),
            ('flags', ctypes.c_uint),
            ('num_regions', ctypes.c_uint),
            ('num_irqs', ctypes.c_uint),
   ]

VFIO_DEVICE_FLAGS_RESET    = (1 << 0)	# Device supports reset
VFIO_DEVICE_FLAGS_PCI      = (1 << 1)	# vfio-pci device */
VFIO_DEVICE_FLAGS_PLATFORM = (1 << 2)	# vfio-platform device
VFIO_DEVICE_FLAGS_AMBA     = (1 << 3)	# vfio-amba device
VFIO_DEVICE_FLAGS_CCW      = (1 << 4)	# vfio-ccw device
VFIO_DEVICE_FLAGS_AP       = (1 << 5)	# vfio-ap device
VFIO_DEVICE_FLAGS_FSL_MC   = (1 << 6)	# vfio-fsl-mc device
VFIO_DEVICE_FLAGS_CAPS     = (1 << 7)	# Info supports caps
VFIO_DEVICE_FLAGS_CDX      = (1 << 8)	# vfio-cdx device

# Fixed region and IRQ index mapping
VFIO_PCI_BAR0_REGION_INDEX     = 0
VFIO_PCI_BAR1_REGION_INDEX     = 1
VFIO_PCI_BAR2_REGION_INDEX     = 2
VFIO_PCI_BAR3_REGION_INDEX     = 3
VFIO_PCI_BAR4_REGION_INDEX     = 4
VFIO_PCI_BAR5_REGION_INDEX     = 5
VFIO_PCI_ROM_REGION_INDEX      = 6
VFIO_PCI_CONFIG_REGION_INDEX   = 7

class VfioGroupStatus(ctypes.Structure):
    _fields_ = [
            ('argsz', ctypes.c_uint),
            ('flags', ctypes.c_uint),
    ]
VFIO_GROUP_FLAGS_VIABLE        = (1 << 0)
VFIO_GROUP_FLAGS_CONTAINER_SET = (1 << 1)

class VfioRegionInfo(ctypes.Structure):
    _fields_ = [
            ('argsz', ctypes.c_uint),
            ('flags', ctypes.c_uint),
            ('index', ctypes.c_uint),
            ('cap_offset', ctypes.c_uint),
            ('size', ctypes.c_ulong),
            ('offset', ctypes.c_ulong),
            ('cap_raw', ctypes.c_byte * 256),
    ]
VFIO_REGION_INFO_FLAG_READ  = (1 << 0)
VFIO_REGION_INFO_FLAG_WRITE = (1 << 1)
VFIO_REGION_INFO_FLAG_MMAP  = (1 << 2)
VFIO_REGION_INFO_FLAG_CAPS  = (1 << 3)

class VfioIrqInfo(ctypes.Structure):
    _fields_ = [
            ('argsz', ctypes.c_uint),
            ('flags', ctypes.c_uint),
            ('index', ctypes.c_uint),
            ('count', ctypes.c_uint),
    ]
VFIO_IRQ_INFO_EVENTFD    = (1 << 0)
VFIO_IRQ_INFO_MASKABLE   = (1 << 1)
VFIO_IRQ_INFO_AUTOMASKED = (1 << 2)
VFIO_IRQ_INFO_NORESIZE   = (1 << 3)

class VfioIrqSet(ctypes.Structure):
    _fields_ = [
            ('argsz', ctypes.c_uint),
            ('flags', ctypes.c_uint),
            ('index', ctypes.c_uint),
            ('start', ctypes.c_uint),
            ('count', ctypes.c_uint),
            ('data', ctypes.c_ubyte * 64),
    ]
VFIO_IRQ_SET_DATA_NONE      = (1 << 0)
VFIO_IRQ_SET_DATA_BOOL      = (1 << 1)
VFIO_IRQ_SET_DATA_EVENTFD   = (1 << 2)
VFIO_IRQ_SET_ACTION_MASK    = (1 << 3)
VFIO_IRQ_SET_ACTION_UNMASK  = (1 << 4)
VFIO_IRQ_SET_ACTION_TRIGGER = (1 << 5)

class VfioCapability(ctypes.Structure):
    _fields_ = [
            ('id', ctypes.c_ushort),
            ('version', ctypes.c_ushort),
            ('next', ctypes.c_uint),
            ('data', ctypes.c_ubyte * 64),
    ]

VFIO_REGION_INFO_CAP_SPARSE_MMAP    = 1
VFIO_REGION_INFO_CAP_TYPE           = 2
VFIO_REGION_INFO_CAP_MSIX_MAPPABLE  = 3
VFIO_REGION_INFO_CAP_NVLINK2_SSATGT = 4
VFIO_REGION_INFO_CAP_NVLINK2_LNKSPD = 5

class VfioRegionSparseMmapArea(ctypes.Structure):
    _fields_ = [
            ('offset', ctypes.c_ulong),
            ('size', ctypes.c_ulong),
    ]

class VfioRegionInfoCapSparseMmap(ctypes.Structure):
    _fields_ = [
            ('nr_areas', ctypes.c_uint),
            ('reserved', ctypes.c_uint),
            ('areas', VfioRegionSparseMmapArea*64),
    ]

VfioDeviceFeatureData = ctypes.c_ubyte * 64
class VfioDeviceFeature(ctypes.Structure):
    _fields_ = [
            ('argsz', ctypes.c_uint),
            ('flags', ctypes.c_uint),
            ('data', VfioDeviceFeatureData),
    ]
VFIO_DEVICE_FEATURE_MASK    = (0xffff)  # 16-bit feature index
VFIO_DEVICE_FEATURE_GET     = (1 << 16) # Get feature into data[]
VFIO_DEVICE_FEATURE_SET     = (1 << 17) # Set feature from data[]
VFIO_DEVICE_FEATURE_PROBE   = (1 << 18) # Probe feature support

VFIO_DEVICE_FEATURE_PCI_VF_TOKEN  = 0

