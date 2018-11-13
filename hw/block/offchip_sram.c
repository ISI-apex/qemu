
/*
  OFFCHIP SRAM emulation.

 */

#ifndef OFFCHIP_SRAM

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/block/flash.h"
#include "sysemu/block-backend.h"
#include "hw/qdev.h"
#include "qapi/error.h"
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
    DeviceState parent_obj;
    MemoryRegion mm;
    uint32_t size;
    uint8_t *ioaddr;
    uint64_t addr;
    BlockBackend *blk;
};

#define TYPE_OFFCHIP_SRAM "offchip-sram"

#define OFFCHIP_SRAM(obj) \
    OBJECT_CHECK(OFFCHIP_SRAMState, (obj), TYPE_OFFCHIP_SRAM)

static void offchip_sram_reset(DeviceState *dev)
{
    OFFCHIP_SRAMState *s = OFFCHIP_SRAM(dev);
    s->addr = 0;
}

static const VMStateDescription vmstate_offchip_sram = {
    .name = "offchip_sram",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(addr, OFFCHIP_SRAMState),
        /* XXX: do we want to save s->storage too? */
        VMSTATE_END_OF_LIST()
    }
};

static void offchip_sram_realize(DeviceState *dev, Error **errp)
{
    OFFCHIP_SRAMState *s = OFFCHIP_SRAM(dev);
    int ret;

    if (s->blk) {
        if (blk_is_read_only(s->blk)) {
            error_setg(errp, "Can't use a read-only drive");
            return;
        }
        ret = blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                           BLK_PERM_ALL, errp);
        if (ret < 0) {
            return;
        }
    } 
}

static Property offchip_sram_properties[] = {
    DEFINE_PROP_UINT32("size", OFFCHIP_SRAMState, size, 0),
    DEFINE_PROP_DRIVE("drive", OFFCHIP_SRAMState, blk),
    DEFINE_PROP_END_OF_LIST(),
};

static void offchip_sram_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = offchip_sram_realize;
    dc->reset = offchip_sram_reset;
    dc->vmsd = &vmstate_offchip_sram;
    dc->props = offchip_sram_properties;
}

static const TypeInfo offchip_sram_info = {
    .name          = TYPE_OFFCHIP_SRAM,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(OFFCHIP_SRAMState),
    .class_init    = offchip_sram_class_init,
};

static void offchip_sram_register_types(void)
{
    type_register_static(&offchip_sram_info);
}

void offchip_sram_write (void *opaque, hwaddr addr, uint64_t value64,
                       unsigned int size)
{
    OFFCHIP_SRAMState *s = OFFCHIP_SRAM(opaque);
    assert (size < sizeof(uint64_t)); 
    blk_pwrite(s->blk, addr, &value64, size, 0);
}

uint64_t offchip_sram_read (void *opaque, hwaddr addr,
                         unsigned int size)
{
    uint64_t value64;
    OFFCHIP_SRAMState *s = OFFCHIP_SRAM(opaque);
    assert (size < sizeof(uint64_t)); 
    
    blk_pread(s->blk, addr, &value64, size);
    return value64;
}

static const MemoryRegionOps offchip_sram_ops = {
    .read = offchip_sram_read,
    .write = offchip_sram_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};

DeviceState *offchip_sram_init(BlockBackend *blk, uint32_t size)
{
    DeviceState *dev;

    if (size == 0) {
        hw_error("%s: Unsupported OFFCHIP_SRAM size.\n", __FUNCTION__);
    }
    dev = DEVICE(object_new(TYPE_OFFCHIP_SRAM));
    qdev_prop_set_uint8(dev, "size", size);
    if (blk) {
        qdev_prop_set_drive(dev, "drive", blk, &error_fatal);
    }

    qdev_init_nofail(dev);
/*
    memory_region_init_io(&sdev->mm, OBJECT(sdev), &offchip_sram_ops, sdev, "pl35x.offchip_sram",
                          (size == 0 ? 1 << 28 : size)); // 256 GB NAND
*/
    return dev;
}

type_init(offchip_sram_register_types)

#else


# undef PAGE_SIZE
# undef PAGE_SHIFT
# undef PAGE_SECTORS
# undef ADDR_SHIFT
#endif	/* OFFCHIP_SRAM_IO */
