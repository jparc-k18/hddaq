// -*- C++ -*-
/**
 *  @file   EventDistributor.cc
 *  @brief
 *  @author Kazuo NAKAYOSHI <kazuo.nakayoshi@kek.jp>
 *  @date
 *
 *  $Id: EventDistributor.cc,v 1.8 2012/04/27 10:48:09 igarashi Exp $
 *  $Log: EventDistributor.cc,v $
 *  Revision 1.8  2012/04/27 10:48:09  igarashi
 *  debug mutex lock in EventDistributor
 *
 *  Revision 1.7  2012/04/16 11:23:06  igarashi
 *  include Tomo's improvements
 *
 *  Revision 1.5  2009/10/20 09:36:38  igarashi
 *  add executing options eb host, port, etc.
 *
 *  Revision 1.4  2009/10/19 06:05:57  igarashi
 *  *** empty log message ***
 *
 *  Revision 1.3  2009/10/19 04:41:22  igarashi
 *  *** empty log message ***
 *
 *  Revision 1.2  2009/07/01 07:37:40  igarashi
 *  add ANYONE/ENTRY scheme, and NodeId class
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
 *  Revision 1.2  2007/03/07 15:28:48  igarashi
 *  change MessageClient to GlobalMessageClient and little debug
 *
 *  Revision 1.1.1.1  2007/01/31 13:37:53  kensh
 *  Initial version.
 *
 *
 *
 *
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <csignal>

#include <cstring>

#include "kol/koltcp.h"
#include "kol/kolthread.h"
#include "EventData/EventParam.h"
#include "ControlThread/controlThread.h"
#include "ControlThread/GlobalInfo.h"
#include "ControlThread/NodeId.h"
#include "EventDistributor/distReader.h"
#include "EventDistributor/dataServer.h"
#include "EventDistributor/monDataSender.h"
#include "EventDistributor/dataSender.h"
#include "EventDistributor/EventDistributor.h"
#include "EventBuilder/EventBuilder.h"
#include "Message/Message.h"
#include "Message/GlobalMessageClient.h"
#include "EventDistributor/watchdog.h"
#include "EventDistributor/EdControl.h"

bool g_VERBOSE = false;
static const int k_quelen = 2000;

void sigpipehandler(int signum)
{
  fprintf(stderr, "Got SIGPIPE! %d\n", signum);
  return;
}

int set_signal()
{
  struct sigaction act;

  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = sigpipehandler;
  act.sa_flags |= SA_RESTART;

  if(sigaction(SIGPIPE, &act, NULL) != 0 ) {
    fprintf( stderr, "sigaction(2) error!\n" );
    return -1;
  }

  return 0;
}


void print_usage(const std::string& process_name)
{
  std::cout << "#D Usage: \n"
	    << "   " << process_name << " ([option] ...)"
	    << "\n"
	    << "   option: "
	    << "\n"

	    << "          -h, -help, --h, --help"
	    << "\n"
	    << "                    "
	    << " print this message"
	    << "\n\n"

	    << "          --node-id=<number>, --id=<4 ASCII characters>"
	    << "\n"
	    << "                    "
	    << " overwrite Node ID (32 bit) in Event Header with this value"
	    << "\n"
	    << "                    "
	    << " defalut = automatically calculated from IPv4 address"
	    << "\n\n"

	    << "          --nickname=<string>"
	    << "\n"
	    << "                    "
	    << " nickname shown in DAQ Controller widget"
	    << "\n\n"

	    << "          --mon-port=<number>"
	    << "\n"
	    << "                    "
	    << " port number of monitor server."
	    << "\n"
	    << "                    "
	    << " default = 8901"
	    << "\n\n"

	    << "          --rec-port=<number>"
	    << "\n"
	    << "                    "
	    << " port number of recorder server."
	    << "\n"
	    << "                    "
	    << " default = 8902"
	    << "\n\n"

	    << "          --src-host=<string>"
	    << "\n"
	    << "                    "
	    << " hostname or IPv4 address of data source host."
	    << "\n"
	    << "                    "
	    << " default = localhost"
	    << "\n\n"

	    << "          --src-port=<number>"
	    << "\n"
	    << "                    "
	    << " port number of data source."
	    << "\n"
	    << "                    "
	    << " default = 8900 (= event builder port)"
	    << "\n\n"

	    << "          --mon-timeout-sec=<number>"
	    << "\n"
	    << "          --mon-timeout-usec=<number>"
	    << "\n"
	    << "                    "
	    << " timeout value for monitor client socket."
	    << "\n"
	    << "                    "
	    << " default = 1 sec"
	    << "\n\n"

	    << "          --buf-len=<number>"
	    << "\n"
	    << "                    "
	    << " event size of ring buffer (in bytes)"
	    << "\n"
	    << "                    "
	    << " default = " << max_event_len
	    << "\n\n"

	    << "          --que-len=<number>"
	    << "\n"
	    << "                    "
	    << " ring buffer length (in events)"
	    << "\n"
	    << "                    "
	    << " default = " << k_quelen
	    << "\n\n"

	    << std::endl;
  return;
}

int main(int argc, char* argv[])
{
  int buflen = 0;
  // 	const int quelen = 20;
  int quelen = k_quelen;

  std::string src_hostname("localhost");
  unsigned int src_port = eventbuilder_port;
  unsigned int rec_port = eventDist_rec_port;
  unsigned int mon_port = eventDist_mon_port;

  int nodeid = -1;
  std::string nickname = NodeId::getNodeId(NODETYPE_ED, &nodeid);

  unsigned int mon_tv_sec  = 10;
  unsigned int mon_tv_usec = 0;

  for (int i = 1 ; i < argc ; i++) {
    bool is_match = false;
    std::string arg = argv[i];
    const std::string k_opt_srchost("--src-host=");
    const std::string k_opt_srcport("--src-port=");
    const std::string k_opt_recport("--rec-port=");
    const std::string k_opt_monport("--mon-port=");
    const std::string k_opt_mon_timeout_sec("--mon-timeout-sec=");
    const std::string k_opt_mon_timeout_usec("--mon-timeout-usec=");
    const std::string k_opt_buf_len("--buf-len=");
    const std::string k_opt_que_len("--que-len=");
    if (arg=="-h" || arg=="--h" || arg=="-help" || arg=="--help") {
      print_usage(argv[0]);
      return 0;
    }
    if (arg.find(k_opt_srchost)==0) {
      src_hostname = arg.substr(k_opt_srchost.size());
      is_match = true;
    }
    if (arg.find(k_opt_srcport)==0) {
      std::stringstream ss(arg.substr(k_opt_srcport.size()));
      ss >> src_port;
      is_match = true;
    }
    if (arg.find(k_opt_recport)==0) {
      std::stringstream ss(arg.substr(k_opt_recport.size()));
      ss >> rec_port;
      is_match = true;
    }
    if (arg.find(k_opt_monport)==0) {
      std::stringstream ss(arg.substr(k_opt_monport.size()));
      ss >> mon_port;
      is_match = true;
    }
    if (arg.find(k_opt_mon_timeout_sec)==0) {
      std::stringstream
	ss(arg.substr(k_opt_mon_timeout_sec.size()));
      ss >> mon_tv_sec;
      is_match = true;
    }
    if (arg.find(k_opt_mon_timeout_usec)==0) {
      std::stringstream
	ss(arg.substr(k_opt_mon_timeout_usec.size()));
      ss >> mon_tv_usec;
      is_match = true;
    }
    if (arg.find(k_opt_buf_len)==0) {
      std::stringstream
	ss(arg.substr(k_opt_buf_len.size()));
      ss >> buflen;
      is_match = true;
    }
    if (arg.find(k_opt_que_len)==0) {
      std::stringstream
	ss(arg.substr(k_opt_que_len.size()));
      ss >> quelen;
      is_match = true;
    }
    if (arg.substr(0, 10) == "--node-id=") {
      std::istringstream ssval(arg.substr(10));
      ssval >> nodeid;
      is_match = true;
    }
#if 0  //2014.11.26 K.Hosomi
    if (arg.substr(0, 5) == "--id=") {
      char idchar[5];
      strncpy(idchar, arg.substr(5,4).c_str(), 4);
      for (int i = strlen(idchar) ; i < 5 ; i++) {
	idchar[i] = '\0';
      }
      nodeid = *(reinterpret_cast<int *>(idchar));
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

  if (buflen==0) buflen = max_event_len;

  std::cout << "data source: host = " << src_hostname
	    << ", port = " << src_port << std::endl;
  std::cout << "recorder port = " << rec_port << "\n"
	    << "monitor  port = " << mon_port << std::endl;

  std::cout << "NODE ID : " << nodeid
	    << " " << std::showbase << std::hex << nodeid
	    << std::dec << std::noshowbase << std::endl;

  std::cout << "event buffer size = " << buflen
	    << ", que length = " << quelen << std::endl;

  set_signal();

  GlobalMessageClient::getInstance("localhost", g_MESSAGE_PORT_UPSTREAM, nodeid);

  GlobalInfo& gi = GlobalInfo::getInstance();

  gi.nickname = nickname;
  gi.node_id = nodeid;

  //std::string mstring = "ENTRY " + nickname;
  //msock.sendString(MT_STATUS, &mstring);

  try {
    EdControl controller;
    DistReader reader(buflen, quelen);
    reader.setName("%% DistReader");
    reader.setHost(src_hostname.c_str(), src_port);
    gi.reader = &reader;

    DataSender    recDataSender(reader);
    recDataSender.setName("++ recDataSender");
    recDataSender.setTimeout(10, 0);
    DataServer recDataSrv(rec_port, recDataSender);
    gi.sender = &recDataSender;

    MonDataSender monDataSender(reader);
    monDataSender.setName("-- monDataSender");
    monDataSender.setTimeout(mon_tv_sec, mon_tv_usec);
    DataServer monDataSrv(mon_port, monDataSender);
    gi.monsender = &monDataSender;

    controller.setSlave(&recDataSender);
    controller.setSlave(&monDataSender);
    controller.setSlave(&reader);


    controller.sendEntry();
    controller.start();

    recDataSrv.start();
    monDataSrv.start();
    recDataSender.start();
    monDataSender.start();
    reader.start();

    WatchDog watchdog(&controller);
    watchdog.start();


    reader.join(); std::cerr << "reader joined" << std::endl;
    monDataSender.join(); std::cerr << "mondatasender joined" << std::endl;
    recDataSender.join(); std::cerr << "recdatasender joined" << std::endl;

    monDataSrv.cancel();
    recDataSrv.cancel();
    monDataSrv.join(); std::cerr << "mondatasrv joined" << std::endl;
    recDataSrv.join(); std::cerr << "recdatasrv joined" << std::endl;
    controller.join(); std::cerr << "controller joined" << std::endl;
  } catch(...) {
    std::cout << " @@ Data Distributor: ERROR caught" << std::endl;
  }
  std::cerr << "#D main end\n" << std::endl;

  return 0;
}
