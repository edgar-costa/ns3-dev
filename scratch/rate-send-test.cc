/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <fstream>
#include <string>
#include <iostream>
#include <unordered_map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

#include "ns3/traffic-generation-module.h"
#include "ns3/utils.h"

using namespace ns3;


std::string fileNameRoot = "rate-send-test";    // base name for trace files, etc

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

int
main (int argc, char *argv[])
{

	//INITIALIZATION

//	LogComponentEnable("rate-send-test", LOG_ALL);
//	LogComponentEnable("rate-send-app", LOG_ALL);
//	LogComponentEnable("custom-bulk-app", LOG_ALL);



	//Set simulator's time resolution (click)
	Time::SetResolution (Time::NS);


  DataRate linkBandiwdth("10Mbps");
  //Command line arguments
  uint16_t queue_size = 100;
  uint64_t delay = 50;  //milliseconds
  std::string ecmpMode = "ECMP_RANDOM";

  double error = 0;
  bool animation = true;
  uint64_t runStep = 1;

  uint64_t flow_size =1500;
  uint64_t n_packets= 10;
  double duration = 1;


  CommandLine cmd;
  //Misc
  cmd.AddValue("Animation", "Enable animation module" , animation);
  //General
  cmd.AddValue ("EcmpMode", "EcmpMode: (0 none, 1 random, 2 flow, 3 Round_Robin)", ecmpMode);
	cmd.AddValue("LinkBandwidth", "Bandwidth of link, used in multiple experiments", linkBandiwdth);
	cmd.AddValue("Delay", "Added delay between nodes", delay);

	//Experiment
  cmd.AddValue("QueueSize", "Interfaces Queue length", queue_size);
  cmd.AddValue("RunStep", "random seed", runStep);

  cmd.AddValue("FlowSize", "Interfaces Queue length", flow_size);
  cmd.AddValue("Duration", "random seed", duration);
  cmd.AddValue("Packets", "random seed", n_packets);


  cmd.AddValue("Error", "", error);


  cmd.Parse (argc, argv);

	RngSeedManager::SetRun (runStep);   // Changes run number from default of 1 to 7


  //GLOBAL CONFIGURATION

  //Routing
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", StringValue(ecmpMode));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

	//Tcp Socket (general socket conf)

	Config::SetDefault ("ns3::TcpSocket::ConnCount",UintegerValue(10)); //retrnamissions during connection
 	Config::SetDefault ("ns3::TcpSocket::DataRetries", UintegerValue (10)); //retranmissions
 	Config::SetDefault ("ns3::TcpSocketBase::MinRto",TimeValue(MicroSeconds(50000))); //min RTO value that can be set
 	Config::SetDefault ("ns3::TcpSocket::ConnTimeout",TimeValue(MicroSeconds(50000))); // connection retransmission timeout
// 	Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue(MicroSeconds(10)));
 	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(1));
// 	Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue(true)); //disable nagle's algorithm


	//TCP L4
	Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
 	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446)); //MTU


	//QUEUES
	//PFIFO
	Config::SetDefault ("ns3::PfifoFastQueueDisc::Limit", UintegerValue (queue_size));


  //Define acsii helper
  AsciiTraceHelper asciiTraceHelper;


  //Define Interfaces

  //Define the csma channel

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (linkBandiwdth));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delay)));
  p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2p.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"));
  p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));

  NodeContainer hosts;
  hosts.Create(2);

  NetDeviceContainer link  = p2p.Install(NodeContainer(hosts.Get(0), hosts.Get(1)));



  // Install Internet stack and assing ips
  InternetStackHelper stack;
  stack.Install (hosts);

  Ipv4AddressHelper address("10.0.1.0", "255.255.255.0");
	address.Assign(link);

	//Not needed but i leave it here..
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  ///////////////////
  //START TRAFFIC
  ///////////////////
  uint16_t port = 7000;
  std::string protocol = "TCP";
  installSink(hosts.Get(1), port, 1000, protocol);

//  installBulkSend(hosts.Get(0), hosts.Get(1), port, flow_size, 1);

  installRateSend(hosts.Get(0), hosts.Get(1), port, n_packets, flow_size, duration, 1);


  p2p.EnablePcap(fileNameRoot, link.Get(0), bool(1));
  p2p.EnablePcap(fileNameRoot, link.Get(1), bool(1));



  Simulator::Stop (Seconds (1000));

  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
}

