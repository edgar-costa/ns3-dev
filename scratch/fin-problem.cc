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
//#include "ns3/utils.h"

using namespace ns3;

//UTILS

void
_RxDropPcap (Ptr<PcapFileWrapper> file, Ptr<const Packet> packet)
{
//	Ptr<PcapFileWrapper> file OLD VERSION
 //NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());

 file->Write (Simulator::Now (), packet);
}

Ipv4Address
_GetNodeIp(Ptr<Node> node)
{

  Ptr<Ipv4> ip = node->GetObject<Ipv4> ();

  ObjectVectorValue interfaces;
  NS_ASSERT(ip !=0);
  ip->GetAttribute("InterfaceList", interfaces);
  for(ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End (); j ++)
  {
    Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface> ();
    NS_ASSERT(ipIface != 0);
    Ptr<NetDevice> device = ipIface->GetDevice();
    NS_ASSERT(device != 0);
    Ipv4Address ipAddr = ipIface->GetAddress (0).GetLocal();

    // ignore localhost interface...
    if (ipAddr == Ipv4Address("127.0.0.1")) {
      continue;
    }
    else {
      return ipAddr;
    }
  }

  return Ipv4Address("127.0.0.1");
}

void _installSink(Ptr<Node> node, uint16_t sinkPort, uint32_t duration, std::string protocol){

  //create sink helper
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));

  if (protocol == "UDP"){
  	packetSinkHelper.SetAttribute("Protocol",StringValue("ns3::UdpSocketFactory"));
	}

  ApplicationContainer sinkApps = packetSinkHelper.Install(node);

  sinkApps.Start (Seconds (0));
  sinkApps.Stop (Seconds (duration));
}

void _installBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime){

  Ipv4Address addr = _GetNodeIp(dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<BulkSendApplication> bulkSender = CreateObject<BulkSendApplication>();

  bulkSender->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
  bulkSender->SetAttribute("MaxBytes", UintegerValue(size));
  bulkSender->SetAttribute("Remote", AddressValue(sinkAddress));


  //Install app
  srcHost->AddApplication(bulkSender);

  bulkSender->SetStartTime(Seconds(startTime));
  bulkSender->SetStopTime(Seconds(1000));

  return;
}

void _installCustomBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime){

  Ipv4Address addr = _GetNodeIp(dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<CustomBulkApplication> bulkSender = CreateObject<CustomBulkApplication>();

  bulkSender->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
  bulkSender->SetAttribute("MaxBytes", UintegerValue(size));
  bulkSender->SetAttribute("Remote", AddressValue(sinkAddress));

  AsciiTraceHelper asciiTraceHelper;

  Ptr<OutputStreamWrapper> flowsCompletionTime = asciiTraceHelper.CreateFileStream ("fct.fct");
  Ptr<OutputStreamWrapper> flowsFile = asciiTraceHelper.CreateFileStream ("flows.flows");

  bulkSender->SetOutputFile(flowsCompletionTime);
  bulkSender->SetFlowsFile(flowsFile);


  //Install app
  srcHost->AddApplication(bulkSender);

  bulkSender->SetStartTime(Seconds(startTime));
  bulkSender->SetStopTime(Seconds(1000));

  return;
}


//FIN UTILS
//////////////////////////////



std::string fileNameRoot = "fin-test";    // base name for trace files, etc

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

int
main (int argc, char *argv[])
{

	//INITIALIZATION

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
 	Config::SetDefault ("ns3::TcpSocketBase::MinRto",TimeValue(MicroSeconds(5000))); //min RTO value that can be set
 	Config::SetDefault ("ns3::TcpSocket::ConnTimeout",TimeValue(MicroSeconds(5000))); // connection retransmission timeout
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

	Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
	em->SetAttribute ("ErrorRate", DoubleValue (error));
	em->SetAttribute ("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

	link.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
//	link.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

	PcapHelper pcapHelper;
	Ptr<PcapFileWrapper> drop_pcap = pcapHelper.CreateFile ("drops.pcap", std::ios::out, PcapHelper::DLT_PPP);
	link.Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&_RxDropPcap, drop_pcap));



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
  _installSink(hosts.Get(1), port, 1000, protocol);

//  _installBulkSend(hosts.Get(0), hosts.Get(1), port, 10000, 1);
  _installCustomBulkSend(hosts.Get(0), hosts.Get(1), port, 10000, 1);

  p2p.EnablePcap(fileNameRoot, link.Get(0), bool(1));
  p2p.EnablePcap(fileNameRoot, link.Get(1), bool(1));



  Simulator::Stop (Seconds (1000));

  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
}

