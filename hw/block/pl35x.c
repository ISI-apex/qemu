/*
 * QEMU model of Primcell PL35X family of memory controllers
 *
 * Copyright (c) 2012 Xilinx Inc.
 * Copyright (c) 2012 Peter Crosthwaite <peter.crosthwaite@xilinx.com>.
 * Copyright (c) 2011 Edgar E. Iglesias.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/block/flash.h"
#include "qapi/error.h"
#include "qemu/timer.h"
#include "sysemu/block-backend.h"
#include "exec/address-spaces.h"
#include "qemu/host-utils.h"
#include "hw/sysbus.h"

#define HPSC 
#ifdef HPSC
#include "hw/register-dep.h"
#include "sysemu/blockdev.h"
#endif

#define PL35X_ERR_DEBUG
#ifdef PL35X_ERR_DEBUG
#define DB_PRINT(...) do { \
    fprintf(stderr,  ": %s: ", __func__); \
    fprintf(stderr, ## __VA_ARGS__); \
    } while (0);
#else
    #define DB_PRINT(...)
#endif

#define TYPE_PL35X "arm.pl35x"

#define PL35X(obj) \
     OBJECT_CHECK(PL35xState, (obj), TYPE_PL35X)

#define HPSC_ECC

typedef struct PL35xItf {
    MemoryRegion mm;
    DeviceState *dev;
    uint8_t nand_pending_addr_cycles;
#ifdef HPSC_ECC
    uint8_t ecc_digest[16 * 1024];
    uint8_t ecc_oob[16 * 1024];
    uint32_t ecc_pos, ecc_subpage_offset;
    void * parent;
#endif
 
} PL35xItf;

typedef struct PL35xState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    /* FIXME: add support for multiple chip selects/interface */

    PL35xItf itf[2];

    /* FIXME: add Interrupt support */

    /* FIXME: add ECC support */
#ifdef HPSC_ECC
    uint32_t regs[0x450 >> 2];
#else
    uint32_t configs[10];	/* 0x0, ..., 0x24 */ 
    uint32_t cycles[8];	/* 0x100, 120, ... 1E0 */ 
    uint32_t ecc[2][11];	/* 0x300, 304, 308, 30c, 310, 314, .., 328 */ 
				/* 0x400, 404, 408, 40c, 410, 414, .., 428 */
#endif

    uint8_t x; /* the "x" in pl35x */
} PL35xState;

#ifdef HPSC_ECC
static bool new_ecc = false;
static void pl35x_ecc_init(PL35xItf *s)
{
    /* FIXME: Bad performance */
    memset(s->ecc_digest, 0xFF, 16 * 1024);
    memset(s->ecc_oob, 0xFF, 16 * 1024);
    s->ecc_pos = 0;
    s->ecc_subpage_offset = 0;
}
#define ECC_BYTES_PER_SUBPAGE 3
#define ECC_CODEWORD_SIZE 512

static void pl35x_ecc_digest(PL35xItf *s, uint8_t data)
{
    uint32_t ecc_bytes_per_subpage = ECC_BYTES_PER_SUBPAGE;

    s->ecc_digest[s->ecc_pos++] ^= ~data;
    if (!(s->ecc_pos % ecc_bytes_per_subpage)) {
        s->ecc_pos -= ecc_bytes_per_subpage;
    }

    s->ecc_subpage_offset++;
    if (s->ecc_subpage_offset == ECC_CODEWORD_SIZE) {
        s->ecc_subpage_offset = 0;
        do {
            s->ecc_pos++;
        } while (s->ecc_pos % ecc_bytes_per_subpage);
    }
}
#endif

#ifdef HPSC_ECC__
#define #define R_ECC_SLC_MLC (1 << 25)
/* code borrowd from Arasan NFC */
static void hpsc_ecc_init(PL35xState *s)
{
    /* FIXME: Bad performance */
    memset(s->ecc_digest, 0xFF, 16 * 1024);
    s->ecc_pos = 0;
    s->ecc_subpage_offset = 0;
}

/* not an ECC algorithm, but gives a deterministic OOB that
 * depends on the in band data
 */

static void hpsc_ecc_digest(PL35xState *s, uint8_t data)
{
    uint32_t page_size = 512; /* PL35x RTM page 2-11 */
    int ecc_bytes_per_subpage = DEP_AF_EX32(s->regs, ECC, ECC_SIZE) /
                                (page_size / ECC_CODEWORD_SIZE);

    s->ecc_digest[s->ecc_pos++] ^= ~data;
    if (!(s->ecc_pos % ecc_bytes_per_subpage)) {
        s->ecc_pos -= ecc_bytes_per_subpage;
    }

    s->ecc_subpage_offset++;
    if (s->ecc_subpage_offset == ECC_CODEWORD_SIZE) {
        s->ecc_subpage_offset = 0;
        do {
            s->ecc_pos++;
        } while (s->ecc_pos % ecc_bytes_per_subpage);
    }
}

static bool hpsc_ecc_correct(PL35xState *s)
{
    int i;
    uint8_t cef = 0;

    for (i = 0; i < DEP_AF_EX32(s->regs, ECC, ECC_SIZE); ++i) {
        if (s->ecc_oob[i] != s->ecc_digest[i]) {
            arasan_nfc_irq_event(s, R_INT_MUL_BIT_ERR);
            if (DEP_AF_EX32(s->regs, ECC_ERR_COUNT, PAGE_BOUND) != 0xFF) {
                s->regs[R_ECC_ERR_COUNT] +=
                    1 << R_ECC_ERR_COUNT_PAGE_BOUND_SHIFT;
            }
            /* FIXME: All errors in the first packet - not right */
            if (DEP_AF_EX32(s->regs, ECC_ERR_COUNT, PACKET_BOUND) != 0xFF) {
                s->regs[R_ECC_ERR_COUNT] +=
                    1 << R_ECC_ERR_COUNT_PACKET_BOUND_SHIFT;
            }
            DB_PRINT("ECC check failed on ECC byte %#x, %#02" PRIx8 " != %#02"
                     PRIx8 "\n", i, s->ecc_oob[i], s->ecc_digest[i]);
            return true;
        } else {
            cef ^= s->ecc_oob[i];
        }
    }
    /* Fake random successful single bit corrections for hamming */
    for (i = 0; i < 7; ++i) {
        cef = (cef >> 1) ^ (cef & 0x1);
    }
    if ((cef & 0x1) && ((s->regs[R_ECC] & R_ECC_SLC_MLC))) {
        arasan_nfc_irq_event(s, R_INT_ERR_INTRPT);
    }
    DB_PRINT("ECC check passed");
    return false;
}
#endif
static int first = 1;
static uint64_t pl35x_read(void *opaque, hwaddr addr,
                         unsigned int size)
{
    PL35xState *s = opaque;
    uint32_t r = 0;
#ifdef HPSC
    if (first) {
    memory_region_add_subregion(s->mmio.container, 0x600000000, &s->itf[1].mm);
    first = 0;
    }
#endif
    switch (addr) {
    case 0x0:
      {
        int rdy;
        if (s->itf[0].dev && object_dynamic_cast(OBJECT(s->itf[0].dev),
                                                      "nand")) {
            nand_getpins(s->itf[0].dev, &rdy);
            r |= (!!rdy) << 5;
        }
        if (s->itf[1].dev && object_dynamic_cast(OBJECT(s->itf[1].dev),
                                                      "nand")) {
            nand_getpins(s->itf[1].dev, &rdy);
            r |= (!!rdy) << 6;
        }
        }
        break;

    case 0x4:
    case 0x20:
    case 0x24:
#ifdef HPSC_ECC
         r = s->regs[addr >> 2];
#else
         r = s->configs[addr >> 2]; 
#endif
#ifdef HPSC
    case 0x100:
    case 0x120:
    case 0x140:
    case 0x160:
    case 0x180:
    case 0x1a0:
    case 0x1c0:
    case 0x1e0: {
#ifdef HPSC_ECC
         r = s->regs[addr >> 2];
#else
	r = s->cycles[(addr & 0xf0) >> 5];
#endif
	break;
    }
    case 0x300:
    case 0x304:
    case 0x308:
    case 0x30c:
    case 0x310:
    case 0x314:
    case 0x318:
    case 0x31c:
    case 0x320:
    case 0x324:
    case 0x328:
    case 0x400:
    case 0x404:
    case 0x408:
    case 0x40c:
    case 0x410:
    case 0x414:
        r = s->regs[addr >> 2];
        break;
#ifdef HPSC_ECC
    case 0x418: 
    case 0x41c:
    case 0x420:
    case 0x424:
        r = s->regs[addr >> 2];
        break;
#else
    case 0x418: /* how about calculate it now? */
                s->regs[addr >> 2] = 0x50 << 24; /* | (ecc_calculate() & 0x00ffffff); */
                r = s->regs[addr >> 2];
		break;
    case 0x41c:
                s->regs[addr >> 2] = 0x50 << 24; /* | (ecc_calculate() & 0x00ffffff); */
                r = s->regs[addr >> 2];
		break;
    case 0x420:
                s->regs[addr >> 2] = 0x50 << 24; /* | (ecc_calculate() & 0x00ffffff); */
                r = s->regs[addr >> 2];
		break;
    case 0x424:
                s->regs[addr >> 2] = 0x50 << 24; /* | (ecc_calculate() & 0x00ffffff); */
                r = s->regs[addr >> 2];
		break;
#endif
    case 0x428:
        r = s->regs[addr >> 2];
	break;
#endif
    default:
        DB_PRINT("Unimplemented SMC read access reg=" TARGET_FMT_plx "\n",
                 addr);
        break;
    }
    DB_PRINT("=== addr = 0x%lx val = 0x%x\n", addr, r);
    return r;
}

static void pl35x_write(void *opaque, hwaddr addr, uint64_t value64,
                      unsigned int size)
{
    DB_PRINT("=== addr = 0x%x v = 0x%x\n", (unsigned)addr, (unsigned)value64);
#ifdef HPSC
    PL35xState *s = opaque;
    switch (addr) {
    case 0x8:
    case 0xc:
    case 0x10:
    case 0x14:
    case 0x18:
    case 0x1c:
    case 0x20:
    case 0x24:
#ifdef HPSC_ECC
        s->regs[addr >> 2] = value64;
#else
         s->configs[addr >> 2] = value64;
#endif
        break;
    case 0x100:
    case 0x120:
    case 0x140:
    case 0x160:
    case 0x180:
    case 0x1a0:
    case 0x1c0:
    case 0x1e0: {
#ifdef HPSC_ECC
        s->regs[addr >> 2] = value64;
#else
	s->cycles[(addr & 0xf0) >> 5] = value64;
#endif
	break;
    }
    case 0x300:
    case 0x304:
    case 0x308:
    case 0x30c:
    case 0x310:
    case 0x314:
    case 0x318:
    case 0x31c:
    case 0x320:
    case 0x324:
    case 0x328:
    case 0x400:
    case 0x404:
    case 0x408:
    case 0x40c:
    case 0x410:
    case 0x414:
    case 0x418:
    case 0x41c:
    case 0x420:
    case 0x424:
    case 0x428: {
#ifdef HPSC_ECC
        s->regs[addr >> 2] = value64;
#else
        s->ecc[(addr &0x400) >> 10][(addr & 0xf) >> 2] = value64;
#endif
	break;
    }
    default:
        DB_PRINT("Unimplemented SMC read access reg=" TARGET_FMT_plx "\n",
                 addr);
        break;
    }
#else
    /* FIXME: implement */
    DB_PRINT("Unimplemented SMC write access reg=" TARGET_FMT_plx "\n",
                 addr);
#endif
}

static const MemoryRegionOps pl35x_ops = {
    .read = pl35x_read,
    .write = pl35x_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

#ifdef HPSC_ECC
    int data_size = 0;
#endif
static uint64_t nand_read(void *opaque, hwaddr addr,
                           unsigned int size)
{
    PL35xItf *s = opaque;
    unsigned int len = size;
    int shift = 0;
    uint32_t r = 0;

#ifdef HPSC_ECC
    PL35xState * ps = s->parent;
    int page_size = nand_page_size(s->dev);
    int nand_remain_data = nand_iolen(s->dev);
    DB_PRINT("before: nand_iolen = 0x%x\n", nand_remain_data);
    if (nand_remain_data > 0 && nand_remain_data < 4 && size >= 4) {
	// previous read_byte didn't clean the buffer.
	// flush it.
        while(nand_remain_data--) {
            nand_getio(s->dev);
        }
    }
#endif
    while (len--) {
        uint8_t r8;

        r8 = nand_getio(s->dev) & 0xff;
        r |= r8 << shift;
        shift += 8;
    }
    DB_PRINT("addr=0x%x r=0x%x size=%d\n", (unsigned)addr, r, size);
#ifdef HPSC_ECC
    DB_PRINT("after: nand_iolen = 0x%x\n", nand_iolen(s->dev));
    if (new_ecc) {
        DB_PRINT("New ECC starts\n");
        pl35x_ecc_init(s);
        data_size = 0;
        new_ecc = false;
    } 
    data_size += 4;
    pl35x_ecc_digest(s, r);
    if (data_size == page_size) {	// assume PAGE_SIZE = 2048
        /* save ecc value to the registers */
        DB_PRINT("ECC : is saved\n");
        for (int i = 0; i < 4 ; i++) {
            uint32_t r32 = (0x40 << 24);	// always ecc is correct
            for (int j = 0, shift = 0; j < ECC_BYTES_PER_SUBPAGE; j++) {
                uint8_t r8 = s->ecc_digest[i*ECC_BYTES_PER_SUBPAGE+j];
                r32 |= r8 << shift;
                shift += 8;
                printf("0x%x ", r8);
            }
            int ecc1_block_idx = 0x418 + (i << 2);
            ps->regs[ecc1_block_idx >> 2] = r32;
        }
        printf("\n");
    }
    DB_PRINT("return (0x%x)\n", r);
#endif
    return r;
}

static void nand_write(void *opaque, hwaddr addr, uint64_t value64,
                       unsigned int size)
{
    struct PL35xItf *s = opaque;
    bool data_phase, ecmd_valid;
    unsigned int addr_cycles = 0;
    uint16_t start_cmd, end_cmd;
    uint32_t value = value64;
    uint32_t nandaddr = value;


    /* Decode the various signals.  */
    data_phase = (addr >> 19) & 1;
    ecmd_valid = (addr >> 20) & 1;
    start_cmd = (addr >> 3) & 0xff;
    end_cmd = (addr >> 11) & 0xff;

    DB_PRINT("addr=%x v=%x size=%d, data_phase(0x%x), ecmd_vali(0x%x), start_cmd(0x%x), end_cmd(0x%x)\n", (unsigned)addr, (unsigned)value, size, data_phase, ecmd_valid, start_cmd, end_cmd);

    if (!data_phase) {
        addr_cycles = (addr >> 21) & 7;
    }

    if (!data_phase) {
        DB_PRINT("start_cmd=%x end_cmd=%x (valid=%d) acycl=%d\n",
                start_cmd, end_cmd, ecmd_valid, addr_cycles);
    }

    /* Writing data to the NAND.  */
    if (data_phase) {
    DB_PRINT("data_phase: writing data to NAND\n");
        nand_setpins(s->dev, 0, 0, 0, 1, 0);
        while (size--) {
            nand_setio(s->dev, value & 0xff);
            value >>= 8;
        }
    }

    /* Writing Start cmd.  */
    if (!data_phase && !s->nand_pending_addr_cycles) {
    DB_PRINT("writing Start cmd \n");
        nand_setpins(s->dev, 1, 0, 0, 1, 0);
        nand_setio(s->dev, start_cmd);
    }

    if (!addr_cycles) {
        s->nand_pending_addr_cycles = 0;
    }
    if (s->nand_pending_addr_cycles) {
        addr_cycles = s->nand_pending_addr_cycles;
        s->nand_pending_addr_cycles = 0;
    }
    if (addr_cycles > 4) {
        s->nand_pending_addr_cycles = addr_cycles - 4;
        addr_cycles = 4;
    }
    while (addr_cycles) {
        nand_setpins(s->dev, 0, 1, 0, 1, 0);
        DB_PRINT("nand cycl=%d addr=%x\n", addr_cycles, nandaddr & 0xff);
        nand_setio(s->dev, nandaddr & 0xff);
        nandaddr >>= 8;
        addr_cycles--;
    }

    /* Writing commands. One or two (Start and End).  */
    if (ecmd_valid && !s->nand_pending_addr_cycles) {
    DB_PRINT("writing commands. One or two (Start and End)\n");
        nand_setpins(s->dev, 1, 0, 0, 1, 0);
        nand_setio(s->dev, end_cmd);
#ifdef HPSC_ECC
	new_ecc = true;
#endif
    }
}

static const MemoryRegionOps nand_ops = {
    .read = nand_read,
    .write = nand_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};

static void pl35x_init_sram(SysBusDevice *dev, PL35xItf *itf)
{
    /* d Just needs to be a valid sysbus device with at least one memory
     * region
     */
    SysBusDevice *sbd = SYS_BUS_DEVICE(itf->dev);

    memory_region_init(&itf->mm, OBJECT(dev), "pl35x.sram", 1 << 24);
    if (sbd) {
        memory_region_add_subregion(&itf->mm, 0,
                                    sysbus_mmio_get_region(sbd, 0));
    }
    sysbus_init_mmio(dev, &itf->mm);
}

static void pl35x_init_nand(SysBusDevice *dev, PL35xItf *itf)
{
#ifdef HPSC
//    DriveInfo *dinfo = drive_get_next(IF_MTD); 
    DriveInfo *dinfo = drive_get_next(IF_PFLASH); 
    /* d Must be a NAND flash */
    if (dinfo) {
#ifdef HPSC
        itf->dev = nand_init(dinfo ? blk_by_legacy_dinfo(dinfo) : NULL,
                               NAND_MFR_MICRON, 0xaa);	/* from drivers/mtd/nand/nand_ids.c, MiB */
//                               NAND_MFR_MICRON, 0xda);	/* from drivers/mtd/nand/nand_ids.c, MiB */
//                               NAND_MFR_MICRON, 0xA2);	/* from drivers/mtd/nand/nand_ids.c, 512MiB */
#else
        itf->dev = nand_init(dinfo ? blk_by_legacy_dinfo(dinfo) : NULL,
                               NAND_MFR_MICRON, 0x44);
#endif
    } else {
	return;
    }
#endif
    assert(object_dynamic_cast(OBJECT(itf->dev), "nand"));

    memory_region_init_io(&itf->mm, OBJECT(dev), &nand_ops, itf, "pl35x.nand",
                          1 << 24);
    sysbus_init_mmio(dev, &itf->mm);
#ifdef HPSC__
    PL35xState *s = PL35X(dev);
    memory_region_add_subregion(&s->mmio, 0x1000, &itf->mm);
#endif
}

static int pl35x_init(SysBusDevice *dev)
{
    PL35xState *s = PL35X(dev);
    int itfn = 0;

    memory_region_init_io(&s->mmio, OBJECT(dev), &pl35x_ops, s, "pl35x_io",
                          0x1000);
    sysbus_init_mmio(dev, &s->mmio);
    if (s->x != 1) { /* everything cept PL351 has at least one SRAM */
        pl35x_init_sram(dev, &s->itf[itfn]);
        itfn++;
    }
    if (s->x & 0x1) { /* PL351 and PL353 have NAND */
        pl35x_init_nand(dev, &s->itf[itfn]);
    } else if (s->x == 4) { /* PL354 has a second SRAM */
        pl35x_init_sram(dev, &s->itf[itfn]);
    }
#ifdef HPSC_ECC
    s->itf[0].parent = s;
    s->itf[1].parent = s;
#endif
    return 0;
}
static void pl35x_initfn(Object *obj)
{
    PL35xState *s = PL35X(obj);

    object_property_add_link(obj, "dev0", TYPE_DEVICE,
                             (Object **)&s->itf[0].dev,
                             object_property_allow_set_link,
                             OBJ_PROP_LINK_UNREF_ON_RELEASE,
                             &error_abort);
    object_property_add_link(obj, "dev1", TYPE_DEVICE,
                             (Object **)&s->itf[1].dev,
                             object_property_allow_set_link,
                             OBJ_PROP_LINK_UNREF_ON_RELEASE,
                             &error_abort);
}

static Property pl35x_properties[] = {
    DEFINE_PROP_UINT8("x", PL35xState, x, 3),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vmstate_pl35x = {
    .name = "pl35x",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(itf[0].nand_pending_addr_cycles, PL35xState),
        VMSTATE_UINT8(itf[1].nand_pending_addr_cycles, PL35xState),
        VMSTATE_END_OF_LIST()
    }
};

static void pl35x_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(klass);

    k->init = pl35x_init;
    dc->props = pl35x_properties;
    dc->vmsd = &vmstate_pl35x;
}

static TypeInfo pl35x_info = {
    .name           = TYPE_PL35X,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(PL35xState),
    .class_init     = pl35x_class_init,
    .instance_init  = pl35x_initfn,
};

static void pl35x_register_types(void)
{
    type_register_static(&pl35x_info);
}

type_init(pl35x_register_types)
