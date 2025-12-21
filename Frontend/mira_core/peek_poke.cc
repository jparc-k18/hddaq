#include <iostream>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <cerrno>
#include <Map.hh>
#include <peek_poke.hh>

static inline uint32_t loadLittleEndian32(const std::vector<uint8_t> b)
{
  return (uint32_t)b[0]
    | ((uint32_t)b[1] << 8)
    | ((uint32_t)b[2] << 16)
    | ((uint32_t)b[3] << 24);
}

bool send_all(int sock, const void* data, size_t len, std::string& err)
{
  const uint8_t* p = static_cast<const uint8_t*>(data);
  ssize_t sent = 0;

  while (sent<len){
    ssize_t n = ::send(sock,p+sent,len-sent,0);
    if(n>0){
      sent+=n;
      continue;
    }
    if (n==0){
      err = "send returned 0 (no process)";
      return false;
    }
    //n<0
    if(errno == EINTR){
      continue;
    }
    if(errno == EAGAIN || errno == EWOULDBLOCK){
      err="send timeout (EAGAIN/EWOULDBLOCK)";
    }
    err = std::string("send failed: ") + std::strerror(errno);
    return false;
  }

  return true;
  
}
bool recv_all(int sock, void* data, size_t len, std::string& err)
{
  uint8_t* p = static_cast<uint8_t*>(data);
  ssize_t recvd = 0;

  while (recvd<len){
    ssize_t n = ::recv(sock,p+recvd,len-recvd,0);
    if(n>0){
      recvd+=n;
      continue;
    }
    if (n==0){
      err = "peer closed connection (recv returned 0)";
      return false;
    }

    //n<0
    
    if(errno == EINTR) continue;

    if(errno == EAGAIN || errno == EWOULDBLOCK){
      err="recv timeout (EAGAIN/EWOULDBLOCK)";
    }
    err = std::string("recv failed: ") + std::strerror(errno);
    return false;
  }
  return true;
  
}

std::vector<uint8_t> sendcom(const char* ip, const uint16_t& port,const uint32_t& com, const std::string& payload)
{
  struct sockaddr_in SiTCP_ADDR;

  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  SiTCP_ADDR.sin_family = AF_INET;
  SiTCP_ADDR.sin_port   = htons(port); // little -> big
  SiTCP_ADDR.sin_addr.s_addr   = inet_addr(ip);// string ->uint32 and little -> big

  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec= 0; 
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
  int flag = 1;
  //  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
  
  if(0>connect(sock, (struct sockaddr*)&SiTCP_ADDR, sizeof(SiTCP_ADDR))){
    throw std::runtime_error(" connect failed [sendcom] ");
    std::cout<<" connect failed [sendcom] "<<std::endl;
    close(sock);
  }

  std::string err;
  
  uint32_t comlen = 4 + payload.size(); // 4 means comm bytes
  if(!send_all(sock, &comlen,4,err)){
    throw std::runtime_error(" send header lengh failed: " + err);
  };

  std::vector<uint8_t> sendbuf;
  sendbuf.resize(comlen);

  if(payload.size()>0){
    std::memcpy(sendbuf.data(), &com, 4);
    std::memcpy(sendbuf.data()+4,payload.data(),payload.size());    
  }else{
    std::memcpy(sendbuf.data(), &com, 4);
  }

  if(!send_all(sock,sendbuf.data(),sendbuf.size(),err)){
    throw std::runtime_error(" send body failed: " + err);
  }

  std::vector<uint8_t> lenbuf;
  lenbuf.resize(4);
  if(!recv_all(sock,lenbuf.data(),lenbuf.size(),err)){
    throw std::runtime_error("recv length header failed: " + err);
  }
  uint32_t retlen = loadLittleEndian32(lenbuf);

  // if(retlen < 4){
  //   //    std::cout<<"lenbuf = "<<lenbuf<<std::endl;
  //   throw std::runtime_error("protocol error: retlen<4 (" + std::to_string(retlen)+")");
  // }

  std::vector<uint8_t> recvbuf(retlen);
  recv_all(sock,recvbuf.data(),recvbuf.size(),err);
  // if(!recv_all(sock,recvbuf.data(),recvbuf.size(),err)){
  //   throw std::runtime_error("recv body failed: " + err);
  // }

  close(sock);
  
  return recvbuf;
}
