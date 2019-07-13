#include <stdint.h>

#include "qemu/osdep.h"
#include "hw/register.h"
#include "qemu/log.h"

#include "packet.h"

#include "rio-dev.h"

#define WORD_SHIFT (sizeof(uint32_t) * BITS_PER_BYTE)

void rio_cfg_space_read(void *dev, csr_read32_accessor_t *csr_read32,
                        RioPkt *req, RioPkt *resp)
{
    unsigned req_dwords = div_ceil(req->rdwr_bytes, sizeof(uint64_t));
    uint64_t req_dw_mask = rdwr_dw_mask_from_span(req->rdwr_bytes,
                                                  req->dw_offset);
    uint32_t csr_addr = DW_ADDR_TO_BYTE_ADDR(req->dw_addr);

    unsigned w, dw;
    uint32_t word_mask, csr_val;
    uint64_t value;
    int rc;

    assert(dev);
    assert(csr_read32);

    req_dwords = div_ceil(req->rdwr_bytes, sizeof(uint64_t));
    req_dw_mask = rdwr_dw_mask_from_span(req->rdwr_bytes, req->dw_offset);
    csr_addr = DW_ADDR_TO_BYTE_ADDR(req->dw_addr);

    /* Accessor is 32-bit, so to process each dword, we do two accesses. */

    resp->status = RIO_STATUS_DONE;
    resp->payload_len = 0;
    for (dw = 0; dw < req_dwords; ++dw) {
        value = 0;
        for (w = 0; w < WORDS_IN_DWORD; ++w) {
            word_mask = req_dw_mask >> (w * WORD_SHIFT);
            if (word_mask) {
                rc = csr_read32(dev, csr_addr, &csr_val);
                if (rc) {
                    qemu_log("RIO: NOTICE: maint read @0x%x failed\n",
                             csr_addr);
                    resp->status = RIO_STATUS_ERROR;
                    return;
                }
                value |= (uint64_t)(csr_val & word_mask) << (w * WORD_SHIFT);
            }
            csr_addr += sizeof(uint32_t);
        }
        if (resp->status == RIO_STATUS_ERROR) {
            return;
        }
        resp->payload[resp->payload_len++] = value;
    }
    if (resp->status == RIO_STATUS_ERROR)
        return;
    assert(div_ceil(req->rdwr_bytes, sizeof(uint64_t)) == resp->payload_len);
}

void rio_cfg_space_write(void *dev, csr_write32_accessor_t *csr_write32,
                        RioPkt *req, RioPkt *resp)
{
    unsigned req_dwords = div_ceil(req->rdwr_bytes, sizeof(uint64_t));
    uint64_t req_dw_mask = rdwr_dw_mask_from_span(req->rdwr_bytes,
                                                  req->dw_offset);
    uint32_t csr_addr = DW_ADDR_TO_BYTE_ADDR(req->dw_addr);

    unsigned w, dw;
    uint32_t word, word_mask;
    int rc;

    assert(dev);
    assert(csr_write32);

    /* Accessor is 32-bit, so to process each dword, we do two accesses. */

    assert(req_dwords == req->payload_len); /* enforced by unpacker */
    for (dw = 0; dw < req_dwords; ++dw) {
        for (w = 0; w < WORDS_IN_DWORD; ++w) {
            word_mask = req_dw_mask >> (w * WORD_SHIFT);
            word = req->payload[dw] >> (w * WORD_SHIFT);

            if (word_mask) {
                rc = csr_write32(dev, csr_addr, word, word_mask); 
                if (rc) {
                    qemu_log("RIO: NOTICE: maint write @0x%x failed\n",
                             csr_addr);
                    resp->status = RIO_STATUS_ERROR;
                    return;
                }
            }
            csr_addr += sizeof(uint32_t);
        }
        if (resp->status == RIO_STATUS_ERROR) {
            break;
        }
    }
}
