
#include "userdevice.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "FPGAModule.hh"
#include "RegisterMap.hh"
#include "RegisterMapCommon.hh"
#include "UDPRBCP.hh"
#include "errno.h"
#include "network.hh"
#include "rbcp.hh"

//#define DEBUG 1

namespace
{
  using namespace HUL;
  using namespace Reset;
  //maximum datasize by byte unit
  static const int n_header = 3;
  static const int max_n_word = n_header + 32*4;
  static const int max_data_size = 4*max_n_word;
  DaqMode g_daq_mode = DM_NORMAL;
  std::string nick_name;

  char ip[100];
  int  sock=0;
  bool flag_master = false;
  int reg_en_block=0xf;

  //______________________________________________________________________________
  // local function
  //______________________________________________________________________________
  int
  ConnectSocket( char *ip )
  {
    struct sockaddr_in SiTCP_ADDR;
    unsigned int port = 24;

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    SiTCP_ADDR.sin_family      = AF_INET;
    SiTCP_ADDR.sin_port        = htons((unsigned short int)port);
    SiTCP_ADDR.sin_addr.s_addr = inet_addr(ip);

    struct timeval tv;
    tv.tv_sec  = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    // Connection
    if(0 > connect(sock, (struct sockaddr*)&SiTCP_ADDR, sizeof(SiTCP_ADDR))){
      close(sock);
      return -1;
    }

    return sock;
  }

  //______________________________________________________________________________
  int
  receive( int sock, char* data_buf, unsigned int ReadLength )
  {
    unsigned int revd_size = 0;
    int tmp_returnVal      = 0;

    while( revd_size < ReadLength ){
      tmp_returnVal = recv(sock, data_buf +revd_size, ReadLength -revd_size, 0);
      if(tmp_returnVal == 0){break;}
      if(tmp_returnVal < 0){
	int errbuf = errno;
	std::cerr << "TCP receive" << std::endl;
	if(errbuf == EAGAIN){
	  // time out
	}else{
	  // something wrong
	  std::ostringstream oss;
	  oss << "::" << __func__ << "() " << ip
	      << " TCP receive error " << errbuf;
	  send_error_message( oss.str() );
	}

	revd_size = tmp_returnVal;
	break;
      }
      revd_size += tmp_returnVal;
    }

    return revd_size;
  }

  //______________________________________________________________________________
  int
  EventCycle( int socket, unsigned int* event_buffer )
  {
    static const std::string& func_name(nick_name+" [::"+__func__+"()]");

    // data read ---------------------------------------------------------
    static const unsigned int sizeHeader = n_header*sizeof(unsigned int);
    int ret = receive(sock, (char*)event_buffer, sizeHeader);
    if(ret < 0) return -1;

    unsigned int n_word_data  = event_buffer[1] & 0xfff;
    unsigned int sizeData     = n_word_data*sizeof(unsigned int);

    if(event_buffer[0] != 0xffff0200){
      std::ostringstream oss;
      oss << func_name << " Data broken : " << ip;
      send_fatal_message( oss.str() );
      std::cerr << oss.str() << std::endl;
      std::exit(EXIT_FAILURE);
    }

#if DEBUG
    std::cout << ip << std::hex <<std::endl;
    std::cout << "H1 " << event_buffer[0] << std::endl;
    std::cout << "H2 " << event_buffer[1] << std::endl;
    std::cout << "H3 " << event_buffer[2] << std::endl;
    std::cout << "\n" << std::dec << std::endl;
#endif

    if(n_word_data == 0) return sizeHeader;

    ret = receive(sock, (char*)(event_buffer + n_header), sizeData);
#if DEBUG
    for(unsigned int i = 0; i<n_word_data; ++i){
      printf("D%d : %x\n", i, event_buffer[n_header+i]);
    }
#endif

    if(ret < 0) return -1;
    return sizeHeader + sizeData;

  }

}

//______________________________________________________________________________
int
get_maxdatasize( void )
{
  return max_data_size;
}

//______________________________________________________________________________
void
open_device( NodeProp& nodeprop )
{
  nick_name = nodeprop.getNickName();
  const std::string& func_name(nick_name+" [::"+__func__+"()]");

  // update DAQ mode
  g_daq_mode = nodeprop.getDaqMode();

  // load parameters
  const int argc = nodeprop.getArgc();
  for( int i=0; i<argc; ++i ){
    std::string arg = nodeprop.getArgv(i);
    std::istringstream iss;
    if( arg.substr(0,11) == "--sitcp-ip=" ){
      iss.str( arg.substr(11) );
      iss >> ip;
    }

    // HRM flag
    if( arg.substr(0, 8) == "--master"){
      flag_master = true;
    }

    // register enable block
    if( arg.substr(0,11) == "--en-block=" ){
      iss.str( arg.substr(11) );
      iss >> std::hex >> reg_en_block;
    }
  }

  //Connection check -----------------------------------------------
  while(0 > (sock = ConnectSocket(ip) )){
    std::ostringstream oss;
    oss << func_name << " Connection fail : " << ip;
    send_error_message( oss.str() );
    std::cerr << oss.str() << std::endl;
  }
  
  RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::UDPRBCP::kInteractive);
  FPGAModule fModule(udp_rbcp);
  fModule.WriteModule(BCT::kAddrReset,  1, 1);
  ::sleep(1);

  close(sock);

  return;
}

//______________________________________________________________________________
void
init_device( NodeProp& nodeprop )
{
  const std::string& nick_name(nodeprop.getNickName());
  const std::string& func_name(nick_name+" [::"+__func__+"()]");

  // update DAQ mode
  g_daq_mode = nodeprop.getDaqMode();

  //  event_num = 0;

  switch(g_daq_mode){
  case DM_NORMAL:
    {
      while(0 > (sock = ConnectSocket(ip) )){
	std::ostringstream oss;
	oss << func_name << " Connection fail : " << ip;
	send_error_message( oss.str() );
	std::cerr << oss.str() << std::endl;
      }

      {
	std::ostringstream oss;
	oss << func_name << " Connection done : " << ip;
	send_normal_message( oss.str() );
      }

      // Start DAQ
      RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::UDPRBCP::kInteractive);
      FPGAModule fModule(udp_rbcp);
      {
	std::ostringstream oss;
	oss << func_name << " Firmware : " << std::hex << std::showbase
	    << fModule.ReadModule(BCT::kAddrVersion, 4);
	send_normal_message( oss.str() );
      }

      /*
      if(flag_master){
	fModule.WriteModule(TRM::kAddrSelectTrigger,
			    TRM::kRegL1RM | TRM::kRegL2RM | TRM::kRegClrRM |
			    TRM::kRegEnL2 | TRM::kRegEnRM, 2 );
      */
      if(flag_master){
	fModule.WriteModule(TRM::kAddrSelectTrigger,
			    TRM::kRegL1Ext | TRM::kRegL2RM | TRM::kRegClrRM |
			    TRM::kRegEnL2 | TRM::kRegEnRM, 2 );
      }else{
	fModule.WriteModule(TRM::kAddrSelectTrigger,
			    TRM::kRegL1Ext | TRM::kRegL2J0 | TRM::kRegClrJ0 |
			    TRM::kRegEnL2 | TRM::kRegEnJ0, 2);
      }

      fModule.WriteModule(DCT::kAddrResetEvb, 0x1, 1);

      fModule.WriteModule(RTR::kAddrCounterReset, 0x0, 1);
      fModule.WriteModule(RTR::kAddrEnableBlock, reg_en_block, 1);
      fModule.WriteModule(RTR::kAddrEnableHdrst, 0xf, 1);



      fModule.WriteModule(IOM::kAddrExtL1     , IOM::kReg_i_Nimin1, 1);
      fModule.WriteModule(IOM::kAddrNimout1     , IOM::kReg_o_ModuleBusy, 1);


      // start DAQ
      fModule.WriteModule(DCT::kAddrDaqGate, 1, 1);
      return;
    }
  case DM_DUMMY:
    {
      return;
    }
  default:
    return;
  }

}

//______________________________________________________________________________
void
finalize_device( NodeProp& nodeprop )
{
  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");

  RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::UDPRBCP::kInteractive);
  FPGAModule fModule(udp_rbcp);
  fModule.WriteModule(DCT::kAddrDaqGate, 0, 1);
  ::sleep(1);
  unsigned int data[max_n_word];
  while(-1 != EventCycle(sock, data));

  shutdown(sock, SHUT_RDWR);
  close(sock);

  return;
}

//______________________________________________________________________________
void
close_device( NodeProp& nodeprop )
{
  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");
  return;
}

//______________________________________________________________________________
int
wait_device( NodeProp& nodeprop )
/*
  return -1: TIMEOUT or FAST CLEAR -> continue
  return  0: TRIGGED -> go read_device
*/
{
  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");
  switch(g_daq_mode){
  case DM_NORMAL:
    {
      return 0;
    }
  case DM_DUMMY:
    {
      usleep(200000);
      return 0;
    }
  default:
    return 0;
  }

}

//______________________________________________________________________________
int
read_device( NodeProp& nodeprop, unsigned int* data, int& len )
/*
  return -1: Do Not Send data to EV
  return  0: Send data to EV
*/
{
  // const std::string& nick_name(nodeprop.getNickName());
  // const std::string& func_name(nick_name+" [::"+__func__+"()]");
  switch(g_daq_mode){
  case DM_NORMAL:
    {
      int ret_event_cycle = EventCycle(sock, data);
      len = ret_event_cycle == -1 ? -1 : ret_event_cycle/sizeof(unsigned int);
      // if( len > 0 ){
      // 	for(int i = 0; i<n_word; ++i){
      // 	  printf("%x ", data[i]);
      // 	  if(i%8==0) printf("\n");
      // 	}
      // }
      return len;
    }
  case DM_DUMMY:
    {
      len = 0;
      return 0;
    }
  default:
    len = 0;
    return 0;
  }

}
