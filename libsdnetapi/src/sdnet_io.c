#include "sdnet_io.h"
#ifdef SDNETIO_DEBUG
#include <stdio.h>		/* printf */
#endif

uint64_t sdnetio_reg_read_ctr = 0;
uint64_t sdnetio_reg_write_ctr = 0;

bool sdnetio_reg_write(uintptr_t sdnet_base_addr, uintptr_t offset, uint32_t data) {
  uint32_t * reg = (uint32_t *)(sdnet_base_addr + offset);

#ifdef SDNETIO_DEBUG
  printf("> %08lx: %08x\n", offset, data);
#endif
  *reg = data;

  sdnetio_reg_write_ctr++;

  return true;
}

bool sdnetio_reg_read(uintptr_t sdnet_base_addr, uintptr_t offset, uint32_t * data) {
  uint32_t * reg = (uint32_t *)(sdnet_base_addr + offset);

  *data = *reg;
#ifdef SDNETIO_DEBUG
  printf("< %08lx: %08x\n", offset, *data);
#endif

  sdnetio_reg_read_ctr++;

  return true;
}
