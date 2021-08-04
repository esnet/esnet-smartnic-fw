#include <stdint.h>
#include <stdbool.h>

extern bool sdnetio_reg_write(uintptr_t sdnet_base_addr, uintptr_t offset, uint32_t data);
extern bool sdnetio_reg_read(uintptr_t sdnet_base_addr, uintptr_t offset, uint32_t * data);

