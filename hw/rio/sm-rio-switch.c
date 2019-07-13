#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/register.h"
#include "qemu/bitops.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "net/net.h"
#include "qapi/qmp/qerror.h"
#include "hw/fdt_generic_util.h"
#include "hw/rio/sm-rio-switch.h"
#include "hw/rio/brc1-rio-endpt.h"

#include "packet.h"
#include "rio-dev.h"

#define SM_RIO_SWITCH_ERR_DEBUG 1

#ifndef SM_RIO_SWITCH_ERR_DEBUG
#define SM_RIO_SWITCH_ERR_DEBUG 0
#endif

#define CSR_DEBUG SM_RIO_SWITCH_ERR_DEBUG

#define LOG(fmt, ...) qemu_log("sm-rio-switch: " fmt, __VA_ARGS__)
#define LOG_ERR(fmt, ...) LOG("ERROR: " fmt, __VA_ARGS__)

#define IOMEM_REGION_SIZE   0x00200000 /* 2 MB */

#define PORT_INVALID 0xff

typedef struct NetSwitchPort {
    NetClientState nc;
    SMRIOSwitch *rio_switch;
    int port;
} NetSwitchPort;

typedef enum { /* User Guide Table 4 */
    RIO_MAPPING_TYPE_UNICAST   = 0b00,
    RIO_MAPPING_TYPE_MULTICAST = 0b01,
    RIO_MAPPING_TYPE_AGGREGATE = 0b10,
} RIOMappingType;

static int csr_read32(void *dev, hwaddr addr, uint32_t *value)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(dev);
    RegisterInfo *r;
    unsigned n;
    int rc = 0;

    switch (addr) {
        case A_PORT_MAPPING_TABLE_ENTRY_0 ... A_PORT_MAPPING_TABLE_ENTRY_255:
            n = (addr - A_PORT_MAPPING_TABLE_ENTRY_0) / sizeof(uint32_t);
            *value = s->mapping_table[n] & PORT_MAPPING_TABLE_ENTRY_MASK;
            break;
        default:
            if (addr < SM_RIO_SWITCH_REG_MAX * sizeof(uint32_t)) {
                r = &s->regs_info[addr / sizeof(uint32_t)];
                *value = register_read(r, ~0, TYPE_SM_RIO_SWITCH, CSR_DEBUG);
            } else if (addr < SM_RIO_SWITCH_CSR_SIZE) {
                qemu_log("RIO: NOTICE: read unimplemented register: 0x%lx\n",
                         addr);
            } else {
                qemu_log("RIO: register addr 0x%lx out of range\n", addr);
                rc = 1;
            }
    }
    return rc;
}

static int csr_write32(void *dev, hwaddr addr,
                       uint32_t value, uint32_t mask)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(dev);
    RegisterInfo *r;
    unsigned n;
    int rc = 0;

    switch (addr) {
        case A_PORT_MAPPING_TABLE_ENTRY_0 ... A_PORT_MAPPING_TABLE_ENTRY_255:
            n = (addr - A_PORT_MAPPING_TABLE_ENTRY_0) / sizeof(uint32_t);
            s->mapping_table[n] &= ~(mask & PORT_MAPPING_TABLE_ENTRY_MASK);
            s->mapping_table[n] |= (value & mask) & PORT_MAPPING_TABLE_ENTRY_MASK;
            break;
        default:
            if (addr < SM_RIO_SWITCH_REG_MAX * sizeof(uint32_t)) {
                r = &s->regs_info[addr / sizeof(uint32_t)];
                register_write(r, value, mask, TYPE_SM_RIO_SWITCH, CSR_DEBUG);
            } else if (addr < SM_RIO_SWITCH_CSR_SIZE) {
                qemu_log("RIO: NOTICE: wrote unimplemented register: 0x%lx\n",
                         addr);
            } else {
                qemu_log("RIO: NOTICE: register addr 0x%lx out of range\n",
                         addr);
                rc = 1;
            }
    }
    return rc;
}

static uint64_t sm_rio_switch_read(void *opaque, hwaddr addr, unsigned size)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(opaque);
    uint32_t value;
    int rc;
    assert(size == sizeof(uint32_t) && "not implemented"); /* TODO */
    rc = csr_read32(s, addr, &value);
    assert(!rc); /* model error: means mem region is misconfigured */
    return value;
}
static void sm_rio_switch_write(void *opaque, hwaddr addr, uint64_t value,
                                unsigned size)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(opaque);
    int rc;
    assert(size == sizeof(uint32_t) && "not implemented"); /* TODO */
    assert(value >> 32 == 0);
    rc = csr_write32(s, addr, value, ~0x0);
    assert(!rc); /* model error: means mem region is misconfigured */
}

static void send_tx(SMRIOSwitch *s, unsigned port, RIOTx *tx)
{
    /* TODO: abstract endpoint type */
    if (port < 2) {
        brc1_rio_endpt_in(s->ports[port], tx);
    } else {
        LOG("send %d byte packet\n", tx->payload_len);
        qemu_hexdump((const char *)tx->payload, stdout, "net", tx->payload_len);
        ssize_t rc = qemu_send_packet(&s->netports[port]->nc,
                (const uint8_t *)tx->payload, tx->payload_len);
        LOG("sent packet: rc %ld\n", rc);
    }
}

static void forward_tx(SMRIOSwitch *s, rio_devid_t dest, RIOTx *tx)
{
    bool valid;
    RIOMappingType type;
    unsigned p;
    uint8_t port_map;
    uint32_t map_entry;

    map_entry = s->mapping_table[dest];

    valid = FIELD_EX32(map_entry, PORT_MAPPING_TABLE_ENTRY, VALID);
    if (!valid) {
        qemu_log("RIO: packet to 0x%x dropped: no port map\n", dest);
        /* TODO: generate DestinationID Not Mapped event */
        return;
    }

    type = FIELD_EX32(map_entry, PORT_MAPPING_TABLE_ENTRY, TYPE);
    switch (type) {
        case RIO_MAPPING_TYPE_UNICAST:
        case RIO_MAPPING_TYPE_MULTICAST:
        case RIO_MAPPING_TYPE_AGGREGATE:
            /* nothing different to do (could validate bit count in
             * port map field against the type) */
            break;
        default:
            qemu_log("RIO: packet to 0x%x dropped: "
                     "invalid port mapping type 0x%x\n", dest, type);
            return;
    }

    assert(sizeof(port_map) * 8 >= R_PORT_MAPPING_TABLE_ENTRY_PORT_MAP_LENGTH);
    port_map = FIELD_EX32(map_entry, PORT_MAPPING_TABLE_ENTRY, PORT_MAP);
    assert(R_PORT_MAPPING_TABLE_ENTRY_PORT_MAP_LENGTH >=
                SM_RIO_SWITCH_MAX_PORTS);
    for (p = 0; p < SM_RIO_SWITCH_MAX_PORTS; ++p) {
        if (port_map & (1UL << p))
            send_tx(s, p, tx);
    }
}

static void handle_maint_access(SMRIOSwitch *s, RioPkt *req, RioPkt *resp)
{
    resp->ttype = req->ttype;
    resp->src_id = req->dest_id;
    resp->dest_id = req->src_id;

    resp->ftype = RIO_FTYPE_MAINT;
    resp->target_tid = req->src_tid;
    resp->hop_count = 0xFF; /* see Spec Part 3 2.5 */

	resp->status = RIO_STATUS_DONE; /* overwritten below in case of error */
    resp->payload_len = 0;

    switch (req->transaction) {
        case RIO_TRANS_MAINT_REQ_READ:
            resp->transaction = RIO_TRANS_MAINT_RESP_READ;
            rio_cfg_space_read(s, csr_read32, req, resp);
            break;
        case RIO_TRANS_MAINT_REQ_WRITE:
            resp->transaction = RIO_TRANS_MAINT_RESP_WRITE;
            rio_cfg_space_write(s, csr_write32, req, resp);
            break;
        default:
            abort(); /* this handler should not have been called */
    }
}

static void switch_maint_tx(SMRIOSwitch *s, RIOTx *tx, unsigned pos,
                            RioPkt *pkt, unsigned in_port)
{
    RioPkt resp; /* TODO: alloc only on branch when needed */
    uint8_t pkt_buf[PKT_BUF_WORDS * 4]; /* TODO: dyn alloc only as much as needed */
    unsigned len;
    int rc;

    /* Inefficient to unpack the whole packet, just for hopcount,
     * but we needed it unpacked to pack back up for transmission,
     * since we don't have direct field accessors. Not great. */
    rc = unpack_body(pkt, tx->payload, tx->payload_len, &pos);
    if (rc) {
        qemu_log("RIO: packet dropped: invalid body\n");
        return;
    }
    /* TODO: do this only for requests? */
    if (pkt->hop_count == 0) { /* handle and reply */
        handle_maint_access(s, pkt, &resp);

        len = pack_pkt(pkt_buf, sizeof(pkt_buf), &resp);
        log_pkt("sending maint response", pkt_buf, len);
        RIOTx out_tx = { .payload_len = len, .payload = (uint8_t *)&pkt_buf[0] };

        send_tx(s, in_port, &out_tx);
    } else { /* modify and forward */
        --pkt->hop_count;

        len = pack_pkt(pkt_buf, sizeof(pkt_buf), pkt);
        log_pkt("forwarding maint packet transmitting packet", pkt_buf, len);
        RIOTx out_tx = { .payload_len = len, .payload = (uint8_t *)&pkt_buf[0] };

        forward_tx(s, pkt->dest_id, &out_tx);
    }
}

void sm_rio_switch_in(SMRIOSwitch *s, unsigned in_port, RIOTx *tx)
{
    int rc;
    RioPkt pkt;
    unsigned pos = 0;

    assert(in_port < SM_RIO_SWITCH_MAX_PORTS);

    rc = unpack_header(&pkt, tx->payload, tx->payload_len, &pos);
    if (rc) {
        qemu_log("RIO: packet dropped: invalid header\n");
        return;
    }
    switch (pkt.ftype) {
        case RIO_FTYPE_MAINT:
            switch_maint_tx(s, tx, pos, &pkt, in_port);
            break;
        default: /* forward unmodified */
            forward_tx(s, pkt.dest_id, tx);
    }
}

static int rio_net_client_can_receive(NetClientState *nc)
{
   return 1;
}

static ssize_t rio_net_client_receive_iov(NetClientState *nc,
                                        const struct iovec *iov, int iovcnt)
{
    int i;
    size_t len = 0;
    NetSwitchPort *np = DO_UPCAST(NetSwitchPort, nc, nc);
    qemu_log("RIO: NETTEST: receive iov\n");
    for (i = 0; i < iovcnt; ++i) {
        qemu_hexdump((const char *)iov[i].iov_base, stdout, "sm-rio-switch",
                iov[i].iov_len);
        len += iov[i].iov_len;

        RIOTx tx = {
            .payload = iov[i].iov_base,
            .payload_len = iov[i].iov_len,
        };
        sm_rio_switch_in(np->rio_switch, np->port, &tx);
    }
    return len;
}

static void rio_net_client_cleanup(NetClientState *nc)
{
   qemu_log("RIO: NETTEST: cleanup\n");
}

static NetClientInfo rio_net_client_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NetSwitchPort),
    .can_receive = rio_net_client_can_receive,
    .receive_iov = rio_net_client_receive_iov,
    .cleanup = rio_net_client_cleanup,
};

static const RegisterAccessInfo sm_rio_switch_regs_info[] = {
    /* registers that form an array are handled dynamically in r/w callbacks */
};

static const MemoryRegionOps sm_rio_switch_ops = {
    .read = sm_rio_switch_read,
    .write = sm_rio_switch_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};


static void sm_rio_switch_reset(DeviceState *dev)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(dev);
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(s->regs_info); ++i) {
        register_reset(&s->regs_info[i]);
    }
    bzero(s->mapping_table, sizeof(s->mapping_table));
}

static void sm_rio_switch_realize(DeviceState *dev, Error **errp)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(dev);
    NetClientState *nd, *nc;
    NetSwitchPort *np;
    const char *nd_id;
    unsigned int i, p, reg_idx;
    unsigned num_regs = ARRAY_SIZE(sm_rio_switch_regs_info);

    for (i = 0; i < num_regs; ++i) {
        reg_idx = sm_rio_switch_regs_info[i].addr / sizeof(uint32_t);
        RegisterInfo *r = &s->regs_info[reg_idx];

        *r = (RegisterInfo) {
            .data = (uint8_t *)&s->regs[reg_idx],
            .data_size = sizeof(uint32_t),
            .access = &sm_rio_switch_regs_info[i],
            .opaque = s,
        };
    }

    for (p = 2 /* TODO */; p < s->num_ports; ++p) {
        nd_id = s->netdev_ids[p];
        if (!nd_id) {
            continue;
        }
        nd = qemu_find_netdev(nd_id);
        if (!nd) {
            error_setg(errp, "netdev '%s' not found", nd_id);
            return;
        }
        nc = qemu_new_net_client(&rio_net_client_info, nd, NULL,
                                 memory_region_name(&s->iomem));
        if (!nc) {
            error_setg(errp, "failed to create net client");
            return;
        }
        np = DO_UPCAST(NetSwitchPort, nc, nc);
        np->rio_switch = s;
        np->port = p;
        s->netports[p] = np;
    }
}

static void sm_rio_switch_init(Object *obj)
{
    SMRIOSwitch *s = SM_RIO_SWITCH(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    
    memory_region_init_io(&s->iomem, obj, &sm_rio_switch_ops,
                          s, TYPE_SM_RIO_SWITCH, IOMEM_REGION_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->event_irq);
}

static const VMStateDescription vmstate_sm_rio_switch = {
    .name = TYPE_SM_RIO_SWITCH,
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, SMRIOSwitch, SM_RIO_SWITCH_REG_MAX),
        VMSTATE_END_OF_LIST(),
    }
};

static Property sm_rio_switch_properties[] = {
#if 0
    DEFINE_PROP_LINK("port", SMRIOSwitch, port,
                     TYPE_BRC1_RIO_PORT, RIOPort *),
#else
    // TODO: array
    DEFINE_PROP_LINK("port0", SMRIOSwitch, ports[0],
                     TYPE_BRC1_RIO_ENDPT, BRC1RIOEndpt *),
    DEFINE_PROP_LINK("port1", SMRIOSwitch, ports[1],
                     TYPE_BRC1_RIO_ENDPT, BRC1RIOEndpt *),
#endif
    /* TODO: type */
    DEFINE_PROP_STRING("port2", SMRIOSwitch, netdev_ids[2]),
    DEFINE_PROP_STRING("port3", SMRIOSwitch, netdev_ids[3]),
    DEFINE_PROP_UINT8("num-ports", SMRIOSwitch, num_ports, 4),
    DEFINE_PROP_END_OF_LIST(),
};

static void sm_rio_switch_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = sm_rio_switch_reset;
    dc->realize = sm_rio_switch_realize;
    dc->props = sm_rio_switch_properties;
    dc->vmsd = &vmstate_sm_rio_switch;
}

static const TypeInfo sm_rio_switch_info = {
    .name          = TYPE_SM_RIO_SWITCH,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SMRIOSwitch),
    .class_init    = sm_rio_switch_class_init,
    .instance_init = sm_rio_switch_init,
};

static void sm_rio_switch_register_types(void)
{
    type_register_static(&sm_rio_switch_info);
}

type_init(sm_rio_switch_register_types)
