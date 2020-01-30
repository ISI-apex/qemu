/*
  OFFCHIP SRAM emulation.

 */

#ifndef OFFCHIP_SRAM

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/hw.h"
#include "hw/block/flash.h"
#include "sysemu/block-backend.h"
#include "sysemu/blockdev.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/qdev.h"
#include "qemu/error-report.h"

#include "hw/block/offchip_sram.h"

#ifndef OFFCHIP_SRAM_ERR_DEBUG
#define OFFCHIP_SRAM_ERR_DEBUG 1
#endif

#define DB_PRINT_L(...) do { \
    if (OFFCHIP_SRAM_ERR_DEBUG) { \
        qemu_log_mask(DEV_LOG_OFFCHIP_SRAM, ": %s: ", __func__); \
        qemu_log_mask(DEV_LOG_OFFCHIP_SRAM, ## __VA_ARGS__); \
    } \
} while (0);

typedef struct OFFCHIP_SRAMState OFFCHIP_SRAMState;
struct OFFCHIP_SRAMState {
//    DeviceState parent_obj;
    SysBusDevice parent_obj;
    uint32_t rank;	/* index of memory rank */
    uint32_t pflash_index;	/* index of pflash device */
    MemoryRegion iomem;
    BlockBackend *blk;
    uint32_t size;	/* size of SRAM */
};

#define TYPE_OFFCHIP_SRAM "offchip-sram"

#define OFFCHIP_SRAM(obj) \
    OBJECT_CHECK(OFFCHIP_SRAMState, (obj), TYPE_OFFCHIP_SRAM)

static const VMStateDescription vmstate_offchip_sram = {
    .name = "offchip-sram-vm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(size, OFFCHIP_SRAMState),
        /* XXX: do we want to save s->storage too? */
        VMSTATE_END_OF_LIST()
    }
};

void offchip_sram_write(void *opaque, hwaddr addr, uint64_t value64,
                       unsigned int size)
{
    OFFCHIP_SRAMState *s = OFFCHIP_SRAM(opaque);
    assert (size < sizeof(uint64_t));
    if (!s->blk)
	return;
    blk_pwrite(s->blk, addr, &value64, size, 0);
}

uint64_t offchip_sram_read(void *opaque, hwaddr addr,
                         unsigned int size)
{
    uint64_t value64;
    OFFCHIP_SRAMState *s = OFFCHIP_SRAM(opaque);
    assert(size < sizeof(uint64_t));

    if (!s->blk)
	return 0;
    blk_pread(s->blk, addr, &value64, size);
    return value64;
}


const MemoryRegionOps offchip_sram_ops = {
    .read = offchip_sram_read,
    .write = offchip_sram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};

static void offchip_sram_realize(DeviceState *dev, Error **errp)
{
    OFFCHIP_SRAMState*s = OFFCHIP_SRAM(dev);
    memory_region_init_io(&s->iomem, OBJECT(s), &offchip_sram_ops, s,
                          "nvram", s->size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static Property offchip_sram_properties[] = {
    DEFINE_PROP_DRIVE("drive", OFFCHIP_SRAMState, blk),
    DEFINE_PROP_UINT32("pflash-index", OFFCHIP_SRAMState, pflash_index, 0),
    DEFINE_PROP_UINT32("size", OFFCHIP_SRAMState, size, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void offchip_sram_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = offchip_sram_realize;
    dc->vmsd = &vmstate_offchip_sram;
    dc->props = offchip_sram_properties;
}

static const TypeInfo offchip_sram_info = {
    .name          = TYPE_OFFCHIP_SRAM,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(OFFCHIP_SRAMState),
    .class_init    = offchip_sram_class_init,
};

static void offchip_sram_register_types(void)
{
    type_register_static(&offchip_sram_info);
}

type_init(offchip_sram_register_types)

#endif	/* OFFCHIP_SRAM_IO */
