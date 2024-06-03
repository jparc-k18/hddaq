#include "userdevice.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "FPGAModule.hh"
#include "RegisterMap.hh"
#include "RegisterMapCommon.hh"
#include "UDPRBCP.hh"
#include "BctBusBridgeFunc.hh"
#include "errno.h"
#include "network.hh"
#include "rbcp.hh"

bool    stand_alone = false;
DaqMode g_daq_mode  = DM_NORMAL;
std::string nick_name;
#define DEBUG 0

namespace
{
  using namespace HUL;
  using namespace HRTDC_BASE;
  //maximum datasize by byte unit
  static const int n_header      = 3;
  static const int max_n_word    = n_header + 2 + 16*64 + 16*64;
  static const int max_data_size = sizeof(unsigned int)*max_n_word;

  char ip[100];
  unsigned int min_time_window;
  unsigned int max_time_window;
  int          en_slot = 0x3;
  bool         en_slot_up   = false;
  bool         en_slot_down = false;

  int  sock=0;
  //rbcp_header rbcpHeader;

  //______________________________________________________________________________
  // local function
  //______________________________________________________________________________
  int
  ConnectSocket( char *ip )
  {
    struct sockaddr_in SiTCP_ADDR;
    //unsigned int port = tcp_port;
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

    unsigned int n_word_data  = event_buffer[1] & 0x3ff;
    unsigned int sizeData     = n_word_data*sizeof(unsigned int);

    if(event_buffer[0] != 0xffff80eb){
      std::ostringstream oss;
      oss << func_name << " Data broken : " << ip;
      send_fatal_message( oss.str() );
      std::cerr << oss.str() << std::endl;
    }

#if DEBUG
    std::cout << ip << std::hex <<std::endl;
    std::cout << "H1 " << event_buffer[0] << std::endl;
    std::cout << "H2 " << event_buffer[1] << std::endl;
    std::cout << "H3 " << event_buffer[2] << std::endl;
#endif

    if(n_word_data == 0) return sizeHeader;

    ret = receive(sock, (char*)(event_buffer + n_header), sizeData);
#if DEBUG
    for(unsigned int i = 0; i<n_word_data; ++i){
      printf("D%d : %x\n", i, event_buffer[n_header+i]);
    }
    std::cout << "\n" << std::dec << std::endl;
#endif

    if(ret < 0) return -1;
    return sizeHeader + sizeData;
  }


  // SetTdcWindow ------------------------------------------------------------
  void
  SetTdcWindow(unsigned int wmax, unsigned int wmin, FPGAModule& fModule, uint32_t addr_base)
  {
    static const unsigned int kCounterMax = 2047;
    static const unsigned int kPtrDiffWr  = 2;

    unsigned int ptr_ofs = kCounterMax - wmax + kPtrDiffWr;

    WriteModuleIn2ndryFPGA(fModule, addr_base,
			   HRTDC_MZN::TDC::kAddrPtrOfs, ptr_ofs, 2);
    WriteModuleIn2ndryFPGA(fModule, addr_base,
			 HRTDC_MZN::TDC::kAddrWinMax, wmax, 2);
    WriteModuleIn2ndryFPGA(fModule, addr_base,
			 HRTDC_MZN::TDC::kAddrWinMin, wmin, 2);

  }

  // DdrInitialize --------------------------------------------------------
  void
  DdrInitialize(FPGAModule& fModule)
  {
    unsigned int reg_enable_up   = en_slot_up   ? DCT::kRegEnableU : 0;
    unsigned int reg_enable_down = en_slot_down ? DCT::kRegEnableD : 0;

    std::cout << "#D : Do DDR initialize" << std::endl;
    // MZN
    if(en_slot_up){
      WriteModuleIn2ndryFPGA(fModule, BBP::kUpper,
			   HRTDC_MZN::DCT::kAddrTestMode, 1, 1 );
    }

    if(en_slot_down){
      WriteModuleIn2ndryFPGA(fModule, BBP::kLower,
			   HRTDC_MZN::DCT::kAddrTestMode, 1, 1 );
    }

    unsigned int reg =
      reg_enable_up   |
      reg_enable_down |
      DCT::kRegTestModeU |
      DCT::kRegTestModeD;

    // Base
    fModule.WriteModule(DCT::kAddrCtrlReg, reg, 1);
    fModule.WriteModule(DCT::kAddrInitDDR, 0, 1);

    unsigned int ret = fModule.ReadModule(DCT::kAddrRcvStatus, 1);

    if(en_slot_up){
      if( ret & DCT::kRegBitAlignedU){
	std::cout << "#D : DDR initialize succeeded (MZN-U)" << std::endl;
      }else{
	std::cout << "#E : Failed (MZN-U)" << std::endl;
	exit(-1);
      }
    }// bit aligned ?

    if(en_slot_down){
      if( ret & DCT::kRegBitAlignedD){
	std::cout << "#D : DDR initialize succeeded (MZN-D)" << std::endl;
      }else{
	std::cout << "#E : Failed (MZN-D)" << std::endl;
	exit(-1);
      }
    }// bit aligned ?

    // Set DAQ mode

    if(en_slot_up){
      WriteModuleIn2ndryFPGA(fModule, BBP::kUpper,
		     HRTDC_MZN::DCT::kAddrTestMode, 0, 1 );

    }

    if(en_slot_down){
      WriteModuleIn2ndryFPGA(fModule, BBP::kLower,
		     HRTDC_MZN::DCT::kAddrTestMode, 0, 1 );
    }

    reg = reg_enable_up | reg_enable_down;
    fModule.WriteModule(DCT::kAddrCtrlReg, reg, 1);

  }// ddr_initialize

  // CalibLUT ---------------------------------------------------------------
  void
  CalibLUT(FPGAModule& fModule, uint32_t addr_base)
  {
    WriteModuleIn2ndryFPGA(fModule, addr_base,
		    HRTDC_MZN::TDC::kAddrControll, 0, 1);

    WriteModuleIn2ndryFPGA(fModule, addr_base,
		   HRTDC_MZN::DCT::kAddrExtraPath, 1, 1);

    while(!(ReadModuleIn2ndryFPGA(fModule, addr_base, HRTDC_MZN::TDC::kAddrStatus, 1) & HRTDC_MZN::TDC::kRegReadyLut)){
      sleep(1);
      std::cout << "#D waiting LUT ready" << std::endl;
    }// while

    if((int32_t)addr_base == BBP::kUpper){
      std::cout << "#D LUT is ready! (BBP-U)" << std::endl;
    }else{
      std::cout << "#D LUT is ready! (BBP-D)" << std::endl;
    }

    WriteModuleIn2ndryFPGA(fModule, addr_base,
		   HRTDC_MZN::DCT::kAddrExtraPath, 0, 1);
    WriteModuleIn2ndryFPGA(fModule, addr_base,
		    HRTDC_MZN::TDC::kAddrReqSwitch, 1, 1);
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
  static const std::string& func_name(nick_name+" [::"+__func__+"()]");

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

    if( arg.substr(0,11) == "--min-twin=" ){
      iss.str( arg.substr(11) );
      iss >> min_time_window;
    }

    if( arg.substr(0,11) == "--max-twin=" ){
      iss.str( arg.substr(11) );
      iss >> max_time_window;
    }

    if( arg.substr(0,10) == "--en-slot=" ){
      iss.str( arg.substr(10) );
      iss >> std::hex >> en_slot;
      if( en_slot      & 0x1 ) en_slot_up   = true;
      if( (en_slot>>1) & 0x1 ) en_slot_down = true;
    }
  }

  //Connection check -----------------------------------------------
  while(0 > (sock = ConnectSocket(ip) )){
    std::ostringstream oss;
    oss << func_name << " Connection fail : " << ip;
    send_error_message( oss.str() );
    std::cerr << oss.str() << std::endl;
  }

  close(sock);

  RBCP::UDPRBCP udp_rbcp(ip, RBCP::gUdpPort, RBCP::UDPRBCP::kNoDisp); //kInteractive->kNoDisp FOura
  FPGAModule fModule(udp_rbcp);
  fModule.WriteModule(BCT::kAddrReset, 0, 1);
  ::sleep(1);

  if(en_slot_up){
    uint32_t reg = fModule.WriteModule(DCT::kAddrCtrlReg, 1);
    fModule.WriteModule(DCT::kAddrCtrlReg, reg | DCT::kRegFRstU);
    fModule.WriteModule(DCT::kAddrCtrlReg, reg);
  }


  if(en_slot_up){
    uint32_t reg = fModule.WriteModule(DCT::kAddrCtrlReg, 1);
    fModule.WriteModule(DCT::kAddrCtrlReg, reg | DCT::kRegFRstD);
    fModule.WriteModule(DCT::kAddrCtrlReg, reg);
  }

  ::sleep(1);


  DdrInitialize(fModule);
  if(en_slot_up)   CalibLUT(fModule, BBP::kUpper);
  if(en_slot_down) CalibLUT(fModule, BBP::kLower);

  unsigned int tdc_ctrl = HRTDC_MZN::TDC::kRegAutosw;
  if(en_slot_up){
    WriteModuleIn2ndryFPGA(fModule, BBP::kUpper,
		    HRTDC_MZN::TDC::kAddrControll, tdc_ctrl, 1);

  }

  if(en_slot_down ){
    WriteModuleIn2ndryFPGA(fModule, BBP::kLower,
		    HRTDC_MZN::TDC::kAddrControll, tdc_ctrl, 1);
  }

  return;
}

//______________________________________________________________________________
void
init_device( NodeProp& nodeprop )
{
  static const std::string& func_name(nick_name+" [::"+__func__+"()]");

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
	oss << func_name << " Firmware (BASE): " << std::hex << std::showbase
	    << fModule.ReadModule(BCT::kAddrVersion, 4);
	send_normal_message( oss.str() );

	if(en_slot_up){
	  std::ostringstream oss_mznu;
	  oss_mznu << func_name << " Firmware (MZNU): " << std::hex << std::showbase
		   << ReadModuleIn2ndryFPGA(fModule, BBP::kUpper,
				    HRTDC_MZN::BCT::kAddrVersion, 4 );
	  send_normal_message( oss_mznu.str() );
	}
	if(en_slot_down){
	  std::ostringstream oss_mznd;
	  oss_mznd << func_name << " Firmware (MZND): " << std::hex << std::showbase
		   << ReadModuleIn2ndryFPGA(fModule, BBP::kLower,
				    HRTDC_MZN::BCT::kAddrVersion, 4 );
	  send_normal_message( oss_mznd.str() );
	}
      }

      fModule.WriteModule(TRM::kAddrSelectTrigger,
			  TRM::kRegL1Ext | TRM::kRegL2J0 | TRM::kRegClrJ0
			  | TRM::kRegEnL2 | TRM::kRegEnJ0,
			  2);
      // fModule.WriteModule(TRM::kAddrSelectTrigger,
      // 			  TRM::kRegL1Ext | TRM::kRegL2Ext | TRM::kRegClrExt
      // 			  | TRM::kRegEnL2 | TRM::kRegEnJ0,
      // 			  2);

      fModule.WriteModule(DCT::kAddrResetEvb, 0x1, 1);

      uint32_t en_blocks = HRTDC_MZN::DCT::kEnLeading;
      if(en_slot_up){
	WriteModuleIn2ndryFPGA(fModule, BBP::kUpper,
		       HRTDC_MZN::DCT::kAddrEnBlocks, en_blocks, 1);
      }

      if(en_slot_down ){
	WriteModuleIn2ndryFPGA(fModule, BBP::kLower,
		       HRTDC_MZN::DCT::kAddrEnBlocks, en_blocks, 1);
      }

      if(en_slot_up)   SetTdcWindow(max_time_window, min_time_window, fModule, BBP::kUpper);
      if(en_slot_down) SetTdcWindow(max_time_window, min_time_window, fModule, BBP::kLower);

      fModule.WriteModule(IOM::kAddrExtL1,  IOM::kReg_i_Nimin1, 1);
      fModule.WriteModule(IOM::kAddrExtL2,  IOM::kReg_i_Nimin2, 1);
      fModule.WriteModule(IOM::kAddrExtClr, IOM::kReg_i_Nimin3, 1);
      //fModule.WriteModule(IOM::kAddrExtBusy, IOM::kReg_i_Nimin2, 1);
      //fModule.WriteModule(IOM::kAddrNimout1,  IOM::kReg_o_ModuleBusy, 1);
      //fModule.WriteModule(IOM::kAddrNimout1,  IOM::kAddrExtL1, 1);
      fModule.WriteModule(IOM::kAddrNimout2,  IOM::kReg_o_clk10kHz, 1);

      // start DAQ
      if(en_slot_up){
	WriteModuleIn2ndryFPGA(fModule, BBP::kUpper,
		       HRTDC_MZN::DCT::kAddrGate, 1, 1);
      }

      if(en_slot_down ){
	WriteModuleIn2ndryFPGA(fModule, BBP::kLower,
		       HRTDC_MZN::DCT::kAddrGate, 1, 1);
      }

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
  if(en_slot_up){
    WriteModuleIn2ndryFPGA(fModule, BBP::kUpper,
		   HRTDC_MZN::DCT::kAddrGate, 0, 1);
  }

  if(en_slot_down){
    WriteModuleIn2ndryFPGA(fModule, BBP::kLower,
		   HRTDC_MZN::DCT::kAddrGate, 0, 1);
  }

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
