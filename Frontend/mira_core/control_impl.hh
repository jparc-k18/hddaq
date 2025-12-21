#ifndef CTRL_IMPL_H
#define CTRL_IMPL_H
#include<array>

#include <peek_poke.hh>
#include <Map.hh>

struct Resp428{
  int32_t efid;
  std::array<char,80> runname;
  int32_t runnumber;
  int16_t erport;
  int16_t comport;
  std::array<char,80> erhost;
  int32_t hd1;
  std::array<char,80> hd1dir;
  int32_t hd2;
  std::array<char,80> hd2dir;
  int32_t mt;
  std::array<char,80> mtdir;
  int32_t connect;
};
static_assert(sizeof(Resp428)==428,"Resp428 must be 428 bytes");

void pokewrap(const char* ip, const uint32_t& addr, const uint32_t& data);
uint32_t peekwrap(const char* ip, const uint32_t& addr);
void fpwrite(const char* ip, const uint32_t& mod, const uint32_t& ch, const std::string& pname, const uint32_t& val);
void csrwrite(const char* ip, const uint32_t& mod, const uint32_t& ch, const std::string& pname, const uint32_t& val);
void miraspiwrap(const char* ip, const std::string& com, const std::string& num, const uint32_t& val);
void pgaconfig(const char* ip, const uint32_t& comgainstr);
Resp428 unpack_resp(const std::vector<uint8_t>& buf, size_t len);
std::string pack_resp(const Resp428 resp);  
//void dump_resp(const Resp428& resp);
#endif
