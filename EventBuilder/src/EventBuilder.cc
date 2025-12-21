// -*- C++ -*-
/**
 *  @file   eventbuilder.cc
 *  @brief
 *  @author Kazuo NAKAYOSHI <kazuo.nakayoshi@kek.jp>
 *  @date
 *
 *  $Id: EventBuilder.cc,v 1.5 2012/04/27 10:48:09 igarashi Exp $
 *  $Log: EventBuilder.cc,v $
 *  Revision 1.5  2012/04/27 10:48:09  igarashi
 *  debug mutex lock in EventDistributor
 *
 *  Revision 1.4  2012/04/13 12:04:11  igarashi
 *  include Tomo's improvements
 *    slowReader
 *
 *  Revision 1.3  2010/06/28 08:31:50  igarashi
 *  adding C header files to accept the varied distribution compilers
 *
 *  Revision 1.2  2009/07/01 07:36:58  igarashi
 *  add ANYONE/ENTRY sequence, and NodeId class
 *
 *  Revision 1.1.1.1  2008/05/14 15:05:43  igarashi
 *  Network DAQ Software Prototype 1.5
 *
 *  Revision 1.4  2008/05/14 14:38:53  igarashi
 *  *** empty log message ***
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
 *  Revision 1.2  2007/03/07 15:27:18  igarashi
 *  change MessageClient to GlobalMessageClient
 *
 *  Revision 1.1.1.1  2007/01/31 13:37:53  kensh
 *  Initial version.
 *
 *
 *
 *
 */

#include <cstdlib>
#include <cstring>

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>

#include "kol/koltcp.h"
#include "ControlThread/controlThread.h"
#include "ControlThread/GlobalInfo.h"
#include "ControlThread/NodeId.h"
#include "EventData/EventParam.h"
#include "EventBuilder/readerThread.h"
#include "EventBuilder/syncReaderThread.h"
#include "EventBuilder/slowReaderThread.h"
#include "EventBuilder/builderThread.h"
#include "EventBuilder/senderThread.h"
#include "EventBuilder/nodeInfo.h"
#include "Message/Message.h"
#include "Message/GlobalMessageClient.h"
#include "EventBuilder/watchdog.h"
#include "EventBuilder/EbControl.h"

// #define USE_PARAPORT
#ifdef  USE_PARAPORT
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ppdev.h>
#include <linux/parport.h>
#endif

bool g_VERBOSE = false;

typedef std::multimap<std::string, int, std::less<std::string> > Node_map;
typedef Node_map::value_type node_inf;

typedef std::vector<NodeInfo> Node_info;
Node_info node_info;

const int k_quelen = 2000;

int get_node_inf(const char* filename, Node_map* node_map)
{
  std::ifstream ifs(filename, std::ios::in);
  if(!ifs) {
    std::cerr << "Error: unable to open node-map file " << filename << std::endl;
    return -1;
  }

  std::string line;

  while (ifs.good()) {
    std::getline(ifs, line, '\n');
    std::istringstream ss(line);
    if( (line.find('#')== std::string::npos) && (line.size() > 0)) {
      std::string host;
      int         port;
      int         rb_size;
      int         rb_len;
      std::string flag;
      ss >> host >> port >> rb_size >> rb_len;
      if(!ss.eof()){
	ss >> flag;
	// std::cout << host << " needs handshake " << std::endl;
      }
      node_map->insert( node_inf(host, port) );
      NodeInfo nodeInfo(host, port, rb_size, rb_len, flag);
      node_info.push_back(nodeInfo);
    }
  }
  return node_map->size();
}

#ifdef  USE_PARAPORT
int open_ppdev()
{
  int ret = 0;
  int fd = ::open("/dev/parport0", O_RDWR);
  if(fd==-1){
    std::cerr << "open error?" << std::endl;
    ret = 1;
  }
  else {
    ret = fd;
  }
  if(::ioctl(fd,PPCLAIM)){
    std::cerr << "PPCLAIM error?" << std::endl;
    ::close(fd);
    ret = 1;
  }
  return ret;
}

void close_ppdev(int fd)
{
  ::ioctl(fd, PPRELEASE);
  ::close(fd);
}
#endif

void sigpipehandler(int signum)
{
  std::cerr << "Got SIGPIPE! " << signum << std::endl;
}

int set_signal()
{
  struct sigaction act;

  std::memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = sigpipehandler;
  act.sa_flags |= SA_RESTART;

  if( sigaction(SIGPIPE, &act, NULL) != 0 ){
    std::cerr << "sigaction(2) error!" << std::endl;
    return -1;
  }
  return 0;
}

int main(int argc, char* argv[])
{
  int event_buflen = max_event_len;

  NodeInfo nodeInfo;
  std::vector<ReaderThread *> readers;
  Node_map node_map;

  if (argc <= 1) {
    std::cerr << "Error: node-map filename missing" << std::endl;
    return -1;
  }
  //int nodeid = g_NODE_EB;
  //std::string nickname = "EB";

  int nodeid;
  std::string nickname = NodeId::getNodeId(NODETYPE_EB, &nodeid);

  std::string nodemapname = "nodemap.txt";
  for(int i=1 ; i<argc ; i++){
    std::string arg = argv[i];
    if( arg.size() > 0 && arg[0] != '-' ){
      nodemapname = argv[i];
    } else {
      bool is_match = false;
      if (arg.substr(0, 11) == "--idnumber=") {
	std::istringstream ssval(arg.substr(11));
	ssval >> nodeid;
	std::cout << "NODE ID : " << nodeid << std::endl;
	is_match = true;
      }
#if 0  //2014.11.26 K.Hosomi
      if (arg.substr(0, 5) == "--id=") {
	//union {char idchar[5]; int idint;};
	char idchar[5];
	strncpy(idchar, arg.substr(5,4).c_str(), 4);
	for (int i = strlen(idchar) ; i < 5 ; i++) {
	  idchar[i] = '\0';
	}
	nodeid = *(reinterpret_cast<int *>(idchar));
	//nodeid = idint;
	std::cout << "NODE ID : " << nodeid << std::endl;
	is_match = true;
      }
#endif //2014.11.26 K.Hosomi
      if (arg.substr(0, 11) == "--nickname=") {
	nickname = arg.substr(11);
	std::cout << "NICKNAME : " << nickname << std::endl;
	is_match = true;
      }
      if (!is_match) {
	std::cout << "unknown option " << arg << std::endl;
      }
    }
  }



#ifdef  USE_PARAPORT
  int ppdev_fd = open_ppdev();
#endif

  set_signal();
  GlobalMessageClient& msock =
    GlobalMessageClient::getInstance("localhost",
				     g_MESSAGE_PORT_UPSTREAM, nodeid);
  GlobalInfo& gi = GlobalInfo::getInstance();

  gi.nickname = nickname;
  gi.node_id = nodeid;

  //std::string mstring = "ENTRY " + nickname;
  //msock.sendString(MT_STATUS, &mstring);

  try
    {
      // int node_number = get_node_inf(argv[1], &node_map);
      int node_number = get_node_inf(nodemapname.c_str(), &node_map);
      if(node_number == -1)
	return -1;
      if(node_number > max_node_num){
	std::stringstream msg;
	msg << "EB: node number is too much! ["
	    << node_number << "/" << max_node_num << "]";
	std::cerr << msg.str() << std::endl;
	msock.sendString(MT_FATAL, msg.str());
	return -1;
      }

      //     readers = new ReaderThread * [node_number];
      readers.resize(node_number);

      for(int node=0; node<node_number; node++) {
	int node_buflen = node_info[node].getRingbufSize();
	int quelen = node_info[node].getRingbufLen();
	const std::string& flag = node_info[node].getSyncFlag();
	if (flag.empty())
	  readers[node] = new ReaderThread(node_buflen, quelen);
	else if (flag=="slow")
	  readers[node] = new SlowReaderThread(node_buflen, quelen);

	//	const char *hostname = node_info[node].getHostName().c_str();
	const std::string hostname = node_info[node].getHostName();
	int port       = node_info[node].getPortNo();
	std::stringstream name;
	name << "** ReaderThread" << std::setw(3) << node;
	readers[node]->setName(name.str());
	readers[node]->setHost(hostname, port, node);
	std::cout<<hostname;
	// std::cerr << "  hostname:" << node_info[node].getHostName();
	// std::cerr << "  RingBuf Size:" << node_buflen
	// 	  << "  RingBuf len:" << quelen << " "
	// 	  << (flag.empty() ? "" : " +"+flag) << std::endl;
	std::stringstream msg;
	msg << "EB: node =" << std::setw(15) << hostname
	    << "  port =" << std::setw(5) << port
	    << "  Bsize =" << std::setw(6) << node_buflen
	    << "  Nque =" << std::setw(6) << quelen;
	std::cout << msg.str() << std::endl;
	//	std::cout << node_info[node].getHostName() << std::endl;
	msock.sendString(msg.str());
      }

      BuilderThread builder(event_buflen, k_quelen);
      builder.setName("## BuilderThread");
      builder.setDebugPrint(0);
      builder.setAllReaders(&readers[0], node_number);
      {
	std::stringstream msg;
	msg << "## bulderThread: Bsize = " << event_buflen
	    << " Nque = " << k_quelen;
	std::cout << msg.str() << std::endl;
	msock.sendString(msg.str());
      }

#ifdef  USE_PARAPORT
      builder.setParaFd(ppdev_fd);
#endif

      SenderThread  sender(event_buflen, k_quelen);
      sender.setName("== SenderThread");
      sender.setBuilder(&builder);

      EbControl controller;
      controller.setSlave(&sender);
      controller.setSlave(&builder);
      for(int node=0; node<node_number; node++)
	controller.setSlave(readers[node]);

      gi.builder = &builder;
      gi.sender = &sender;
      for (int i = 0 ; i < node_number ; i++) {
	gi.readers.push_back(readers[i]);
      }
      gi.state = IDLE;

      controller.sendEntry();
      controller.start();

      sender.start();
      for(int node=0; node < node_number; node++)
	readers[node]->start();
      std::cerr << "readers start" << std::endl;

      builder.start();

      WatchDog watchdog(&controller, &builder, &readers[0], node_number);
      watchdog.start();

      builder.join();
      sender.join();
      for(int node=0; node < node_number; node++)
	readers[node]->join();
      controller.join();
    }
  catch(...)
    {
      std::cout << " @@ Eventbuilder: ERROR caught" << std::endl;
#ifdef  USE_PARAPORT
      close_ppdev(ppdev_fd);
#endif
    }

#ifdef  USE_PARAPORT
  close_ppdev(ppdev_fd);
#endif

  //   delete[] readers;

  std::cerr << "#D main end" << std::endl;

  return 0;
}
