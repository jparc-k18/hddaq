
#include "userdevice.h"

#include "kol/koltcp.h"

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
#include<thread>
#include<array>
#include<cstring>

#include "control_impl.hh"
#include "peek_poke.hh"
#include "Map.hh"
#include "my_endian.h"

static const unsigned int tcp_port = ESCOMPORT;
//static const unsigned int tcp_port = ERRCVPORT;

DaqMode g_daq_mode  = DM_NORMAL;
std::string nick_name;
char ip[100];

namespace
{

  //maximum datasize by byte unit
  static const int n_header      = 3;
  static const int max_n_word    = n_header+2*16*64 + 64*2;
  static const int max_data_size = sizeof(unsigned int)*max_n_word;

  std::string module_num;
  int  sock_com=0;
  int  sock_data=0;
  char selfip[80] = "192.168.1.3";
  //_________________________________________________________________________
  // local function
  //_________________________________________________________________________
  int
  ConnectSocket( char *ip , const uint16_t& port_sel)
  {
    struct sockaddr_in SiTCP_ADDR;
    unsigned int port = port_sel;

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
  int
  BindSocket(const uint16_t& port_sel)
  {
    struct sockaddr_in SiTCP_ADDR;
    unsigned int port = port_sel;

    // std::cout<<"ip = "<<std::string(ip)<<std::endl;
    // std::cout<<"port = "<<port<<std::endl;
	
    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    SiTCP_ADDR.sin_family      = AF_INET;
    SiTCP_ADDR.sin_port        = htons((unsigned short int)port);
    SiTCP_ADDR.sin_addr.s_addr = INADDR_ANY;

    // making ip:port socket
    bind(sock, (struct sockaddr*)&SiTCP_ADDR, sizeof(SiTCP_ADDR));
    
    return sock;
  }

  //_________________________________________________________________________
  void accept_loop(int sock){
    struct sockaddr_in CLIENT_ADDR;
    socklen_t len = sizeof(CLIENT_ADDR);
    sock_data = accept(sock,(struct sockaddr*)&CLIENT_ADDR,&len);
    std::cout<<"sock_data : "<<sock_data<<std::endl;
    return;
  }
  
  //_________________________________________________________________________
  void
  readAndThrowPreviousData(double timeout = 0.1)
  {
    int thrownBytes = 0;
    while(1) {
      fd_set rfds;
      FD_ZERO(&rfds);
      std::cout<<"sock_data = "<<sock_data<<std::endl;
      FD_SET(sock_data, &rfds);

      struct timeval tv;
      tv.tv_sec = static_cast<unsigned int>(timeout);
      tv.tv_usec = static_cast<unsigned int>(timeout * 1000 * 1000);

      int select_ret = select(sock_data + 1, &rfds, NULL, NULL, &tv);
      if(select_ret < 0) {
	perror("readAndThrowPreviousData");
	throw std::runtime_error("select failed");
	return;
      }else if(select_ret == 0) {
	break;
      }else{
	char buf[256];
	thrownBytes += recv(sock_data, buf, sizeof(buf), 0);
      }
    }
    std::cout << thrownBytes << " bytes are thrown" << std::endl;
  }

  int
  receiveNByte(uint8_t* buf, size_t bytes)
  {
    ssize_t receivedBytes = 0;
    while(receivedBytes < bytes) {
      ssize_t receivedDataLength = recv(sock_data, buf + receivedBytes,
					bytes - receivedBytes, 0);
      if(receivedDataLength < 0) {
	//	std::cout<<"recievedDataLength<0 "<<std::endl;
	perror("recv");
	throw std::runtime_error("recv failed");
      }
      receivedBytes += receivedDataLength;
      //      std::cout << "receiveNbyte_: " << receivedDataLength << " bytes are read" << std::endl;
    }
    return receivedBytes;
  }

  //_________________________________________________________________________
  uint32_t
  receiveHeader()
  {
    //    std::cout<<"recieve header start "<<std::endl;
    uint8_t headerBuf[4];
    if(receiveNByte(headerBuf, 4) < 0) {     
      std::cout << "Receive header failed" << std::endl;
      throw std::runtime_error("Receive header failed");
    }
    //    std::cout<<"recieve header start clear "<<std::endl;
    uint32_t header = Endian::getBigEndian32(headerBuf);
    uint32_t cid = (header >> 22) & 0x3f;    
    bool isHeader = false;
    if(cid == 6) isHeader = true;
    if(!isHeader) {
      std::cout << "Frame Error or event start" << std::endl;
      printf("    %08X\n", header);
      throw std::runtime_error("Frame Error or event start");
    }
    return header;
  }

  uint32_t
  receiveSecondHeader()
  {
    uint8_t headerBuf[4];
    if(receiveNByte(headerBuf, 4) < 0) {
      std::cout << "Receive second header failed" << std::endl;
      throw std::runtime_error("Receive second header failed");
    }
    uint32_t header = Endian::getBigEndian32(headerBuf);
    uint32_t cid = (header >> 22) & 0x3f;
    bool isHeader = false;
    if(cid == 4) isHeader = true;
    if(!isHeader) {
      std::cout << "Frame Error " << std::endl;
      printf("    %08X\n", header);
      throw std::runtime_error("Frame Error or event start");
    }
    return header;
  }

  
  void
  receivenullHeader(size_t length)
  {

    for(int i = 0; i<length; i++){
      uint8_t headerBuf[4];
      if(receiveNByte(headerBuf, 16) < 0) {
	std::cout << "Receive null header failed" << std::endl;
	throw std::runtime_error("Receive null header failed");
      }
    }
    return;
  }

  
  //_________________________________________________________________________
  size_t
  receiveData(uint8_t* buf, size_t length)
  {
    return receiveNByte(buf, length * 4);
  }
  
}

//___________________________________________________________________________
int
get_maxdatasize( void )
{
  return max_data_size;
}

//___________________________________________________________________________
void
open_device( NodeProp& nodeprop )
{
  nick_name = nodeprop.getNickName();
  static const std::string& func_name(nick_name+" [::"+__func__+"()]");

  // update DAQ mode
  g_daq_mode = nodeprop.getDaqMode();

  // load parameters
  std::string reg_dir;
  const int argc = nodeprop.getArgc();
  for( int i=0; i<argc; ++i ){
    std::string arg = nodeprop.getArgv(i);
    std::istringstream iss;
    if( arg.substr(0,11) == "--sitcp-ip=" ){
      iss.str( arg.substr(11) );
      iss >> ip;
    }
    if( arg.substr(0,13) == "--module-num=" ){
      iss.str( arg.substr(13) );
      iss >> module_num;
    }
  }

  while(true){
    try{
      std::cout<<"init start"<<std::endl;
      fpwrite(ip,0,0,"rdena",0x33ff);
      for(int i = 0; i<12; i++){
	fpwrite(ip,0,i,"dellen",0xff06);
	csrwrite(ip,0,i,"inv",0);
	csrwrite(ip,0,i,"fastdiff",127);
	csrwrite(ip,0,i,"fastgain",0);
	csrwrite(ip,0,i,"stretch",255);
	csrwrite(ip,0,i,"thresh",100);
	csrwrite(ip,0,i,"trapezflat",63);
	csrwrite(ip,0,i,"trapezrise",255);
	csrwrite(ip,0,i,"trapezgain",0);
	csrwrite(ip,0,i,"tau",1000);
	csrwrite(ip,0,i,"blrhpd",0);

	fpwrite(ip,0,i,"monitor",1);
	pgaconfig(ip,10);
      }
      break;
    }catch(const std::exception& e){
      send_error_message(e.what());
      ::sleep(1);
    }
  }

  //Connection check -----------------------------------------------
  while(0 > (sock_com = ConnectSocket(ip,ESCOMPORT) )){
    std::ostringstream oss;
    oss << func_name << " Connection fail : " << ip;
    send_error_message( oss.str() );
    std::cerr << oss.str() << std::endl;
  }

  close(sock_com);

  return;
}

//______________________________________________________________________________
void
init_device( NodeProp& nodeprop )
{
  static const std::string& func_name(nick_name+" [::"+__func__+"()]");

  // update DAQ mode
  //g_daq_mode = DM_NORMAL;
  g_daq_mode = nodeprop.getDaqMode();

  //  event_num = 0;

 
  switch(g_daq_mode){
  case DM_NORMAL:
    {
      fpwrite(ip,0,0,"rdena",0x33ff);
      for(int i = 0; i<12; i++){
	fpwrite(ip,0,i,"dellen",0xff06);
	csrwrite(ip,0,i,"inv",0x0);
	csrwrite(ip,0,i,"fastdiff",0x7f);
	csrwrite(ip,0,i,"fastgain",0x0);
	csrwrite(ip,0,i,"stretch",0xff);
	csrwrite(ip,0,i,"thresh",0x64);
	csrwrite(ip,0,i,"trapezflat",0x3f);
	csrwrite(ip,0,i,"trapezrise",0xff);
	csrwrite(ip,0,i,"trapezgain",0x0);
	csrwrite(ip,0,i,"tau",0x3e8);
	csrwrite(ip,0,i,"blrhpd",0x0);
      }
      fpwrite(ip,0,0,"monitor",0x1);
      pgaconfig(ip,10);

      int sock_conn = 0;
      ////////////////making server socket and witing connection

      if(0 > (sock_conn = BindSocket(ERRCVPORT))){
	std::cout<<"bind fail"<<std::endl;
	perror("bind");
	close(sock_conn);
	return;
      }

      if(0 > listen(sock_conn, 1)){
	std::cout<<"listen fail"<<std::endl;
	perror("listen");
	close(sock_conn);
	return;
      }
      std::cout<<"sock_conn : "<<sock_conn<<std::endl;      

      std::thread th(accept_loop, sock_conn);

      /////////////////////////////////////
      sleep(0.5);

      try{
	std::vector<uint8_t> buf = sendcom(ip,ESCOMPORT,ES_GET_CONFIG,"");
	std::cout<<"get config"<<std::endl;
	Resp428 resp = unpack_resp(buf,buf.size());

	std::cout<<"erhost before = "<<resp.erhost.data()<<std::endl;
	std::cout<<"erport before= "<<resp.erport<<std::endl;
	std::memcpy(resp.erhost.data(),selfip,sizeof(selfip));
	std::string payload = pack_resp(resp);

	sendcom(ip,ESCOMPORT,ES_SET_CONFIG,payload);

	std::vector<uint8_t> getbuf = sendcom(ip,ESCOMPORT,ES_GET_CONFIG,"");
	std::cout<<"-----------------"<<std::endl;
	std::cout<<"get config second"<<std::endl;
	std::cout<<"-----------------"<<std::endl;
		
	Resp428 respsecond = unpack_resp(getbuf,getbuf.size());
	std::cout<<"finish unpack"<<std::endl;
	std::cout<<"efid  : "<<respsecond.efid<<std::endl;
	std::cout<<"runnumber : "<<respsecond.runnumber<<std::endl;
	std::cout<<"erport : "<<respsecond.erport<<std::endl;
	std::cout<<"comport : "<<respsecond.comport<<std::endl;
	std::cout<<"erhost : "<<respsecond.erhost.data()<<std::endl;
	std::cout<<"mtdir : "<<respsecond.mtdir.data()<<std::endl;
	std::cout<<"connect : "<<respsecond.connect<<std::endl;
		
	std::vector<uint8_t> evtnbuf = sendcom(ip,ESCOMPORT,ES_GET_EVTN,"");
	std::cout<<"evtn  = ";
	for(auto i : evtnbuf){
	  std::cout<<i<<",";
	}
	std::cout<<std::endl;

	std::vector<uint8_t> runstatbuf = sendcom(ip,ESCOMPORT,ES_GET_RUNSTAT,"");
	std::cout<<"runstat  = ";
	for(auto i : runstatbuf){
	  std::cout<<i<<",";
	}
	std::cout<<std::endl;

	
	std::vector<uint8_t> nsstabuf = sendcom(ip,ESCOMPORT,ES_RUN_NSSTA,""); // after this command, client attempt to connect server
	//std::vector<uint8_t> nsstabuf = sendcom(ip,ESCOMPORT,4,""); 
	std::cout<<"nssta  = ";
	for(auto i : nsstabuf){
	  std::cout<<i<<",";
	}
	std::cout<<std::endl;

	std::cout<<"daq com is finished"<<std::endl;
      }catch(std::exception &e){
	send_error_message(e.what());
	std::cout<<"DAQ COMAND error : "<<e.what()<<std::endl;
	return;
      }
      ::sleep(0.5);
      th.join();

      fpwrite(ip,0,0,"trigsrc",0x00000010); // trig selctor 
      
      {
	std::ostringstream oss;
	oss << func_name << "All Connection done : " << ip;
	send_normal_message( oss.str() );
	std::cout<<oss.str()<<std::endl;
      }
      readAndThrowPreviousData();
      
      return;
    }
  case DM_DUMMY:
    {
      fpwrite(ip,0,0,"rdena",0x33ff);
      for(int i = 0; i<12; i++){
	fpwrite(ip,0,i,"dellen",0xff06);
	csrwrite(ip,0,i,"inv",0);
	csrwrite(ip,0,i,"fastdff",127);
	csrwrite(ip,0,i,"fastgain",0);
	csrwrite(ip,0,i,"stretch",255);
	csrwrite(ip,0,i,"thresh",100);
	csrwrite(ip,0,i,"trapezflat",63);
	csrwrite(ip,0,i,"trapezrise",255);
	csrwrite(ip,0,i,"trapezgain",0);
	csrwrite(ip,0,i,"tau",1000);
	csrwrite(ip,0,i,"blrhpd",0);

	fpwrite(ip,0,i,"monitor",1);
	pgaconfig(ip,10);
      }

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

  sendcom(ip,ESCOMPORT,ES_RUN_STOP,"");
  
  int ret_com = -10;
  int ret_data = -10;
  if(sock_com >= 0) {
    ret_com = close(sock_com);
  }
  if(sock_data >= 0) {
    ret_data = close(sock_data);
  }
  std::cout << "finalize_device() [com]" << ret_com << std::endl;
  std::cout << "finalize_device() [data]" << ret_data<< std::endl;

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
      try {
	//	std::cout<<"recv start"<<std::endl;
        uint32_t header = receiveHeader();
        uint8_t spillNumber = 0x0000; // now null
        uint8_t eventNumber = 0x0000; // now null

	std::cout<<"First Header clear"<<std::endl;
	
        Endian::setLittleEndian32(data, 0x4157494d);
        data++;
        Endian::setLittleEndian32(data, spillNumber << 3 | eventNumber);
        data++;

	receivenullHeader(4);
	std::cout<<"null Header clear"<<std::endl;
	
	receiveSecondHeader();
	uint32_t second_header = receiveSecondHeader();
	uint32_t ch = (header >> 8) & 0x3f;
	uint32_t dataSize = header & 0x0fff - 4*2;
        Endian::setLittleEndian32(data, dataSize);
        data++;
	std::cout<<"Second Header clear"<<std::endl;
	
	receivenullHeader(2);
	std::cout<<"null Header clear"<<std::endl;
	
        size_t receivedDataLength = receiveData(reinterpret_cast<uint8_t*>(data),
                                                 static_cast<size_t>(dataSize));
        if(receivedDataLength != dataSize * 4) {
	  std::cerr << "receiveData failed" << std::endl;
	  return -1;
        }
        len = dataSize + 3;
        return 1;
      }catch(std::exception &e) {
        std::cout << e.what() << std::endl;
        return -1;
      }

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
