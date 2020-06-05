#include "lwip/opt.h"
#include "lwip/ip.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

#include "lwip/ip6_routing.h"

static std::vector< struct rt_entry > ip_rt_table;
static std::pair< struct rt_entry, bool > default_gw;

bool cmp_masks(const struct rt_entry& a, const struct rt_entry& b) {
	return a.mask > b.mask;
}

std::string chunk_to_rest(uint8_t chunk) {
	switch (chunk) {
		case 3:
			return "e::";
		case 2:
			return "c::";
		case 1:
			return "8::";
		case 0:
			return ":";
		default:
			std::cerr << "chunk_to_rest got something wrong\n";
			return "";
	}
}

void mask_to_address(uint8_t mask, ip6_addr_t* m) {
	if (mask == 128) {
		ip6addr_aton("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", m);
		return;
	} else if (mask == 0) {
		ip6_addr_set_zero(m);
		return;
	}

	std::ostringstream s;
	
	while (mask >= 16) {
		s << "ffff:";
		mask = mask - 16;
	}

	while (mask > 4) {
		s << "f";
		mask = mask - 4;
		if (mask == 0) {
			s << ":";
		}
	}

	s << chunk_to_rest(mask);
	ip6addr_aton(s.str().c_str(), m);
}

void print_ip(const ip6_addr_t* ip) {
	printf("ip6: %s\n", ip6addr_ntoa(ip));
}

void ip_print_table() {
	if ( !default_gw.second ) {
		std::cout << "default gateway not set\n";
	} else {
		std::cout << "default gateway set - ip: " << ip6addr_ntoa( &default_gw.first.addr ) << " gw: " << default_gw.first.gw_name << "\n"; 
	}
	for (const auto& rec : ip_rt_table) {
		std::cout << "ip: " << ip6addr_ntoa(&rec.addr) << "/" << static_cast<int>(rec.mask)
		    << " gw: " << rec.gw_name << "\n";
	}
}

void unmask(ip6_addr_t& ip, const ip6_addr_t& mask) {
	//std::cout << "ip for unmask"; print_ip(&ip);
	//std::cout << "mask for unmask"; print_ip(&mask);

	for (int i = 0; i < 4; i++)
		ip.addr[i] &= mask.addr[i];
}

void fill_rt_entry(struct rt_entry* r, const ip6_addr_t* addr, uint8_t mask, const char* netif_name) {
	ip6_addr_copy(r->addr, *addr);
	r->mask = mask;
	memcpy(r->gw_name, netif_name, GW_NAME_SIZE);
}



bool ip_add_route(const ip6_addr_t* addr, uint8_t mask, const char* netif_name) {
	auto entry = ip_find_route(addr);

	if (entry == nullptr) {
		struct rt_entry r;
		fill_rt_entry(&r, addr, mask, netif_name);
		ip_rt_table.push_back(r);
		std::sort(ip_rt_table.begin(), ip_rt_table.end(), cmp_masks);
		return true;
	}

  	return false;
}


struct netif* ip_find_route(const ip6_addr_t* ip) {
	for (auto& rec : ip_rt_table) {
		ip6_addr_t unmasked, mask;
		mask_to_address(rec.mask, &mask);
		ip6_addr_copy(unmasked, *ip);
		unmask(unmasked, mask);
		//printf("ip zone - %d | mask zone - %d | unmasked zone - %d | rec zone - %d\n", ip6_addr_zone(ip), ip6_addr_zone(&mask), ip6_addr_zone(&unmasked), ip6_addr_zone(&rec.addr));
		if (ip6_addr_cmp_zoneless(&unmasked, &rec.addr)) {
			return netif_find(rec.gw_name);
    	}
  	}

	if ( default_gw.second )
		return netif_find( default_gw.first.gw_name );

	return nullptr;
}

bool ip_rm_route(const ip6_addr_t* ip, uint8_t mask, const char* netif_name) {
	for (auto it = ip_rt_table.begin(); it != ip_rt_table.end(); it++) {
		if (ip6_addr_cmp(&it->addr, ip) && it->mask == mask) {
			ip_rt_table.erase(it);
			return true;
		}
	}
	return false;
}

bool ip_rm_route_if(const char* netif_name) {
	int size = ip_rt_table.size();

	ip_rt_table.erase( std::remove_if( ip_rt_table.begin(), ip_rt_table.end(), [&netif_name](rt_entry& r) {
			return strcmp(netif_name, r.gw_name) == 0;
		} ), ip_rt_table.end() );
	
	return size != ip_rt_table.size();
}

void ip_update_route(const ip6_addr_t* ip, uint8_t mask, const char* new_gw) {
	int i = 0;
	while (ip_rt_table[i].mask <= mask) {
		if (mask == ip_rt_table[i].mask && ip6_addr_cmp_zoneless(&ip_rt_table[i].addr, ip)) {
			memcpy(ip_rt_table[i].gw_name, new_gw, GW_NAME_SIZE);
			break;
		}
		i++;
	}
}

bool set_default_gw(const ip6_addr_t* addr, uint8_t mask, const char* netif_name) {
	if (default_gw.second) {
		return false;
	}

	fill_rt_entry(&default_gw.first, addr, mask, netif_name);
	default_gw.second = true;
	return true;
}

void remove_default_gw() {
	default_gw.second = false;
}
