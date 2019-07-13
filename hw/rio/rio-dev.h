#ifndef HW_RIO_RIO_DEV_H
#define HW_RIO_RIO_DEV_H

#include <stdint.h>

#include "exec/hwaddr.h"

#include "hw/rio/rio.h"
#include "hw/rio/packet.h"

typedef int (csr_read32_accessor_t)(void *dev, hwaddr addr, uint32_t *val);
typedef int (csr_write32_accessor_t)(void *dev, hwaddr addr,
                                     uint32_t val, uint32_t mask);

void rio_cfg_space_read(void *dev, csr_read32_accessor_t *csr_read32,
                        RioPkt *req, RioPkt *resp);
void rio_cfg_space_write(void *dev, csr_write32_accessor_t *csr_write32,
                        RioPkt *req, RioPkt *resp);

#endif // HW_RIO_RIO_DEV_H
