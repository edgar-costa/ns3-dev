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

//#include "ns3/traffic-generation-module.h"
//#include "ns3/utils.h"

using namespace ns3;

//UTILS
//Returns node if added to the name system , 0 if it does not exist
Ptr<Node> GetNode(std::string name){
	return Names::Find<Node>(name);
}

Ipv4Address
GetNodeIp(Ptr<Node> node)
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

void installSink(Ptr<Node> node, uint16_t sinkPort, uint32_t duration, std::string protocol){

  //create sink helper
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));

  if (protocol == "UDP"){
  	packetSinkHelper.SetAttribute("Protocol",StringValue("ns3::UdpSocketFactory"));
	}

  ApplicationContainer sinkApps = packetSinkHelper.Install(node);

  sinkApps.Start (Seconds (0));
  sinkApps.Stop (Seconds (duration));
}

void installBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime){

  Ipv4Address addr = GetNodeIp(dstHost);
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


//FIN UTILS
//////////////////////////////



std::string fileNameRoot = "ecmp-test";    // base name for trace files, etc

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


  bool animation = true;


  CommandLine cmd;
  //Misc
  cmd.AddValue("Animation", "Enable animation module" , animation);
  //General
  cmd.AddValue ("EcmpMode", "EcmpMode: (0 none, 1 random, 2 flow, 3 Round_Robin)", ecmpMode);
	cmd.AddValue("LinkBandwidth", "Bandwidth of link, used in multiple experiments", linkBandiwdth);
	cmd.AddValue("Delay", "Added delay between nodes", delay);
	//Experiment
  cmd.AddValue("QueueSize", "Interfaces Queue length", queue_size);

  cmd.Parse (argc, argv);



  //GLOBAL CONFIGURATION

  //Routing
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", StringValue(ecmpMode));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

	//Tcp Socket (general socket conf)

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

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue (linkBandiwdth));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delay)));
//  csma.SetChannelAttribute("FullDuplex", BooleanValue("True"));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  csma.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));


  //Define point to point
//  PointToPointHelper csma;
//
////   create point-to-point link from A to R
//  csma.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (linkBandiwdth)));
//  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delay)));
//  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
//
//  csma.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"));
////  csma.SetQueue("ns3::DropTailQueue", "Mode", EnumValue(DropTailQueue::QUEUE_MODE_BYTES));
////  csma.SetQueue("ns3::DropTailQueue", "MaxBytes", UintegerValue(queue_size*1500));
//  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));

  uint32_t num_hosts =  4;

  NodeContainer hosts;
  hosts.Create(num_hosts*2);

  NodeContainer routers;
  routers.Create(num_hosts+2);
	std::stringstream host_name;
	std::stringstream router_name;
	std::stringstream central_router_name;

  uint32_t i = 0;
  for (auto it = hosts.Begin(); it != hosts.End(); it++){
  		if (i < hosts.GetN()/2){
				host_name << "s" << i;
				Names::Add(host_name.str(), (*it));
				host_name.str("");
  		}
  		else{
				host_name << "d" << i - (hosts.GetN()/2);
				Names::Add(host_name.str(), *it);
				host_name.str("");
  		}
  		i++;
  }
  i =0;
  for (auto it = routers.Begin(); it != routers.End(); it++){
  		router_name << "r" << i;
  		Names::Add(router_name.str(), *it);
  		router_name.str("");
  		i++;
  }

  //Links Between devices
  std::unordered_map<std::string, NetDeviceContainer> links;

  //connect sources with first router and router with all the central routers
	router_name << "r" << "0";
  for (uint32_t i = 0; i < (hosts.GetN()/2) ; i++){
		host_name << "s" << i;
		central_router_name << "r" << (i+1);
	  links[host_name.str()+"->"+router_name.str()] = csma.Install (NodeContainer(GetNode(host_name.str()),GetNode(router_name.str())));
	  links[router_name.str()+"->"+central_router_name.str()] = csma.Install (NodeContainer(GetNode(router_name.str()),GetNode(central_router_name.str())));
	  central_router_name.str("");
	  host_name.str("");
  }
  router_name.str("");

  //Connect destinations with
	router_name << "r" << (routers.GetN() -1);
  for (uint32_t i = 0; i < (hosts.GetN()/2) ; i++){
		host_name << "d" << i;
		central_router_name << "r" << (i+1);
	  links[central_router_name.str()+"->"+router_name.str()] = csma.Install (NodeContainer(GetNode(router_name.str()),GetNode(central_router_name.str())));
	  links[host_name.str()+"->"+router_name.str()] = csma.Install (NodeContainer(GetNode(host_name.str()),GetNode(router_name.str())));
	  host_name.str("");
	  central_router_name.str("");
  }
  router_name.str("");

  // Install Internet stack and assing ips
  InternetStackHelper stack;
  stack.Install (hosts);
  stack.Install (routers);


  //Assign IPS
  //Uninstall FIFO queue //  //uninstall qdiscs
  TrafficControlHelper tch;

  Ipv4AddressHelper address("10.0.1.0", "255.255.255.0");
  for (auto it : links){
  	address.Assign(it.second);
  	address.NewNetwork();
//  	tch.Uninstall(it.second);
  }


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  ///////////////////
  //START TRAFFIC
  ///////////////////
  uint16_t port = 5000;
  std::string protocol = "TCP";
  installSink(GetNode("d0"), port, 1000, protocol);

  installBulkSend(GetNode("s0"), GetNode("d0"), port, 10000, 1);

  csma.EnablePcap(fileNameRoot, links["s0->r0"].Get(0), bool(1));


 //ANIMATION

  //Allocate nodes in a fat tree shape
	for (uint32_t i = 0; i < hosts.GetN()/2; i++){
				Ptr<Node> host = hosts.Get(i);
				AnimationInterface::SetConstantPosition(host, 0, i*2 , 0);

	}

	for (uint32_t i = hosts.GetN()/2; i < hosts.GetN(); i++){
				Ptr<Node> host = hosts.Get(i);
				AnimationInterface::SetConstantPosition(host, 16, (i-(hosts.GetN()/2))*2 , 0);

	}

	for (uint32_t i =0; i < routers.GetN(); i++){
		Ptr<Node> router = routers.Get(i);

		if (i == 0){
			AnimationInterface::SetConstantPosition(router, 4, hosts.GetN()-1 , 0);
		}
		else if( i == routers.GetN()-1 ){
			AnimationInterface::SetConstantPosition(router, 12, hosts.GetN()-1 , 0);
		}
		else{
			AnimationInterface::SetConstantPosition(router, 8, (i-1)*2 , 0);
		}

	}

	AnimationInterface anim(fileNameRoot + ".anim");
  if (animation){
		//Animation
		//AnimationInterface anim(outputNameRoot+".anim");
		anim.SetMaxPktsPerTraceFile(10000000);

	//  setting colors
		for (uint32_t i = 0; i < hosts.GetN()/2; i++){
					Ptr<Node> host = hosts.Get(i);
					anim.SetConstantPosition(host, 0, i*2 , 0);
					anim.UpdateNodeColor(host, 0, 0, 255);
					anim.UpdateNodeSize(host->GetId(), 0.5, 0.5);

		}

		for (uint32_t i = hosts.GetN()/2; i < hosts.GetN(); i++){
					Ptr<Node> host = hosts.Get(i);
					anim.UpdateNodeColor(host, 0, 0, 255);
					anim.UpdateNodeSize(host->GetId(), 0.5, 0.5);
					anim.SetConstantPosition(host, 16, (i-(hosts.GetN()/2))*2 , 0);

		}

		for (uint32_t i =0; i < routers.GetN(); i++){
			Ptr<Node> router = routers.Get(i);
			anim.UpdateNodeColor(router, 255,10, 10);
			anim.UpdateNodeSize(router->GetId(), 0.5, 0.5);

			if (i == 0){
				anim.SetConstantPosition(router, 4, hosts.GetN()/2-1 , 0);
			}
			else if( i == routers.GetN()-1 ){
				anim.SetConstantPosition(router, 12, hosts.GetN()/2-1 , 0);
			}
			else{
				anim.SetConstantPosition(router, 8, (i-1)*2 , 0);
			}

		}

		anim.EnablePacketMetadata(true);
  }
  else{
  	anim.SetStopTime(Seconds(0));
  }

  Simulator::Stop (Seconds (100));

  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
}

