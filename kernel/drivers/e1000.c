#include "e1000.h"
#include "serial.h"
#include "pmm.h"
#include <stdint.h>

/* ===== 寄存器 ===== */
#define E1000_CTRL      0x0000
#define E1000_STATUS    0x0008
#define E1000_ICR       0x00C0
#define E1000_IMS       0x00D0
#define E1000_IMC       0x00D8
#define E1000_RCTL      0x0100
#define E1000_TCTL      0x0400
#define E1000_RDBAL     0x2800
#define E1000_RDBAH     0x2804
#define E1000_RDLEN     0x2808
#define E1000_RDH       0x2810
#define E1000_RDT       0x2818
#define E1000_TDBAL     0x3800
#define E1000_TDBAH     0x3804
#define E1000_TDLEN     0x3808
#define E1000_TDH       0x3810
#define E1000_TDT       0x3818
#define E1000_MTA       0x5200
#define E1000_RAL       0x5400
#define E1000_RAH       0x5404

#define NUM_RX_DESC 16
#define NUM_TX_DESC 16

uint8_t gateway_mac[6] = {0};

/* 描述符（legacy，非扩展） */
typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) tx_desc_t;

static uint64_t mmio = 0;
static uint8_t  mac[6] = {0};

static uint64_t rx_desc_phys = 0;
static uint64_t tx_desc_phys = 0;
static uint64_t rx_buf_phys[NUM_RX_DESC];
static uint64_t tx_buf_phys[NUM_TX_DESC];

static int rx_cur = 0;
static int tx_cur = 0;

static inline uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t*)(uintptr_t)(mmio + reg);
}
static inline void e1000_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(uintptr_t)(mmio + reg) = val;
}

void e1000_get_mac(uint8_t out[6]) {
    for (int i = 0; i < 6; i++) out[i] = mac[i];
}

/* ===== Probe ===== */
void e1000_probe(uint64_t mmio_base) {
    mmio = mmio_base;
    uint32_t status = e1000_read(E1000_STATUS);
    serial_printf("E1000: STATUS = 0x%08x\n", status);

    uint32_t ral = e1000_read(E1000_RAL);
    uint32_t rah = e1000_read(E1000_RAH);
    mac[0] = (uint8_t)(ral & 0xFF);
    mac[1] = (uint8_t)((ral >> 8) & 0xFF);
    mac[2] = (uint8_t)((ral >> 16) & 0xFF);
    mac[3] = (uint8_t)((ral >> 24) & 0xFF);
    mac[4] = (uint8_t)(rah & 0xFF);
    mac[5] = (uint8_t)((rah >> 8) & 0xFF);

    serial_printf("E1000: MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    serial_printf("E1000: CTRL = 0x%08x\n", e1000_read(E1000_CTRL));
}

/* ===== 初始化 ===== */
int e1000_init(void) {
    /* 1. 分配描述符环（各 1 页）和缓冲区（每描述符 1 页） */
    rx_desc_phys = pmm_alloc_page();
    tx_desc_phys = pmm_alloc_page();
    if (!rx_desc_phys || !tx_desc_phys) {
        serial_printf("E1000: out of memory (rings)\n");
        return -1;
    }
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_buf_phys[i] = pmm_alloc_page();
        if (!rx_buf_phys[i]) { serial_printf("E1000: OOM rx buf\n"); return -1; }
    }
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_buf_phys[i] = pmm_alloc_page();
        if (!tx_buf_phys[i]) { serial_printf("E1000: OOM tx buf\n"); return -1; }
    }

    rx_desc_t* rx_ring = (rx_desc_t*)(uintptr_t)rx_desc_phys;
    tx_desc_t* tx_ring = (tx_desc_t*)(uintptr_t)tx_desc_phys;

    /* 2. 清空描述符环 */
    for (int i = 0; i < 4096; i++) {
        ((uint8_t*)rx_ring)[i] = 0;
        ((uint8_t*)tx_ring)[i] = 0;
    }

    /* 3. 初始化 RX 描述符：每个指向自己的 4KB buffer */
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_ring[i].addr   = rx_buf_phys[i];
        rx_ring[i].status = 0;
    }

    /* 4. 初始化 TX 描述符 */
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_ring[i].addr   = tx_buf_phys[i];
        tx_ring[i].status = 0;
    }

    /* 5. 屏蔽所有中断 */
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    /* 6. 清 MTA */
    for (int i = 0; i < 128; i++) e1000_write(E1000_MTA + i * 4, 0);

    /* 7. 配置 RX 环 */
    e1000_write(E1000_RDBAL, (uint32_t)(rx_desc_phys & 0xFFFFFFFF));
    e1000_write(E1000_RDBAH, (uint32_t)(rx_desc_phys >> 32));
    e1000_write(E1000_RDLEN, NUM_RX_DESC * 16);
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, NUM_RX_DESC - 1);

    /* 8. 配置 TX 环 */
    e1000_write(E1000_TDBAL, (uint32_t)(tx_desc_phys & 0xFFFFFFFF));
    e1000_write(E1000_TDBAH, (uint32_t)(tx_desc_phys >> 32));
    e1000_write(E1000_TDLEN, NUM_TX_DESC * 16);
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    /* 9. 拉 link up（QEMU 下没有自动协商） */
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | (1u << 6));   /* SLU */

    /* 10. RAH: AV 位必须置位，否则拒收所有单播 */
    uint32_t rah = e1000_read(E1000_RAH);
    e1000_write(E1000_RAH, rah | (1u << 31));   /* AV=1 */

    /* 11. 配置 RCTL: EN | UPE | MPE | BAM | SECRC
           混杂模式：所有包都收，避免硬件过滤掉 ARP 应答 */
    uint32_t rctl = (1u << 1)    /* EN   */
                  | (1u << 3)    /* UPE  */
                  | (1u << 4)    /* MPE  */
                  | (1u << 15)   /* BAM  */
                  | (1u << 26);  /* SECRC */
    e1000_write(E1000_RCTL, rctl);

    /* 12. 配置 TCTL: EN | PSP | CT=0x10 | COLD=0x40 */
    uint32_t tctl = (1u << 1)
                  | (1u << 3)
                  | (0x0F << 4)
                  | (0x40u << 12);
    e1000_write(E1000_TCTL, tctl);

    serial_printf("E1000: RX/TX rings initialized\n");
    serial_printf("E1000: RCTL=0x%08x TCTL=0x%08x\n",
                  e1000_read(E1000_RCTL), e1000_read(E1000_TCTL));
    serial_printf("E1000: RDBAL=0x%08x TDBAL=0x%08x\n",
                  e1000_read(E1000_RDBAL), e1000_read(E1000_TDBAL));

    rx_cur = 0;
    tx_cur = 0;
    return 0;
}

/* ===== 发送 ===== */
int e1000_send(const void* data, int len) {
    if (len <= 0 || len > 4096) return -1;

    tx_desc_t* tx_ring = (tx_desc_t*)(uintptr_t)tx_desc_phys;
    uint8_t* buf = (uint8_t*)(uintptr_t)tx_ring[tx_cur].addr;
    const uint8_t* src = (const uint8_t*)data;
    for (int i = 0; i < len; i++) buf[i] = src[i];

    tx_ring[tx_cur].length = (uint16_t)len;
    tx_ring[tx_cur].cmd    = (1 << 0) | (1 << 1) | (1 << 3);   /* EOP | IFCS | RS */
    tx_ring[tx_cur].status = 0;

    int idx = tx_cur;
    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
    e1000_write(E1000_TDT, tx_cur);

    /* QEMU 的 e1000 处理很快，等一小会儿即可，不等 DD 位 */
    for (volatile int t = 0; t < 500000; t++);
    (void)idx;
    return 0;    
}

/* ===== 接收 ===== */
int e1000_recv(void* out, int max) {
    rx_desc_t* rx_ring = (rx_desc_t*)(uintptr_t)rx_desc_phys;

    uint32_t rdh = e1000_read(E1000_RDH);
    if (rdh == (uint32_t)rx_cur) return -1;

    rx_desc_t* d = &rx_ring[rx_cur];
    if (!(d->status & 0x01)) return -1;   /* DD 未置位 */

    int len = d->length;
    if (len > max) len = max;

    uint8_t* src = (uint8_t*)(uintptr_t)d->addr;
    uint8_t* dst = (uint8_t*)out;
    for (int i = 0; i < len; i++) dst[i] = src[i];

    d->status = 0;
    e1000_write(E1000_RDT, rx_cur);
    rx_cur = (rx_cur + 1) % NUM_RX_DESC;

    return len;
}

/* ===== 发一个 ARP 请求 ===== */
void e1000_send_arp_request(void) {
    uint8_t pkt[42];
    for (int i = 0; i < 42; i++) pkt[i] = 0;

    /* 以太网头 */
    for (int i = 0; i < 6; i++) pkt[i] = 0xFF;          /* 广播 */
    for (int i = 0; i < 6; i++) pkt[6 + i] = mac[i];    /* 源 MAC */
    pkt[12] = 0x08; pkt[13] = 0x06;                     /* ARP EtherType */

    /* ARP 头 */
    pkt[14] = 0x00; pkt[15] = 0x01;   /* Hardware type: Ethernet */
    pkt[16] = 0x08; pkt[17] = 0x00;   /* Protocol type: IPv4 */
    pkt[18] = 6;    pkt[19] = 4;      /* HLEN=6, PLEN=4 */
    pkt[20] = 0x00; pkt[21] = 0x01;   /* Opcode: request */

    /* Sender MAC */
    for (int i = 0; i < 6; i++) pkt[22 + i] = mac[i];
    /* Sender IP: 10.0.2.15 */
    pkt[28] = 10; pkt[29] = 0; pkt[30] = 2; pkt[31] = 15;
    /* Target MAC: 全 0 */
    for (int i = 0; i < 6; i++) pkt[32 + i] = 0;
    /* Target IP: 10.0.2.2 */
    pkt[38] = 10; pkt[39] = 0; pkt[40] = 2; pkt[41] = 2;

    serial_printf("E1000: sending ARP who-has 10.0.2.2\n");
    if (e1000_send(pkt, 42) < 0) {
        serial_printf("E1000: ARP send failed\n");
    } else {
        serial_printf("E1000: ARP sent\n");
    }
}

static uint16_t icmp_checksum(const void* data, int len) {
    const uint16_t* p = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}


/* ===== 简易 IP 校验和 ===== */
static uint16_t ip_checksum(const void* data, int len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += ((uint16_t)p[0] << 8) | p[1];   /* big-endian */
        p += 2;
        len -= 2;
    }
    if (len) sum += ((uint16_t)p[0] << 8);     /* 奇数尾字节 */
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/* ===== 发 ICMP Echo Request ===== */
void e1000_ping(uint32_t dst_ip_be) {
    uint8_t pkt[98];
    for (int i = 0; i < 98; i++) pkt[i] = 0;

    /* --- 以太网头 (14 字节) --- */
    for (int i = 0; i < 6; i++) pkt[i] = gateway_mac[i];   /* 目的 MAC = 网关 */
    for (int i = 0; i < 6; i++) pkt[6 + i] = mac[i];        /* 源 MAC = 自己 */
    pkt[12] = 0x08; pkt[13] = 0x00;                          /* IPv4 */

    /* --- IP 头 (20 字节, 从偏移 14 开始) --- */
    pkt[14] = 0x45;                  /* version=4, IHL=5 */
    pkt[15] = 0x00;                  /* DSCP=0 */
    pkt[16] = 0x00; pkt[17] = 0x54;  /* total length = 84 */
    pkt[18] = 0x00; pkt[19] = 0x01;  /* ID */
    pkt[20] = 0x00; pkt[21] = 0x00;  /* flags/frag */
    pkt[22] = 64;                    /* TTL */
    pkt[23] = 0x01;                  /* protocol = ICMP */
    /* checksum 在 24-25，先留 0 */
    pkt[26] = 10; pkt[27] = 0; pkt[28] = 2; pkt[29] = 15;   /* src = 10.0.2.15 */
    pkt[30] = (dst_ip_be >> 24) & 0xFF;
    pkt[31] = (dst_ip_be >> 16) & 0xFF;
    pkt[32] = (dst_ip_be >> 8)  & 0xFF;
    pkt[33] =  dst_ip_be        & 0xFF;

    uint16_t ips = ip_checksum(pkt + 14, 20);
    pkt[24] = (ips >> 8) & 0xFF;
    pkt[25] =  ips       & 0xFF;

    /* --- ICMP Echo Request (从偏移 34 开始) --- */
    pkt[34] = 0x08;                  /* type = 8 (echo request) */
    pkt[35] = 0x00;                  /* code = 0 */
    /* checksum 在 36-37，先留 0 */
    pkt[38] = 0x00; pkt[39] = 0x01;  /* identifier */
    pkt[40] = 0x00; pkt[41] = 0x01;  /* sequence */
    for (int i = 42; i < 98; i++) pkt[i] = (uint8_t)i;   /* 填充数据 */

    uint16_t ics = ip_checksum(pkt + 34, 64);
    pkt[36] = (ics >> 8) & 0xFF;
    pkt[37] =  ics       & 0xFF;

    serial_printf("ICMP: sending echo request to 10.0.2.2\n");
    if (e1000_send(pkt, 98) < 0) {
        serial_printf("ICMP: send failed\n");
    } else {
        serial_printf("ICMP: echo request sent\n");
    }
}

/* ===== 发送 UDP 包（含 Ethernet + IP + UDP 头） ===== */
static int e1000_send_udp(uint32_t dst_ip_be, uint16_t src_port, uint16_t dst_port,
                          const void* payload, int payload_len)
{
    int total = 14 + 20 + 8 + payload_len;
    if (total > 1500) return -1;

    uint8_t pkt[1500];
    for (int i = 0; i < total; i++) pkt[i] = 0;

    /* --- Ethernet --- */
    for (int i = 0; i < 6; i++) pkt[i] = gateway_mac[i];
    for (int i = 0; i < 6; i++) pkt[6 + i] = mac[i];
    pkt[12] = 0x08; pkt[13] = 0x00;

    /* --- IP --- */
    int ip_total = 20 + 8 + payload_len;
    pkt[14] = 0x45;
    pkt[15] = 0x00;
    pkt[16] = (ip_total >> 8) & 0xFF;
    pkt[17] = ip_total & 0xFF;
    pkt[18] = 0x00; pkt[19] = 0x02;   /* ID */
    pkt[20] = 0x00; pkt[21] = 0x00;
    pkt[22] = 64;                     /* TTL */
    pkt[23] = 0x11;                   /* UDP = 17 */
    pkt[26] = 10; pkt[27] = 0; pkt[28] = 2; pkt[29] = 15;
    pkt[30] = (dst_ip_be >> 24) & 0xFF;
    pkt[31] = (dst_ip_be >> 16) & 0xFF;
    pkt[32] = (dst_ip_be >> 8)  & 0xFF;
    pkt[33] =  dst_ip_be        & 0xFF;
    uint16_t ips = ip_checksum(pkt + 14, 20);
    pkt[24] = (ips >> 8) & 0xFF;
    pkt[25] =  ips       & 0xFF;

    /* --- UDP --- */
    pkt[34] = (src_port >> 8) & 0xFF;
    pkt[35] =  src_port       & 0xFF;
    pkt[36] = (dst_port >> 8) & 0xFF;
    pkt[37] =  dst_port       & 0xFF;
    int udp_len = 8 + payload_len;
    pkt[38] = (udp_len >> 8) & 0xFF;
    pkt[39] =  udp_len       & 0xFF;
    pkt[40] = 0x00; pkt[41] = 0x00;   /* UDP checksum = 0（IPv4 可选） */

    /* --- Payload --- */
    const uint8_t* src = (const uint8_t*)payload;
    for (int i = 0; i < payload_len; i++) pkt[42 + i] = src[i];

    return e1000_send(pkt, total);
}

/* ===== 编码 DNS 域名：example.com -> 07 e x a m p l e 03 c o m 00 ===== */
static int dns_encode_name(uint8_t* out, const char* name) {
    int pos = 0;
    const char* p = name;
    while (*p) {
        const char* start = p;
        while (*p && *p != '.') p++;
        int len = (int)(p - start);
        if (len > 63) return -1;
        out[pos++] = (uint8_t)len;
        for (int i = 0; i < len; i++) out[pos++] = (uint8_t)start[i];
        if (*p == '.') p++;
    }
    out[pos++] = 0;
    return pos;
}

/* ===== 发 DNS 查询 ===== */
void e1000_dns_query(const char* hostname) {
    uint8_t dns[256];
    for (int i = 0; i < 256; i++) dns[i] = 0;

    /* DNS 头（12 字节） */
    dns[0] = 0xAB; dns[1] = 0xCD;     /* 事务 ID */
    dns[2] = 0x01; dns[3] = 0x00;     /* Flags: standard query, RD=1 */
    dns[4] = 0x00; dns[5] = 0x01;     /* QDCOUNT = 1 */
    /* ANCOUNT / NSCOUNT / ARCOUNT = 0 */

    /* 问题段 */
    int nlen = dns_encode_name(dns + 12, hostname);
    if (nlen < 0) {
        serial_printf("DNS: bad hostname\n");
        return;
    }
    int pos = 12 + nlen;
    dns[pos++] = 0x00; dns[pos++] = 0x01;   /* QTYPE = A */
    dns[pos++] = 0x00; dns[pos++] = 0x01;   /* QCLASS = IN */

    serial_printf("DNS: querying %s (%d bytes)\n", hostname, pos);

    /* 发到 10.0.2.3:53 */
    e1000_send_udp(0x0A000203, 12345, 53, dns, pos);
}

/* ===== 解析 DNS 应答 ===== */
int e1000_dns_parse_reply(const uint8_t* dns, int len, uint8_t out_ip[4]) {
    if (len < 12) return -1;

    /* 检查事务 ID */
    if (dns[0] != 0xAB || dns[1] != 0xCD) return -1;

    uint16_t flags    = (dns[2] << 8) | dns[3];
    uint16_t qdcount  = (dns[4] << 8) | dns[5];
    uint16_t ancount  = (dns[6] << 8) | dns[7];

    if (!(flags & 0x8000)) return -1;     /* 必须是应答 */
    if ((flags & 0x000F) != 0) return -1; /* RCODE != 0 表示出错 */

    int pos = 12;

    /* 跳过问题段 */
    for (int q = 0; q < qdcount; q++) {
        while (pos < len && dns[pos] != 0) {
            if ((dns[pos] & 0xC0) == 0xC0) { pos += 2; break; }
            pos += 1 + dns[pos];
        }
        if (pos < len && dns[pos] == 0) pos++;
        pos += 4;   /* QTYPE + QCLASS */
    }

    /* 读应答段 */
    for (int a = 0; a < ancount && pos < len; a++) {
        /* 跳过 name（可能是指针） */
        if ((dns[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (pos < len && dns[pos] != 0) pos += 1 + dns[pos];
            if (pos < len) pos++;
        }

        if (pos + 10 > len) return -1;
        uint16_t type   = (dns[pos] << 8) | dns[pos + 1];
        uint16_t rdlen  = (dns[pos + 8] << 8) | dns[pos + 9];
        pos += 10;

        if (type == 0x0001 && rdlen == 4) {   /* A 记录 */
            out_ip[0] = dns[pos];
            out_ip[1] = dns[pos + 1];
            out_ip[2] = dns[pos + 2];
            out_ip[3] = dns[pos + 3];
            return 0;
        }
        pos += rdlen;
    }
    return -1;
}

uint32_t e1000_debug_rdh(void) { return e1000_read(E1000_RDH); }
uint32_t e1000_debug_rdt(void) { return e1000_read(E1000_RDT); }