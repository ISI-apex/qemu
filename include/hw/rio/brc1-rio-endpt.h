#ifndef HW_RIO_BRC1_RIO_ENDPT
#define HW_RIO_BRC1_RIO_ENDPT

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/register.h"
#include "hw/rio/rio.h"
#include "hw/rio/sm-rio-switch.h"

#define TYPE_BRC1_RIO_ENDPT "praesum,brc1-rio-endpt"

#define BRC1_RIO_ENDPT(obj) \
     OBJECT_CHECK(BRC1RIOEndpt, (obj), TYPE_BRC1_RIO_ENDPT)

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

REG32(DEV_ID,      0x000000)
    FIELD(DEV_ID, DEVICE_IDENTITY,              32 -  0 - 16, 16)
    FIELD(DEV_ID, DEVICE_VENDOR_IDENTITY,       32 - 16 - 16, 16)

REG32(LCS_BA0,      0x000058)
REG32(LCS_BA1,      0x00005c)

REG32(B_DEV_ID,      0x000060)
    FIELD(B_DEV_ID, BASE_DEVICE_ID,             32 -  8 -  8,  8)
    FIELD(B_DEV_ID, LARGE_BASE_DEVICE_ID,       32 - 16 - 16, 16)

REG32(COMP_TAG,      0x00006c)

REG32(IR_SP_TX_CTRL, 0x107000)
    FIELD(IR_SP_TX_CTRL, OCTETS_TO_SEND,        32 -  0 - 16, 16)
REG32(IR_SP_TX_STAT, 0x107004)
    FIELD(IR_SP_TX_STAT, OCTETS_REMAINING,      32 -  0 - 16, 16)
    FIELD(IR_SP_TX_STAT, BUFFERS_FILLED,        32 - 16 -  4,  4)
    FIELD(IR_SP_TX_STAT, FULL,                  32 - 27 -  1,  1)
    FIELD(IR_SP_TX_STAT, TX_FIFO_STATE,         32 - 28 -  4,  4)
REG32(IR_SP_TX_DATA, 0x107008)
REG32(IR_SP_RX_CTRL, 0x10700c)
    FIELD(IR_SP_RX_CTRL, OVERFLOW_MODE_NO_DISCARD, 32 - 31 - 1,  1)
REG32(IR_SP_RX_STAT, 0x107010)
    FIELD(IR_SP_RX_STAT, OCTETS_REMAINING,      32 -  0 - 16, 16)
    FIELD(IR_SP_RX_STAT, BUFFERS_FILLED,        32 - 16 -  4,  4)
    FIELD(IR_SP_RX_STAT, FULL,                  32 - 27 -  1,  1)
    FIELD(IR_SP_RX_STAT, RX_FIFO_STATE,         32 - 28 -  4,  4)
REG32(IR_SP_RX_DATA, 0x107014)

REG32(IR_MSG_CTRL, 0x107100)
REG32(IR_MSG_INIT_PS_STAT, 0x107110)
    FIELD(IR_MSG_INIT_PS_STAT, IDLE,            32 -  0 -  1, 1)
    FIELD(IR_MSG_INIT_PS_STAT, MAX_DESCRIPTORS, 32 -  1 -  3, 3)
    FIELD(IR_MSG_INIT_PS_STAT, DESCRIPTORS,     32 -  4 -  3, 3)
REG32(IR_MSG_INIT_PS_DESCR_FIFO_L, 0x107114)
REG32(IR_MSG_INIT_PS_DESCR_FIFO_H, 0x107118)
REG32(IR_MSG_TRGT_STAT, 0x107150)
    FIELD(IR_MSG_TRGT_STAT, IDLE,               32 -  0 -  1, 1)
    FIELD(IR_MSG_TRGT_STAT, MAX_DESCRIPTORS,    32 -  1 -  3, 3)
    FIELD(IR_MSG_TRGT_STAT, DESCRIPTORS,        32 -  4 -  3, 3)
REG32(IR_MSG_TRGT_DESCR_FIFO_L, 0x107154)
REG32(IR_MSG_TRGT_DESCR_FIFO_H, 0x107158)

REG32(IR_OMAP_CTRL, 0x200000)
    FIELD(IR_OMAP_CTRL, SUBREGIONS,             32 -  1 -  3,  3)
REG32(IR_OME_0_CTRL, 0x200400)
    FIELD(IR_OME_0_CTRL, WRITE_TYPE,            32 -  0 -  2,  2)
    FIELD(IR_OME_0_CTRL, PRIO,                  32 -  2 -  2,  2)
    FIELD(IR_OME_0_CTRL, TTYPE,                 32 -  4 -  2,  2)
    FIELD(IR_OME_0_CTRL, XAMSBS,                32 -  6 -  2,  2)
    FIELD(IR_OME_0_CTRL, CRF,                   32 -  8 -  1,  1)
    FIELD(IR_OME_0_CTRL, ADDR_SIZE,             32 -  9 -  2,  2)
/* There are IR_OME_x_* for x from 0 to 63, they are implied by the addresses
 * of following registers. To refer to these registers use offset from 0:
 * (IR_OME_0_* + region) and to access the fields use the macros for 0. */
REG32(IR_OME_0_DESTID, 0x200500)
REG32(IR_OME_0_ADDR_H, 0x200600)
REG32(IR_OME_0_ADDR_L, 0x200700)

REG32(IR_IMAP_CTRL, 0x300000)
    FIELD(IR_IMAP_CTRL, SUBREGIONS,             32 -  1 -  3,  3)
REG32(IR_IME_0_CTRL, 0x300400)
    FIELD(IR_IME_0_CTRL, ADDR_SIZE,             32 -  0 -  2,  2)
REG32(IR_IME_0_SRCID, 0x300500)
REG32(IR_IME_0_SRCIDM, 0x300600)
REG32(IR_IME_0_ADDR_H, 0x300700)
REG32(IR_IME_0_ADDR_L, 0x300800)

#define REG_MAX 0x440000 /* User Guide Table 26 */

#define MAX_PKT_SIZE 64 /* 32-bit words TODO */
#define SP_TX_FIFO_SIZE 8 /* packets TODO */
#define SP_RX_FIFO_SIZE 8 /* packets TODO */
#define MSG_RX_FIFO_SIZE 8 /* messages TODO */
#define MAX_SP_PKT_SIZE (MAX_PKT_SIZE * 4) /* bytes */

/* Figures 21, 22 */
typedef enum {
    SP_TX_STATE_IDLE,
    SP_TX_STATE_ARMED,
    SP_TX_STATE_ACTIVE,
} SpTxState;

typedef enum {
    SP_RX_STATE_IDLE,
    SP_RX_STATE_ACTIVE,
} SpRxState;

typedef struct {
    unsigned len; /* words. TODO: convert to bytes */
    uint8_t data[MAX_SP_PKT_SIZE];
} SpPkt;

typedef enum ReqStatus {
    REQ_STATUS_INVALID,
    REQ_STATUS_OUTSTANDING,
    REQ_STATUS_DONE,
    REQ_STATUS_ERROR,
} ReqStatus;

typedef struct MapReq {
    ReqStatus status;
    unsigned resp_len;
    uint8_t resp_data[RIO_MAX_RESP_SIZE];
} MapReq;

typedef struct MsqReq {
    ReqStatus status;
    uint8_t mbox;
    uint8_t letter;
    uint8_t msg_seg;
} MsgReq;

typedef uint64_t trgt_addr_t;

struct NetClientState;

typedef struct BRC1RIOEndpt {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    MemoryRegion map_mr;

    MemoryRegion *dma_mr;
    AddressSpace *dma_as;

    SpTxState sp_tx_state;
    SpRxState sp_rx_state;
    SpPkt sp_tx_fifo[SP_TX_FIFO_SIZE];
    SpPkt sp_rx_fifo[SP_RX_FIFO_SIZE];
    unsigned sp_rx_fifo_count; /* TODO: head, tail */
    unsigned sp_tx_fifo_count; /* TODO: head, tail */

    trgt_addr_t msg_rx_fifo[MSG_RX_FIFO_SIZE]; /* addresses of descriptors at heads of chains */
    unsigned msg_rx_fifo_head, msg_rx_fifo_tail;
    trgt_addr_t msg_rx_desc; /* next descriptor to fill within a chain */

    /* HW supports a single outstanding outgoing map request at a time */
    MapReq map_req;

    MsgReq msg_req; /* TODO: support more than one outstanding msg requests */

#if 0
    RIOPort *port;
#else
    struct SMRIOSwitch *rio_switch;
    uint8_t rio_switch_port_idx;
#endif

    qemu_irq event_irq;
    /* TODO: are Messaging Unit interrupts dedicated lines or events? */
    qemu_irq msg_tx_irq;
    qemu_irq msg_rx_irq;

    uint32_t regs[REG_MAX];
    RegisterInfo regs_info[REG_MAX];
} BRC1RIOEndpt;

int brc1_rio_endpt_in(BRC1RIOEndpt *s, RIOTx *tx);

#endif // HW_RIO_BRC1_RIO_ENDPT
