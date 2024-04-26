#include <fcntl.h>		/* open */
#include <sys/mman.h>		/* mmap/munmap */
#include <unistd.h>		/* close */
#include <stdio.h>              /* snprintf */

#include "esnet_smartnic_toplevel.h" /* ESNET_SMARTNIC_BAR2_* */

volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_fd(int fd) {
  void * virt_addr = mmap(0, ESNET_SMARTNIC_BAR2_SIZE_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (virt_addr == MAP_FAILED) {
    return NULL;
  }
  return virt_addr;
}

volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_path(const char *path, bool create) {
  int flags = O_RDWR | O_SYNC | O_CLOEXEC;
  if (create) {
    flags |= O_CREAT;
  }

  int fd = open(path, flags);
  if (fd < 0) {
    return NULL;
  }

  if (create && ftruncate(fd, ESNET_SMARTNIC_BAR2_SIZE_BYTES) < 0) {
    return NULL;
  }

  volatile struct esnet_smartnic_bar2 * bar2 = smartnic_map_bar2_by_fd(fd);
  close(fd);

  return bar2;
}

volatile struct esnet_smartnic_bar2 * smartnic_map_bar2_by_pciaddr(const char *addr) {
  char dev_resource_path[80];
  snprintf(dev_resource_path, sizeof(dev_resource_path), "/sys/bus/pci/devices/%s/resource2", addr);
  return smartnic_map_bar2_by_path(dev_resource_path, false);
}

void smartnic_unmap_bar2(volatile struct esnet_smartnic_bar2 * virt_addr) {
  (void) munmap((void *)virt_addr, ESNET_SMARTNIC_BAR2_SIZE_BYTES);
}
