#include "arp.h"
#include "e1000.h"
#include "serial.h"
#include <stdint.h>

static void put16(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xFF; }
static void put32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24; p[1] = (v >> 16) & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}
static uint16_t get16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
static uint32_t get32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | p[3];
}

int arp_request(uint8_t our_mac[6], uint32_t our_ip, uint32_t target_ip,
                uint8_t out_mac[6]) {
    uint8_t pkt[42];
    /* 以太头 */
    for (int i = 0; i < 6; i++) pkt[i] = 0xFF;       /* 广播 */
    for (int i = 0; i < 6; i++) pkt[6 + i] = our_mac[i];
    put16(pkt + 12, 0x0806);                          /* ARP */

    /* ARP 头 */
    put16(pkt + 14, 0x0001);   /* HW type = Ethernet */
    put16(pkt + 16, 0x0800);   /* Protocol = IPv4 */
    pkt[18] = 6;               /* HW len */
    pkt[19] = 4;               /* Proto len */
    put16(pkt + 20, 0x0001);   /* Opcode = request */
    for (int i = 0; i < 6; i++) pkt[22 + i] = our_mac[i];
    put32(pkt + 28, our_ip);
    for (int i = 0; i < 6; i++) pkt[32 + i] = 0;      /* target MAC = 0 */
    put32(pkt + 38, target_ip);

    serial_printf("ARP: sending request for 0x%08x\n", target_ip);
    if (e1000_send(pkt, 42) < 0) return -1;

    for (int t = 0; t < 1000000; t++) {
        uint8_t rbuf[2048];
        int n = e1000_recv(rbuf, sizeof(rbuf));
        if (n < 42) continue;

        /* 检查以太类型是 ARP */
        if (get16(rbuf + 12) != 0x0806) continue;
        /* Opcode = reply */
        if (get16(rbuf + 20) != 0x0002) continue;
        /* sender IP 匹配 */
        if (get32(rbuf + 28) != target_ip) continue;

        for (int i = 0; i < 6; i++) out_mac[i] = rbuf[22 + i];
        serial_printf("ARP: reply from %02x:%02x:%02x:%02x:%02x:%02x\n",
                      out_mac[0], out_mac[1], out_mac[2],
                      out_mac[3], out_mac[4], out_mac[5]);
        return 0;
    }
    serial_printf("ARP: timeout\n");
    return -1;
}

void arp_test(uint8_t our_mac[6]) {
    /* QEMU SLIRP: 网关 10.0.2.2, 自己 10.0.2.15 */
    uint32_t our_ip    = (10 << 24) | (0 << 16) | (2 << 8) | 15;
    uint32_t gateway   = (10 << 24) | (0 << 16) | (2 << 8) | 2;
    uint8_t  gw_mac[6];
    arp_request(our_mac, our_ip, gateway, gw_mac);
}