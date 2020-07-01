#pragma once

#ifndef LWIP_HDR_IP6_ROUTING_H
#define LWIP_HDR_IP6_ROUTING_H

#include "lwip/opt.h"
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GW_NAME_SIZE 5

struct rt_entry {
	ip6_addr_t addr;
	uint8_t mask;
	char gw_name[GW_NAME_SIZE];
};


int ip_add_route(const ip6_addr_t* ip, uint8_t mask, const char* gw);

struct netif* ip_find_route(const ip6_addr_t* ip);

int ip_rm_route(const ip6_addr_t* ip, uint8_t mask);

int ip_rm_route_if(const char* netif_name);

void ip_update_route(const ip6_addr_t* ip, uint8_t mask, const char* new_gw);

void ip_print_table();

void print_ip(const ip6_addr_t* ip);

void mask_to_address(uint8_t mask, ip6_addr_t* m);

int set_default_gw(const ip6_addr_t* addr, uint8_t mask, const char* netif_name);
void remove_default_gw();

#ifdef __cplusplus
}
#endif


#endif
