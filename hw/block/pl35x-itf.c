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

#ifndef PL35X_ITF_ERR_DEBUG
#define PL35X_ITF_ERR_DEBUG 0
#endif

#define DB_PRINT_L(...) do { \
    if (PL35X_ITF_ERR_DEBUG) { \
        qemu_log_mask(DEV_LOG_PL35X_ITF, ": %s: ", __func__); \
        qemu_log_mask(DEV_LOG_PL35X_ITF, ## __VA_ARGS__); \
    } \
} while (0);

typedef struct PL35X_ITFState PL35X_ITFState;
struct PL35X_ITFState {
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

static const VMStateDescription vmstate_pl35x_itf = {
    .name = "pl35x-itf-vm",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
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
