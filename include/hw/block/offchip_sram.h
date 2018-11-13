#ifndef HW_OFFCHIP_SRAM_H
#define HW_OFFCHIP_SRAM_H


DeviceState *offchip_sram_init(BlockBackend *blk, uint32_t size);
uint64_t offchip_sram_read (void *opaque, hwaddr addr, unsigned int size);
void offchip_sram_write (void *opaque, hwaddr addr, uint64_t value64, unsigned int size);
#endif
