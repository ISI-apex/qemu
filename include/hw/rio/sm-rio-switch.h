#ifndef HW_RIO_SM_RIO_SWITCH_H
#define HW_RIO_SM_RIO_SWITCH_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/register.h"
#include "hw/rio/rio.h"
#include "hw/rio/brc1-rio-endpt.h"

#define TYPE_SM_RIO_SWITCH "praesum,sm-rio-switch"

#define SM_RIO_SWITCH(obj) \
     OBJECT_CHECK(SMRIOSwitch, (obj), TYPE_SM_RIO_SWITCH)

/* The datasheet indexes the MSB as Bit 0 (see User Guide 1.1.1), whereas
 * in context of shifts in C, Bit 0 is LSB. However, the significance of bits
 * in a bitfield is the same in both cases: a bitfield starts at a less signficant
 * bit and ends in a more significat bit. This doesn't have anything to do with
 * endianness, and there's no conversion, no flipping being done here. There is
 * no abiguity about where the bitfields are in register words (unambigously
 * defined by LSB/MSB). The question is just cosmetically how we specify that
 * unabiguous location in the following field declaration. We could ignore the
 * indexing convention in the datasheet altogether, however then the literal
 * values used in the datashet vs in these field definitions would not be easily
 * cross-checkable, at a glance.  So, we adopt a pattern here such that the
 * 'start' bit index value (and width value) matches the tables in the
 * datasheet verbatim:
 *      32 -  start - width, width
 * For example, for bitfield in Table 120 "Priority":
 *    Datasheet:  Bits 2-3
 *    Field def:  32 - 2 - 2, 2
 *                     ^ this value is the same verbatim
 */

REG32(PORT_MAPPING_TABLE_ENTRY_0,   0x102000)
REG32(PORT_MAPPING_TABLE_ENTRY_255, 0x1023FC)
    FIELD(PORT_MAPPING_TABLE_ENTRY, VALID,     32 -  0 -  1, 1)
    FIELD(PORT_MAPPING_TABLE_ENTRY, TYPE,      32 -  1 -  2, 2)
    FIELD(PORT_MAPPING_TABLE_ENTRY, PORT_MAP,  32 - 24 -  8, 8)
//#define PORT_MAPPING_TABLE_ENTRY_MASK 0xe00000ff /* !reserved */
#define PORT_MAPPING_TABLE_ENTRY_MASK \
    (R_PORT_MAPPING_TABLE_ENTRY_VALID_MASK | \
     R_PORT_MAPPING_TABLE_ENTRY_TYPE_MASK | \
     R_PORT_MAPPING_TABLE_ENTRY_PORT_MAP_MASK)

/* PORT_MAPPING_TABLE_ENTRY_n registers are handled dynamically */
#define SM_RIO_SWITCH_REG_MAX (0 + 1)
#define SM_RIO_SWITCH_CSR_SIZE 0x400000

#define SM_RIO_SWITCH_DEV_IDS 256

#define SM_RIO_SWITCH_MAX_PORTS 8

struct NetSwitchPort;

typedef struct SMRIOSwitch {
    SysBusDevice parent_obj;
    MemoryRegion iomem;

    uint8_t num_ports;
    struct BRC1RIOEndpt *ports[SM_RIO_SWITCH_MAX_PORTS];

    /* TODO: should be encapsulated in a port type */
    char *netdev_ids[SM_RIO_SWITCH_MAX_PORTS];
    struct NetSwitchPort *netports[SM_RIO_SWITCH_MAX_PORTS];

    uint32_t mapping_table[SM_RIO_SWITCH_DEV_IDS];

    /* Output control signals connected to inputs on the CPUs */
    qemu_irq event_irq;

    uint32_t regs[SM_RIO_SWITCH_REG_MAX];
    RegisterInfo regs_info[SM_RIO_SWITCH_REG_MAX];
} SMRIOSwitch;

void sm_rio_switch_in(SMRIOSwitch *s, unsigned in_port, RIOTx *tx);

#endif /* HW_RIO_SM_RIO_SWITCH_H */
