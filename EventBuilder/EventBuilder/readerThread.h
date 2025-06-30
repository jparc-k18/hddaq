// -*- C++ -*-
/**
 *  @file   readerThread.h
 *  @brief
 *  @author Kazuo NAKAYOSHI <kazuo.nakayoshi@kek.jp>
 *  @date
 *  @note   Modified by Ken Sakashita <kensh@post.kek.jp>
 *
 *  $Id: readerThread.h,v 1.3 2012/04/13 12:04:11 igarashi Exp $
 *  $Log: readerThread.h,v $
 *  Revision 1.3  2012/04/13 12:04:11  igarashi
 *  include Tomo's improvements
 *    slowReader
 *
 *  Revision 1.2  2008/05/16 06:29:22  igarashi
 *  update eventbuilder control sequence
 *
 *  Revision 1.1.1.1  2008/05/14 15:05:43  igarashi
 *  Network DAQ Software Prototype 1.5
 *
 *  Revision 1.3  2008/05/13 06:41:57  igarashi
 *  change control sequence
 *
 *  Revision 1.2  2008/02/27 16:24:46  igarashi
 *  Control sequence was introduced in ConsoleThread and Eventbuilder.
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
 *  Revision 1.2  2007/02/19 23:59:43  nakayosi
 *  changed all members "protected" to "private".  removed a member "m_running.
 *  removed methods "isRunning()", "setRunFlag(bool flag)".
 *
 *  Revision 1.1.1.1  2007/01/31 13:37:53  kensh
 *  Initial version.
 *
 *
 *
 *
 */
#ifndef READER_THREAD_H
#define READER_THREAD_H

#include <iostream>
#include <string>

#include "kol/kolthread.h"
#include "kol/koltcp.h"
#include "RingBuffer/RingBuffer.h"
#include "ControlThread/statableThread.h"
#include "EventBuilder/EventBuilder.h"

class ReaderThread : public StatableThread
{
public:
  static const int NODE_RB_BUFLEN   = 20;
  static const int HEADER_BYTE_SIZE = 8;
  static const int EVENT_NO_OFFSET  = 8;
  
public:
  ReaderThread(int buflen, int quelen);
  virtual ~ReaderThread();
  void setHost(const std::string& host, int port, int node);
  virtual void initBuffer();
  RingBuffer * getNodeRB();
  virtual EventBuffer * peekReadFragData();
  virtual int releaseReadFragData();
  virtual EventBuffer * peekWriteFragData();
  virtual int releaseWriteFragData();
  virtual int leftEventData();
  virtual int getRingBufferDepth();

  int is_active;
  int is_active4msg;
  
protected:
  int active_loop();
  virtual int writeNullEvent(int event_type = g_EVENT_TYPE_NULL,
			     int event_number=0);
  virtual int readHeader(kol::TcpClient& client,
			 unsigned int* header);
  virtual int updateEventData(kol::TcpClient& client,
			      unsigned int* header,
			      int trans_byte,
			      int rest_byte);

// private:
  std::string m_host;
  int    m_port;
  int    m_node;
  int    m_ringbuf_len;
  RingBuffer * m_node_rb;

};
#endif
