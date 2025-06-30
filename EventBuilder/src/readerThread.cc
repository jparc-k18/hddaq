// -*- C++ -*-
/**
 *  @file   readerThread.cc
 *  @brief
 *  @author Kazuo NAKAYOSHI <kazuo.nakayoshi@kek.jp>
 *  @date
 *  @note   Modified by Ken Sakashita <kensh@post.kek.jp>
 *
 *  $Id: readerThread.cc,v 1.10 2012/04/17 06:47:12 igarashi Exp $
 *  $Log: readerThread.cc,v $
 *  Revision 1.10  2012/04/17 06:47:12  igarashi
 *  miner refine
 *
 *  Revision 1.9  2012/04/13 12:04:11  igarashi
 *  include Tomo's improvements
 *    slowReader
 *
 *  Revision 1.8  2010/06/28 08:31:50  igarashi
 *  adding C header files to accept the varied distribution compilers
 *
 *  Revision 1.7  2009/12/15 08:27:30  igarashi
 *  minar update for error message and trigger rate display
 *
 *  Revision 1.6  2009/10/19 04:38:38  igarashi
 *  add some messages
 *
 *  Revision 1.5  2009/06/29 02:26:50  igarashi
 *  add debug info
 *
 *  Revision 1.4  2008/06/27 15:30:18  igarashi
 *  *** empty log message ***
 *
 *  Revision 1.3  2008/05/18 15:50:14  igarashi
 *  *** empty log message ***
 *
 *  Revision 1.2  2008/05/16 10:50:06  igarashi
 *  *** empty log message ***
 *
 *  Revision 1.1.1.1  2008/05/14 15:05:43  igarashi
 *  Network DAQ Software Prototype 1.5
 *
 *  Revision 1.4  2008/05/13 06:41:57  igarashi
 *  change control sequence
 *
 *  Revision 1.3  2008/02/27 16:24:46  igarashi
 *  Control sequence was introduced in ConsoleThread and Eventbuilder.
 *
 *  Revision 1.2  2008/02/25 13:24:20  igarashi
 *  Threads start timing was changed.
 *  Threads start as series, reader, builder, sender.
 *
 *  Revision 1.1.1.1  2008/01/30 12:33:33  igarashi
 *  Network DAQ Software Prototype 1.4
 *
 *  Revision 1.1.1.1  2007/09/21 08:50:48  igarashi
 *  prototype-1.3
 *
 *  Revision 1.1.1.1  2007/03/28 07:50:17  cvs
 *  prototype-1.2
 *
 *  Revision 1.3  2007/02/20 00:07:40  nakayosi
 *  removed a member "m_running"
 *  removed methods "isRunning()" and "setRunFlag()"
 *
 *  Revision 1.2  2007/02/16 05:28:00  nakayosi
 *  fixed "data header error neglect" bug
 *
 *  Revision 1.1.1.1  2007/01/31 13:37:53  kensh
 *  Initial version.
 *
 *
 *
 *
 */

#include <cstdio>
#include <cerrno>
#include <cstring>

#include <sstream>

#include "EventBuilder/readerThread.h"
#include "Message/GlobalMessageClient.h"
#include "ControlThread/GlobalInfo.h"


ReaderThread::ReaderThread(int buflen, int quelen)
  : m_ringbuf_len(buflen)
{
  m_node_rb = new RingBuffer(buflen, quelen);
  m_command = STOP;
  m_event_number = 0;
}

ReaderThread::~ReaderThread()
{
  delete m_node_rb;
}

void ReaderThread::setHost(const std::string& host, int port, int node)
{
  m_host = host;
  m_port = port;
  m_node = node;
}

void ReaderThread::initBuffer()
{
  m_node_rb->initBuffer();
}

RingBuffer *ReaderThread::getNodeRB()
{
  return m_node_rb;
}

int ReaderThread::releaseReadFragData()
{
  return m_node_rb->readBufRelease();
}

EventBuffer *ReaderThread::peekReadFragData()
{
  return m_node_rb->readBufPeek();
}

EventBuffer *ReaderThread::peekWriteFragData()
{
  return m_node_rb->writeBufPeek();
}

int ReaderThread::releaseWriteFragData()
{
  return m_node_rb->writeBufRelease();
}

int ReaderThread::leftEventData()
{
  return m_node_rb->left();
}

int ReaderThread::getRingBufferDepth()
{
  return m_node_rb->depth();
}

int ReaderThread::writeNullEvent(int event_type,
				 int event_number)
{
  EventBuffer *event_f = m_node_rb->writeBufPeek();
  char *event_buf = event_f->getBuf();
  struct event_header *ev_header;
  ev_header = reinterpret_cast<struct event_header*>(event_buf);
  ev_header->magic = EV_MAGIC;
  ev_header->size = sizeof(struct event_header) / sizeof(int);
  ev_header->event_number = event_number;
  ev_header->run_number = 0;
  ev_header->node_id = 0;
  ev_header->type = event_type;
  ev_header->nblock = 0;
  ev_header->reserve = 0;
  m_node_rb->writeBufRelease();
  return 0;
}

int ReaderThread::readHeader(kol::TcpClient& client,
			     unsigned int* header)
{
  GlobalMessageClient & msock = GlobalMessageClient::getInstance();
  while (true) {
    if (checkCommand()!=0) break;
    try {
      if (client.read(reinterpret_cast<char*>(header),
		      HEADER_BYTE_SIZE) == 0) {
	// std::cerr << "ReaderThread::active_loop :"
	// 	  << " socket closed " << m_host
	// 	  << std::endl;
	// std::stringstream msg;
	// msg << "EB Reader: " << m_host << ": " << m_port
	//     << " bad connection.";
	// msock.sendString(MT_ERROR, msg.str());
	m_state = IDLE;
	break;
      }
    } catch (kol::SocketException& e) {
      if (e.reason() == EWOULDBLOCK) {
	std::cerr << "#D1 " << m_host << " Data socket timeout. retry"
		  << std::endl;
	client.iostate_good(); /** clear err. bit **/
	continue;
      } else {
	std::cerr << "#E Reader Socket error: " << e.what()
		  << " host: " << m_host << std::endl;
	std::stringstream msg;
	msg << "EB Reader exception : " << m_host << ": " << m_port
	    << " : " << e.what();
	msock.sendString(MT_ERROR, msg.str());
	is_active = 0;
	writeNullEvent();
	return -1;
      }
    }

    if (checkTcp(client, m_name+":"+m_host+":", m_port)!=0) {
      m_state = IDLE;
      break;
    }
    return 0;
  }
  return 1;
}

int ReaderThread::updateEventData(kol::TcpClient& client,
				  unsigned int* header,
				  int trans_byte,
				  int rest_byte)
{
  GlobalMessageClient & msock = GlobalMessageClient::getInstance();

  EventBuffer* event_f = m_node_rb->writeBufPeek();
  char* event_buf      = event_f->getBuf();
  std::memcpy(event_buf, header, HEADER_BYTE_SIZE);
  int status = -1;

  while (true) {
    if (checkCommand()) break;
    try {
      if (rest_byte>0) {
	if (!client.read(event_buf + HEADER_BYTE_SIZE,
			 trans_byte)) break;
	if (!client.ignore(rest_byte)) break;
      } else if (!client.read(event_buf + HEADER_BYTE_SIZE,
			      trans_byte)) break;
      status = 0;
      break;
    } catch (kol::SocketException& e) {
      if (e.reason() == EWOULDBLOCK) {
	//std::cerr
	//<< "#D2 Data socket timeout. retry"
	//<< std::endl;
	client.iostate_good();
	continue;
      }
      std::ostringstream msg;
      msg << "EB: Reader data reading error: "
	  << e.what()
	  << " host: " << m_host;
      msock.sendString(MT_ERROR, msg);
      std::cerr << msg.str() << std::endl;
      status = -1;
      break;
    }
  }

  if (status==0) {
    if (m_node_rb->writeBufRelease() != 0) {
      std::cerr
	<< "ERROR: m_node_rb.writeBufRelease()"
	<< std::endl;
    }
  }

  return status;
}

int ReaderThread::active_loop()
{
  is_active = 1;
  while (GlobalInfo::getInstance().state!=IDLE)
    ::usleep(1);
  m_event_number = 0;

  std::cerr << "** reader entered active_loop()" << std::endl;

  std::stringstream name;
  name << m_name  << " " << m_host << " " << m_port;
  initBuffer();
  //GlobalMessageClient & msock = GlobalMessageClient::getInstance();

  //kol::TcpClient client(m_host, m_port);
  kol::TcpClient client;
  for (int numtry=0;;++numtry) {
    try {
      client.Start(m_host, m_port);
      break;
    } catch (kol::SocketException &e) {
      std::cerr << "Reader cnnection open error: " << e.what()
		<< " host: " << m_host << std::endl;
      is_active = 0;
      writeNullEvent();
      return -1;
    }
  }
  struct timeval timeoutv;

  timeoutv.tv_sec  = 10;
  timeoutv.tv_usec = 0;
  client.setsockopt(SOL_SOCKET, SO_RCVTIMEO,
		    &timeoutv, sizeof(timeoutv));

  m_state = RUNNING;
  while (true) {
    unsigned header[2];
    std::memset(header, 0, HEADER_BYTE_SIZE);
    int status = readHeader(client, header);

    if (status>0) break;
    else if (status<0) return status;

    checkHeader(header[0], name.str());

    size_t trans_byte = (header[1] - 2) * sizeof(unsigned int);
    size_t rest_byte
      = checkDataSize(m_ringbuf_len*sizeof(unsigned int),
		      trans_byte, name.str());
    if (rest_byte<0) continue;

    if (updateEventData(client, header, trans_byte, rest_byte)!=0)
      break;

    m_event_number++;
  }

  client.close();
  std::cerr << "** reader exited active_loop()" << std::endl;
  is_active = 0;
  return 0;
}
