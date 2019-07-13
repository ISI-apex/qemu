#ifndef HW_RIO_RIO_H
#define HW_RIO_RIO_H

#include "hw/register.h"
#include "hw/registerfields.h"

#define RIO_MAX_PAYLOAD_SIZE 256 /* TODO: Spec reference */

#define RIO_MAX_PKT_SIZE 64 /* TODO */
#define PKT_BUF_WORDS (RIO_MAX_PKT_SIZE / sizeof(uint32_t))

#define RIO_MAX_RESP_SIZE RIO_MAX_PAYLOAD_SIZE

/* Theoretical max is 32-bit IDs, max supported by Praesum BRC1 EP is 16-bit */
typedef uint32_t rio_devid_t;

/* TODO: rename into CamelCase */
/* Spec Part 3 Section 2.4 Table 2-1 and User Guide Table 120 (assumed equal) */
enum rio_transport_type {
    RIO_TRANSPORT_DEV8  = 0b00,
    RIO_TRANSPORT_DEV16 = 0b01,
    RIO_TRANSPORT_DEV32 = 0b10, /* not supported by Praesum BRC1 EP */
};

/* Tabel 4-1 in RIO Spec P1 */
enum rio_ftype {
    RIO_FTYPE_READ          =  2,
    RIO_FTYPE_WRITE         =  5,
    RIO_FTYPE_STREAM_WRITE  =  6,
    RIO_FTYPE_MAINT         =  8,
    RIO_FTYPE_DOORBELL      = 10,
    RIO_FTYPE_MSG           = 11,
    RIO_FTYPE_RESP          = 13,
};

enum rio_transaction {
    RIO_TRANS_READ_NREAD            = 0b0100,
    RIO_TRANS_WRITE_NWRITE          = 0b0100,
    RIO_TRANS_WRITE_NWRITE_R        = 0b0101,
    RIO_TRANS_MAINT_REQ_READ        = 0b0000,
    RIO_TRANS_MAINT_REQ_WRITE       = 0b0001,
    RIO_TRANS_MAINT_RESP_READ       = 0b0010,
    RIO_TRANS_MAINT_RESP_WRITE      = 0b0011,
    RIO_TRANS_MAINT_REQ_PORT_WRITE  = 0b0100,
    RIO_TRANS_RESP_WITHOUT_PAYLOAD  = 0b0000,
    RIO_TRANS_RESP_WITH_PAYLOAD     = 0b1000,
    RIO_TRANS_RESP_MSG              = 0b0001,
};

/* Table 4-7 in Spec */
enum rio_status { /* TODO: camel case */
    RIO_STATUS_DONE     = 0x0000,
    RIO_STATUS_ERROR    = 0b0111,
};

typedef struct RIOTx { /* TODO: case */
    uint8_t dest; /* device ID */
    unsigned payload_len;
    uint8_t *payload;
    uint64_t send_time;
    uint64_t rcv_time;
} RIOTx;

typedef enum AccessType {
    ACCESS_READ,
    ACCESS_WRITE,
} AccessType;

#endif // HW_RIO_RIO_H
