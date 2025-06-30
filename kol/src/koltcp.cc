/*
 *
 *
 */

#include <iostream>
#include "kol/koltcp.h"

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif /* !WIN32 */

using namespace kol;

TcpBuffer::TcpBuffer() : m_socket()
{
  initparams();
}

TcpBuffer::TcpBuffer(const Socket& s) : m_socket(s)
{
  initparams();
}

TcpBuffer::TcpBuffer(int domain, int type, int protocol) :
  m_socket( domain, type, protocol )
{
  initparams();
}

TcpBuffer::~TcpBuffer()
{
  m_socket.close();
}

void
TcpBuffer::initparams()
{
  m_gcount = 0;
  m_iostate = goodbit;
  m_rbufmax = bufsize;
  m_rbuflen = 0;
  m_rbufnxt = 0;
  m_sbufmax = bufsize;
  m_sbuflen = 0;
  m_sbufnxt = 0;
}

int
TcpBuffer::close()
{
  flush();
  m_socket.close();
  return 0;
}

int
TcpBuffer::shutdown(int how)
{
  return m_socket.shutdown(how);
}

int
TcpBuffer::getsockname(struct sockaddr* name, socklen_t* namelen) const
{
  return m_socket.getsockname(name, namelen);
}

int
TcpBuffer::getpeername(struct sockaddr* name, socklen_t* namelen) const
{
  return m_socket.getpeername(name, namelen);
}

int
TcpBuffer::getsockopt(int level, int optname, void* optval, socklen_t* optlen) const
{
  return m_socket.getsockopt(level, optname, optval, optlen);
}

int
TcpBuffer::setsockopt(int level, int optname, const void* optval, socklen_t optlen)
{
  return m_socket.setsockopt(level, optname, optval, optlen);
}

int
TcpBuffer::recv_all(unsigned char* buf, int nbytes)
{
  int nleft = 0;

  try
  {
    nleft = nbytes;
    while( nleft > 0 )
    {
      int nrecv = m_socket.recv( buf, nleft );
 	//std::cerr << "#D TcpBuffer::recv_all nrecv: " << nrecv << std::endl;
      if( nrecv < 0 )
      {
        m_iostate |= badbit;
        return nrecv;
      }
      else if( nrecv == 0 )
      {
        m_iostate |= (eofbit | failbit);
        break;
      }
      nleft -= nrecv;
      buf += nrecv;
    }
  }
  catch(...)
  {
    m_iostate |= badbit;
	/**** comment out for the retry after Time Out ****/
    //m_socket.close();
 	//std::cerr << "#D TcpBuffer::recv_all m_socket.close()" << std::endl;
    throw;
  }
  return (nbytes - nleft);
}

int
TcpBuffer::send_all(const unsigned char* buf, int nbytes)
{
  int nleft = 0;

  try
  {
    nleft = nbytes;
    while( nleft > 0 )
    {
      int nsend = m_socket.send(buf, nleft);
      if( nsend <= 0 )
      {
        m_iostate |= badbit;
        break;
      }
      nleft -= nsend;
      buf += nsend;
    }
  }
  catch(...)
  {
    m_iostate |= badbit;
    m_socket.close();
    throw;
  }
  return (nbytes - nleft);
}

int
TcpBuffer::send_all(const unsigned char* buf, int nbytes, int flag)
{
  int nleft = 0;

  try
  {
    nleft = nbytes;
    while( nleft > 0 )
    {
      int nsend = m_socket.send(buf, nleft, flag);
      if( nsend <= 0 )
      {
        m_iostate |= badbit;
        break;
      }
      nleft -= nsend;
      buf += nsend;
    }
  }
  catch(...)
  {
    m_iostate |= badbit;
    m_socket.close();
    throw;
  }
  return (nbytes - nleft);
}

TcpBuffer&
TcpBuffer::read(char* buf, std::streamsize len)
{
  m_gcount = 0;
  if((buf == 0) || (len == 0))
    return *this;
  std::streamsize n = 0;

  if(m_rbuflen == 0)
  {
    int nr = recv_all((unsigned char*)buf, (int)len);
    m_gcount = nr;
    return *this;
  }

  for(n = 0; n < len; n++)
  {
    int c;
    if((c = get()) == std::char_traits<char>::eof())
      break;
    *buf++ = (c & 255);
  }
  m_gcount = n;
  return *this;
}

TcpBuffer&
TcpBuffer::ignore(std::streamsize len)
{
  m_gcount = 0;
  if(len == 0)
    return *this;

  std::streamsize n = 0;
  for(n = 0; n < len; n++)
  {
    int c;
    if((c = get()) == std::char_traits<char>::eof())
      break;
  }
  m_gcount = n;
  return *this;
}

TcpBuffer&
TcpBuffer::write(const void* buf, std::streamsize len)
{
  if((buf == 0) || (len == 0))
    return *this;
//  size_t n = 0;
  unsigned char* p = (unsigned char*)buf;
//  for(n = 0; n < len; n++)
//  {
//    int c = *p++;
//    if(put((c & 255)) == std::char_traits<char>::eof())
//      break;
//  } 
//  return n;
  flush();
  send_all(p, (int)len);
  return *this;
}

TcpBuffer&
TcpBuffer::send(const void* buf, std::streamsize len, int flag)
{
  if((buf == 0) || (len == 0))
    return *this;
  unsigned char* p = (unsigned char*)buf;
  flush();
  send_all(p, (int)len, flag);
  return *this;
}

int
TcpBuffer::get()
{
  int c;

  try
  {
    if(m_rbufnxt >= m_rbuflen)
    {
      m_rbuflen = 0;
      m_rbufnxt = 0;
      int n = m_socket.recv(m_rbuf, m_rbufmax);
      if(n <= 0)
      {
        m_gcount = 0;
        m_iostate |= (eofbit | failbit);
        if(n < 0)
          m_iostate |= badbit;
        return std::char_traits<char>::eof();
      }
      m_rbuflen = n;
    }
    c = ((int)(m_rbuf[m_rbufnxt++])) & 255;
    m_gcount = 1;
  }
  catch(...)
  {
    m_iostate |= badbit;
    m_socket.close();
    throw;
  }
  return c;
}

TcpBuffer&
TcpBuffer::put(int c)
{
  if(m_sbufnxt >= m_sbufmax)
  {
    send_all(m_sbuf, m_sbufnxt);
    m_sbuflen = 0;
    m_sbufnxt = 0;
  }
  if(bad())
    return *this;
  m_sbuf[m_sbufnxt++] = (c & 255);
  return *this;
}

TcpBuffer&
TcpBuffer::getline(char* buf, std::streamsize maxlen)
{
  int delim = 0x0a;

  m_gcount = 0;
  if((buf == 0) || (maxlen == 0))
  {
    m_iostate |= failbit;
    return *this;
  }
  int nget = 0;
  int n = 0;
  buf[n] = 0;
  int nmax = (int)maxlen - 1;
  if(nmax <= 0)
  {
    m_iostate |= failbit;
    return *this;
  }
  int c;
  while((c = get()) != std::char_traits<char>::eof())
  {
    ++nget;
    if(c == delim)
    {
      buf[n] = 0;
      m_gcount = nget;
      return *this;
    }
    buf[n++] = (c & 255);
    if(n >= nmax)
    {
      buf[nmax] = 0;
      m_iostate |= failbit;
      m_gcount = nget;
      return *this;
    }
  }
  m_iostate |= (eofbit | failbit);
  m_gcount = nget;
  buf[n] = 0;
  return *this;
}

TcpBuffer&
TcpBuffer::flush()
{
  if(m_sbufnxt > 0)
  {
    send_all(m_sbuf, m_sbufnxt);
    m_sbufnxt = 0;
    m_sbuflen = 0;
  }
  return *this;
}

TcpSocket::TcpSocket()
{
}

TcpSocket::TcpSocket(const Socket& s) :
  TcpBuffer(s)
{
}

TcpSocket::~TcpSocket()
{
}

TcpClient::TcpClient() :
  TcpBuffer(PF_INET, SOCK_STREAM, 0)
{
}

void
TcpClient::Start(const std::string& host, int port)
{
  try
  {
    struct sockaddr_in srvaddr;
    struct sockaddr_in* resaddr;
    struct addrinfo hints;
    struct addrinfo* res;
    int err;

    res = 0;
    ::memset((char*)&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    if((err = getaddrinfo(host.c_str(), 0, &hints, &res)) != 0)
      throw SocketException("TcpClient::getaddrinfo error: "+std::string(gai_strerror(err)));
//       throw SocketException("TcpClient::getaddrinfo error");
    resaddr = (struct sockaddr_in*)res->ai_addr;

    ::memset((char*)&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family = AF_INET;
    srvaddr.sin_port = htons((u_short)port);
    srvaddr.sin_addr = resaddr->sin_addr;
    freeaddrinfo(res);
    m_socket.connect((struct sockaddr*)&srvaddr, sizeof(srvaddr));
  }
  catch(...)
  {
    m_socket.close();
    throw;
  }
}

TcpClient::TcpClient(const std::string& host, int port) :
  TcpBuffer(PF_INET, SOCK_STREAM, 0)
{
  Start(host.c_str(), port);
}

TcpClient::TcpClient(const Socket& s, const std::string& host, int port) :
  TcpBuffer(s)
{
  Start(host.c_str(), port);
}

TcpClient::~TcpClient()
{
}

TcpServer::TcpServer() :
  TcpBuffer(PF_INET, SOCK_STREAM, 0)
{
}

void
TcpServer::Start(int port, int backlog)
{
  struct sockaddr_in addr;
  ::memset((char*)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((u_short)port);
  if(m_socket.bind((const struct sockaddr*)&addr, sizeof(addr)) == -1)
    return;
  m_socket.listen(backlog);
}

TcpServer::TcpServer(int port, int backlog) :
  TcpBuffer(PF_INET, SOCK_STREAM, 0) 
{
  int on = 1;
  m_socket.setsockopt(SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  Start(port, backlog);
}

TcpServer::TcpServer(const Socket& s, int port, int backlog) :
  TcpBuffer(s)
{
  Start(port, backlog);
}

TcpServer::~TcpServer()
{
}

TcpSocket
TcpServer::accept()
{
  return TcpSocket(m_socket.accept());
}
