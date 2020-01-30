/*
  pl35x-itf.

 */

#ifndef PL35X_ITF 

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

#include "qapi/string-input-visitor.h"
#include "qapi/string-output-visitor.h"

// #include "hw/block/pl35x_itf.h"

#ifndef PL35X_ITF_ERR_DEBUG
#define PL35X_ITF_ERR_DEBUG 1
#endif

#define DB_PRINT_L(...) do { \
    if (PL35X_ITF_ERR_DEBUG) { \
        qemu_log_mask(DEV_LOG_PL35X_ITF, ": %s: ", __func__); \
        qemu_log_mask(DEV_LOG_PL35X_ITF, ## __VA_ARGS__); \
    } \
} while (0);

typedef struct PL35X_ITFState PL35X_ITFState;
struct PL35X_ITFState {
//    DeviceState parent_obj;
    SysBusDevice parent_obj;
    uint32_t interface;	/* interface port number 0 or 1 */
    uint32_t bank_width;/* width of memory */
    uint32_t rank;	/* index of memory rank */
    uint32_t pflash_index;	/* index of pflash device */
    char * dev_type;	/* pflash, sram, nand */
    char * chip;
    BlockBackend *blk;
};

#define TYPE_PL35X_ITF "pl35x-itf"

#define PL35X_ITF(obj) \
    OBJECT_CHECK(PL35X_ITFState, (obj), TYPE_PL35X_ITF)

#if 0
static void pl35x_itf_reset(DeviceState *dev)
{
    PL35X_ITFState *s = PL35X_ITF(dev);
    s->addr = 0;
    bool ambiguous;
    Object * chip = object_resolve_path(s->chip, &ambiguous);
    uint32_t pflash_index = object_property_get_int(chip, "pflash-index", NULL);
    uint32_t id0 = object_property_get_int(chip, "id0", NULL);
    printf("%s: name = %s, pflash_index = %d, id0 = 0x%x\n", __func__, s->chip, pflash_index, id0);
}
#endif

static const VMStateDescription vmstate_pl35x_itf = {
    .name = "pl35x-itf-vm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        /* XXX: do we want to save s->storage too? */
        VMSTATE_END_OF_LIST()
    }
};

const MemoryRegionOps pl35x_itf_ops = {
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};
#if 0
static int pl35x_itf_initfn(SysBusDevice *sbd)
{
    DeviceState *dev = DEVICE(sbd);
    PL35X_ITFState*s = PL35X_ITF(dev);
    Error *local_err = NULL;

    return 0;
    bool ambiguous;
    Object * chip = object_resolve_path(s->chip, &ambiguous);
    uint32_t pflash_index = object_property_get_int(chip, "pflash-index", NULL);

    /* TODO */
    /* if ("sram") { */
    uint32_t id0 = object_property_get_int(chip, "id0", NULL);
    printf("%s: name = %s, pflash_index = %d, id0 = 0x%x\n", __func__, s->chip, pflash_index, id0);

    DriveInfo *dinfo = drive_get_by_index(IF_PFLASH, s->pflash_index);
    if (dinfo) 
        s->blk = blk_by_legacy_dinfo(dinfo); 
    else 
        s->blk = NULL;
    if (s->blk) {
        if (blk_is_read_only(s->blk)) {
            error_report("Can't use a read-only drive");
            return -1;
        }
        qdev_prop_set_drive(dev, "drive", s->blk, &error_fatal);
        blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                           BLK_PERM_ALL, &local_err);
        if (local_err) {
            error_report_err(local_err);
            return -1;
        }
    }
    pflash_cfi01_set_drive(chip, s->blk);
    return 0;
}
#endif

static Property pl35x_itf_properties[] = {
    DEFINE_PROP_DRIVE("drive", PL35X_ITFState, blk),
    DEFINE_PROP_UINT32("rank", PL35X_ITFState, rank, 0),
    DEFINE_PROP_UINT32("interface", PL35X_ITFState, interface, 0),
    DEFINE_PROP_STRING("dev-type", PL35X_ITFState, dev_type),
    DEFINE_PROP_STRING("chip", PL35X_ITFState, chip),
    DEFINE_PROP_END_OF_LIST(),
};

static void pl35x_itf_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
#if 0
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = pl35x_itf_initfn;
    dc->reset = pl34x_itf_reset;
#endif
    dc->vmsd = &vmstate_pl35x_itf;
    dc->props = pl35x_itf_properties;
}

static const TypeInfo pl35x_itf_info = {
    .name          = TYPE_PL35X_ITF,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PL35X_ITFState),
    .class_init    = pl35x_itf_class_init,
};

static void pl35x_itf_register_types(void)
{
    type_register_static(&pl35x_itf_info);
}

type_init(pl35x_itf_register_types)

#endif	/*  PL35X_ITF */
