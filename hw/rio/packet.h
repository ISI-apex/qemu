#ifndef HW_RIO_PACKET
#define HW_RIO_PACKET

#include <stdint.h>

#include "hw/rio/rio.h"

#define WORDS_IN_DWORD (sizeof(uint64_t) / sizeof(uint32_t))

/* When we want to use a single variable (so, uint64_t) to hold a 66-bit byte
 * addresses that we know is aligned to a double-word, these are useful. */
#define BYTE_ADDR_TO_DW_ADDR(addr) ((addr) >> 3)
#define DW_ADDR_TO_WORD_ADDR(addr) ((addr) << 1)
#define DW_ADDR_TO_BYTE_ADDR(addr) ((addr) << 3)
#define WORD_ADDR_TO_DW_ADDR(addr) ((addr) << 1)
#define WORD_ADDR_TO_BYTE_ADDR(addr) ((addr) << 2)
#define BYTE_ADDR_TO_WORD_ADDR(addr) ((addr) >> 2)

/* Packet field widths (note: some are programatically determined)
 * Note: we do not use the register field helper API: FIELD() for declaring and
 * FIELD_[EX|DP]*() for accessing, because some fields are variable length
 * (multiple possible fixed widths). Instead we define our own API for
 * packing/unpacking fields sequentially: field_push()/field_pop().*/
enum { /* enum for the debugger to have the literals */
    PKT_FIELD_PRIO             =  2,
    PKT_FIELD_TTYPE            =  2,
    PKT_FIELD_FTYPE            =  4,
    PKT_FIELD_TRANSACTION      =  4,
    PKT_FIELD_RDWRSIZE         =  4,
    PKT_FIELD_TARGET_TID       =  8,
    PKT_FIELD_SRC_TID          =  8,
    PKT_FIELD_HOP_COUNT        =  8,
    PKT_FIELD_ADDR             = 29,
    PKT_FIELD_CONFIG_OFFSET    = 21,
    PKT_FIELD_WDPTR            =  1,
    PKT_FIELD_XAMSBS           =  2,
    PKT_FIELD_STATUS           =  4,
    PKT_FIELD_MSG_LEN          =  4,
    PKT_FIELD_SEG_SIZE         =  4,
    PKT_FIELD_LETTER           =  2,
    PKT_FIELD_MBOX             =  2,
    PKT_FIELD_MSGSEG_XMBOX     =  4,
    PKT_FIELD_INFO             = 16,
};

/* Struct sufficiently general to hold a packet of any type */
typedef struct RioPkt {
    /* Physical Layer */
    uint8_t prio;

    /* Transport Layer */
    enum rio_transport_type ttype;
    rio_devid_t dest_id;
    rio_devid_t src_id;

    /* Logical Layer */
    enum rio_ftype ftype;
    uint8_t src_tid; /* TODO: unify into one tid field */
    uint8_t target_tid;

    /* TODO: Union these fields to overlap msg fields */
    enum rio_transaction transaction;

    /* Ideally, we would just store a byte address and a byte length, but we
     * need to support 66-bit byte addresses, but we want to use uint64_t for
     * efficiency, so we do double-word address + byte offset. */
    uint64_t dw_addr; /* address of a dword (right-shifted byte address) */
    uint8_t dw_offset; /* index of byte within dword pointed to by dw_address */
    uint16_t rdwr_bytes; /* number of bytes to read/write at dw_offset */
    unsigned addr_width; /* bits in byte address obtained frmo dw_address */
    uint8_t status;
    uint8_t hop_count; /* for routing RIO_FTYPE_MAINT packets */

    unsigned payload_len; /* double-words */
    uint64_t payload[RIO_MAX_PAYLOAD_SIZE];

    /* Messaging Layer */
    uint8_t msg_len;
    uint8_t seg_size;
    uint8_t mbox;
    uint8_t letter;
    uint8_t msg_seg;
    uint16_t info; /* don't use payload[] b/c payload is DW granularity */

    /* Implementation metadata */
    union {
        uint64_t send_time;
        uint64_t rcv_time;
    } timestamp;
} RioPkt;

void pack_header(uint8_t *buf, unsigned size, unsigned *pos,
                 const RioPkt *pkt);
int unpack_header(RioPkt *pkt, uint8_t *buf, unsigned size,
                             unsigned *pos);
void pack_body(uint8_t *buf, unsigned size, unsigned *pos, const RioPkt *pkt);
int unpack_body(RioPkt *pkt, uint8_t *buf, unsigned size, unsigned *pos);
unsigned pack_pkt(uint8_t *buf, unsigned size, const RioPkt *pkt);
int unpack_pkt(RioPkt *pkt, uint8_t *buf, unsigned size);

bool is_rdwr_access_supported(AccessType access, uint64_t addr, unsigned size);

void log_pkt(const char *prefix, uint8_t *pkt_buf, unsigned len);

#define MSG_SEG_SIZE_INVALID 0
uint16_t msg_seg_size(uint8_t seg_size_field);
uint8_t msg_seg_size_field(uint8_t seg_size);

static inline unsigned div_ceil(unsigned x, unsigned y)
{
    return x / y + (x % y ? 1 : 0);
}

static inline uint64_t rdwr_dw_mask_from_span(uint16_t bytes, uint8_t offset)
{
    return ((1ULL << (bytes * BITS_PER_BYTE)) - 1) << (offset * BITS_PER_BYTE);
}


#endif /* HW_RIO_PACKET */

