#ifndef MYOS_E1000_H
#define MYOS_E1000_H

#include <stdint.h>

void e1000_probe(uint64_t mmio_base);
void e1000_get_mac(uint8_t mac[6]);

#endif