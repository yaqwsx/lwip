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

int ip_size() {
  return ip_rt_table.size();
}

int cmp_masks(const struct rt_entry& a, const struct rt_entry& b) {
	return a.mask > b.mask;
}

std::string chunk_to_rest(uint8_t chunk, bool b) {
	std::string s;
	switch (chunk) {
		case 3:
			s = "e";
			break;
		case 2:
			s = "c";
			break;
		case 1:
			s = "8";
			break;
		default:
			return b ? "" : ":";
	}
	return b ? s : s.append("::");
}

void mask_to_address(uint8_t mask, ip6_addr_t* m) {
	ip6_addr_set_zero(m);
	bool edge_segment = mask > 112 || mask <= 16;
	std::ostringstream s;

	while (mask >= 16) {
		s << "ffff:";
		mask = mask - 16;
	}

	while (mask >= 4) {
		s << "f";
		mask = mask - 4;
		if (mask == 0 && !edge_segment) {
			s << ":";
		}
	}

	s << chunk_to_rest(mask, edge_segment);
	ip6addr_aton(s.str().c_str(), m);
}

void ip_print_table() {
	for (const auto& rec : ip_rt_table) {
		std::cout << "ip: " << ip6addr_ntoa(&rec.addr) << "/" << static_cast<int>(rec.mask)
		    << " gw: " << rec.gw_name << "\n";
	}
}


void ip6_addr_and(ip6_addr_t& ip, const ip6_addr_t& mask) {
	for (int i = 0; i < 4; i++)
		ip.addr[i] &= mask.addr[i];
}

void ip_mask_and(const ip6_addr_t* ip, uint8_t mask, ip6_addr_t* result) {
	ip6_addr_t ip_mask;
	mask_to_address(mask, &ip_mask);
	ip6_addr_copy(*result, *ip);
	ip6_addr_and(*result, ip_mask);
}

void fill_rt_entry(struct rt_entry* r, const ip6_addr_t* addr, uint8_t mask, const char* netif_name) {
	ip6_addr_t ip;
	ip_mask_and(addr, mask, &ip);
	ip6_addr_copy(r->addr, ip);
	r->mask = mask;
	memcpy(r->gw_name, netif_name, GW_NAME_SIZE);
}



int ip_add_route(const ip6_addr_t* addr, uint8_t mask, const char* netif_name) {
	auto entry = ip_find_route(addr);

	if (entry == nullptr) {
		struct rt_entry r;
		fill_rt_entry(&r, addr, mask, netif_name);
		ip_rt_table.push_back(r);
		std::sort(ip_rt_table.begin(), ip_rt_table.end(), cmp_masks);
		return 1;
	}

  	return 0;
}


struct netif* ip_find_route(const ip6_addr_t* ip) {
	ip6_addr_t searched;
	for (auto& rec : ip_rt_table) {
		ip_mask_and(ip, rec.mask, &searched);
		if (ip6_addr_cmp_zoneless(&searched, &rec.addr)) {
			return netif_find(rec.gw_name);
    	}
  	}

	return nullptr;
}

int ip_rm_route(const ip6_addr_t* ip, uint8_t mask) {
  ip6_addr_t searched;
	ip_mask_and(ip, mask, &searched);
	for (auto it = ip_rt_table.begin(); it != ip_rt_table.end(); it++) {
		if (it->mask == mask && ip6_addr_cmp(&it->addr, &searched)) {
			ip_rt_table.erase(it);
			return 1;
		}
	}
	return 0;
}

int ip_rm_route_if(const char* netif_name) {
	unsigned size = ip_rt_table.size();

	ip_rt_table.erase( std::remove_if( ip_rt_table.begin(), ip_rt_table.end(), [&netif_name](rt_entry& r) {
			return strcmp(netif_name, r.gw_name) == 0;
		} ), ip_rt_table.end() );

	return size != ip_rt_table.size() ? 1 : 0;
}

void ip_update_route(const ip6_addr_t* ip, uint8_t mask, const char* new_gw) {
  ip6_addr_t searched;
	ip_mask_and(ip, mask, &searched);
	int i = 0;
	while (ip_rt_table[i].mask <= mask) {
		if (mask == ip_rt_table[i].mask && ip6_addr_cmp_zoneless(&ip_rt_table[i].addr, &searched)) {
			memcpy(ip_rt_table[i].gw_name, new_gw, GW_NAME_SIZE);
			break;
		}
		i++;
	}
}
