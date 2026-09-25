#ifndef MYOS_E1000_H
#define MYOS_E1000_H

#include <stdint.h>

void e1000_probe(uint64_t mmio_base);
void e1000_get_mac(uint8_t mac[6]);
int  e1000_init(void);
int  e1000_send(const void* data, int len);
int  e1000_recv(void* buf, int max);
void e1000_send_arp_request(void);
uint32_t e1000_debug_rdh(void);
uint32_t e1000_debug_rdt(void);

#endif