#ifndef MAP_H_
#define MAP_H_

#include<unordered_map>

//PORT MAP
const uint16_t BABIMOPORT = 17518; //shell com port
const uint16_t ERRCVPORT = 17601; // data port
const uint16_t ESCOMPORT = 17658; // data aq com 17651 + efn

// FPGA local register address
const uint32_t addr_mode = 0x43c0020c;
const uint32_t addr_dellen = 0x43c00220;
const uint32_t addr_monitor = 0x43c0008c;
const uint32_t addr_trigsrc = 0x43c00208;
const uint32_t addr_rdena =  0x43c00214;
const uint32_t addr_csrwrite = 0x43c00224;

//csrwrite pname to csraddr
enum pname2csraddr{
  i_inv,      i_thresh,     i_stretch,    i_fastgain,
  i_fastdiff, i_trapezgain, i_trapezrise, i_trapezflat,
  i_tau,      i_blrhpd  
};

const uint32_t ES_SET_CONFIG   = 1;
const uint32_t ES_GET_CONFIG   = 2;
const uint32_t ES_RUN_START    = 3;
const uint32_t ES_RUN_NSSTA    = 4;
const uint32_t ES_RUN_STOP     = 5;
const uint32_t ES_RELOAD_DRV   = 6;
const uint32_t ES_GET_EVTN     = 8;
const uint32_t ES_GET_RUNSTAT  = 9;
const uint32_t ES_CON_EFR      = 11;
const uint32_t ES_DIS_EFR      = 12;
const uint32_t ES_STORE_CONFIG = 57;
const uint32_t ES_QUIT         = 99;


static const std::unordered_map<std::string, uint32_t> pname2csraddrTable = {
  {"inv", 0},
  {"thresh", 1},
  {"stretch", 2},
  {"fastgain", 3},
  {"fastdiff", 4},
  {"trapezgain", 5},
  {"trapezrise", 6},
  {"trapezflat", 7},
  {"tau", 8},
  {"blrhpd", 9}
};
//EXEC command
const uint32_t MON_PEXEC = 15;
  
#endif
