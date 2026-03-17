#include<iostream>
#include<fstream>
#include<sstream>
#include<bitset>
#include<cstdio>
#include<cstdlib>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<string.h>
#include<array>
#include<cstring>

#include"control_impl.hh"
#include"Map.hh"
#include"peek_poke.hh"


void pokewrap(const char* ip, const uint32_t& addr, const uint32_t& data)
{
  char buf[64];
  int len = std::snprintf(buf,sizeof(buf),"poke 0x%x 0x%x",addr,(uint32_t)data);
  //  std::cout<<buf<<std::endl;
  std::string payload(buf,len);
  try{
    auto recv = sendcom(ip, BABIMOPORT, MON_PEXEC, payload);
  }catch(const std::exception& e){
    std::cerr<<"poke error: "<<e.what() <<std::endl;
    throw;
  }

}

uint32_t peekwrap(const char* ip, const uint32_t& addr)
{
  char buf[64];
  uint32_t value;
  int len = std::snprintf(buf,sizeof(buf),"peek 0x%x",addr);
  std::string payload(buf,len);
  try{
    auto recv = sendcom(ip, BABIMOPORT, MON_PEXEC, payload);
    std::string s(reinterpret_cast<const char*>(recv.data()),recv.size());
    value = std::stoul(s,nullptr,16);
    
  }catch(const std::exception& e){
    std::cerr<<"peek error: "<<e.what() <<std::endl;
    throw;
  }

  return value;

}

void fpwrite(const char* ip, const uint32_t& mod, const uint32_t& ch, const std::string& pname, const uint32_t& val)
{

  uint32_t addr;
  uint32_t data;
  //ch number ignored for now
  //mode specifiers for 10 channels are packed into one register
  if(pname == "mode"){
    char buf[64];
    snprintf(buf,sizeof(buf),"setting readout mode for mod[%d] ch[%d] to val[%d]",mod,ch,val);
    //    std::cout<<buf<<std::endl;
    addr=addr_mode;
    uint32_t ret = peekwrap(ip,addr);
    snprintf(buf,sizeof(buf),"previous value [0d%d] [0x%x]",ret,ret);
    //    std::cout<<buf<<std::endl;
    uint32_t mask = 0xffffffff - (0x7 << 3*ch); //specific  3bit are removed
    data = (ret & mask) | (val << 3*ch);
    try {
      pokewrap(ip,addr,data);
    }catch(const std::exception& e){
      std::cerr<<"fpwrite error: "<<e.what() <<std::endl;
      throw;
    }

    return;
    
  }else if(pname == "dellen"){
    //address chage in vme mira
    addr = addr_dellen + 8*ch;
    data = val;
  }else if(pname == "monitor"){
    //need to check if this works with VME version
    //ch number ignored, this is a module wide parameter
    addr = addr_monitor;
    data = val;
  }else if(pname == "trigsrc"){
    //ch number ignored, this is a module-wide parameter
    addr = addr_trigsrc;
    data = val;
  }else if(pname == "rdena"){
    //ch number ignored, this is a module-wide parameter
    addr = addr_rdena;
    data = val;
  }else {
    return;
  }

  try{
    uint32_t ret = peekwrap(ip,addr);
    char buf[64];
    snprintf(buf,sizeof(buf),"previous value [%s] [0d%d] [0x%x]",pname.c_str(),ret,ret);
    //    std::cout<<buf<<std::endl;
  }catch(const std::exception& e){
    std::cerr<<"fpwrite error: "<<e.what() <<std::endl;
    throw;
  }
  try{
    pokewrap(ip,addr,data);
    char buf[64];
    snprintf(buf,sizeof(buf),"value is changed [%s] [0d%d] [0x%x]",pname.c_str(),data,data);
    //    std::cout<<buf<<std::endl;
  }catch(const std::exception& e){
    std::cerr<<"fpwrite error: "<<e.what() <<std::endl;
    throw;
  }

}

void csrwrite(const char* ip, const uint32_t& mod, const uint32_t& ch, const std::string& pname, const uint32_t& val){

  auto it = pname2csraddrTable.find(pname);
  
  if(it == pname2csraddrTable.end()) {
    char buf[64];
    snprintf(buf,sizeof(buf),"No item : %s",pname.c_str());
    std::cerr<<buf<<std::endl;
    return;
  }

  uint32_t pnameaddr = static_cast<uint32_t>(it->second);
  
  uint32_t addr = addr_csrwrite + 8*ch; // address change in vme mira
  uint32_t data = (1 << 24) + (pnameaddr << 16) + (0xffff & val);

  try{
    pokewrap(ip,addr,data);
  }catch(const std::exception& e){
    std::cerr<<"csrwrite error: "<<e.what() <<std::endl;
    throw;
 }
  data = (0 << 24) + (pnameaddr << 16) + (0xffff & val);
  try{
    pokewrap(ip,addr,data);
  }catch(const std::exception& e){
    std::cerr<<"csrwrite error: "<<e.what() <<std::endl;
    throw;
 }
  
}

void miraspiwrap(const char* ip, const std::string& com, const std::string& num, const uint32_t& val)
{
  char buf[64];
  int len = std::snprintf(buf,sizeof(buf),"/mnt/sd-mmcblk1p2/bin/miraspi.elf %s %s %d", com, num, val);
  std::string payload(buf,len);

  try{
    auto recv = sendcom(ip, BABIMOPORT, MON_PEXEC, payload);
  }catch(const std::exception& e){
    std::cerr<<"miraspiwrap error: "<<e.what() <<std::endl;
    throw;
  }

}

void pgaconfig(const char* ip, const uint32_t& comgainstr)
{
  // PGA configuration
  // AW1 0-4 -> ch 0-4
  // AW2 0-4 -> ch 4-9
  // gain setting must be given in units of dB
  // valid range -9 - +26 dB corresponds to 16 - 0.3 Vpp
  // need to check how we configure PGA of ch 10 and 11
  //comgainstr = '10' # +10 dB ~ 1.6 Vpp

  try{
    miraspiwrap(ip, "AW1", "0", comgainstr);
    miraspiwrap(ip, "AW1", "1", comgainstr);
    miraspiwrap(ip, "AW1", "2", comgainstr);
    miraspiwrap(ip, "AW1", "3", comgainstr);
    miraspiwrap(ip, "AW1", "4", comgainstr);
    miraspiwrap(ip, "AW1", "5", comgainstr);
    miraspiwrap(ip, "AW2", "0", comgainstr);
    miraspiwrap(ip, "AW2", "1", comgainstr);
    miraspiwrap(ip, "AW2", "2", comgainstr);
    miraspiwrap(ip, "AW2", "3", comgainstr);
    miraspiwrap(ip, "AW2", "4", comgainstr);
    miraspiwrap(ip, "AW2", "5", comgainstr);
  }catch(const std::exception& e){
    std::cerr<<"pgaconfig error: "<<e.what() <<std::endl;
    throw;
  }
  char buf[64];
  snprintf(buf,sizeof(buf),"configuring PGAs gain = %d",comgainstr);
  //std::cout<<buf<<std::endl;

}

Resp428 unpack_resp(const std::vector<uint8_t> &buf, size_t len)
{
  if(len < sizeof(Resp428)){
    std::cout<<"size ="<<len<<std::endl;
    throw std::runtime_error("resp too short");
  }
  Resp428 out{};
  std::memcpy(&out,buf.data(),sizeof(Resp428));
  return out;

}
std::string pack_resp(const Resp428 resp)
{
  char buf[428];
  std::memcpy(buf, &resp, sizeof(Resp428));
  return std::string(buf,428); 
}
//   void dump_resp(const Resp428& resp)
// {
//   std::cout<<"efid : "<<resp.efid <<"\n";
//   std::cout<<"runname : "<<resp.runname.data() <<"\n";
//   std::cout<<"runnumber : "<<resp.runnumber <<"\n";
//   std::cout<<"erport : "<<resp.erport <<"\n";
//   std::cout<<"comport : "<<resp.comport <<"\n";
//   std::cout<<"erhost : "<<resp.erhost.data() <<"\n";
//   std::cout<<"hd1 : "<<resp.hd1 <<"\n";
//   std::cout<<"hd1dir : "<<resp.hd1dir.data() <<"\n";
//   std::cout<<"hd2 : "<<resp.hd2 <<"\n";
//   std::cout<<"hd2dir : "<resp.hd2dir.data() <<"\n";
//   std::cout<<"mt : "<<resp.mt <<"\n";
//   std::cout<<"mtdir : "<<resp.mtdir.data() <<"\n";
//   std::cout<<"connect : "<<resp.connect <<std::endl;
// }
