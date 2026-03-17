#ifndef PEEK_POKE_H
#define PEEK_POKE_H

#include<unistd.h>
#include<vector>
#include<map>
#include<string>
#include<algorithm>
#include<cstdint>
#include<Map.hh>

static inline uint32_t loadLittleEndian32(const std::vector<uint8_t> b);
bool send_all(int sock, const void* data, size_t len, std::string& err);
bool recv_all(int sock, void* data, size_t len, std::string& err);
std::vector<uint8_t> sendcom(const char* ip, const uint16_t& port,const uint32_t &com, const std::string& payload);

#endif
