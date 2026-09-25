#ifndef MYOS_ARP_H
#define MYOS_ARP_H

#include <stdint.h>

int  arp_request(uint8_t our_mac[6], uint32_t our_ip, uint32_t target_ip,
                 uint8_t out_mac[6]);
void arp_test(uint8_t our_mac[6]);

#endif