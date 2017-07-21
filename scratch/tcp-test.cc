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
#include "ns3/flow-monitor-module.h"
#include "ns3/utils.h"


using namespace ns3;

std::string fileNameRoot = "tcp-test";    // base name for trace files, etc
std::string outputNameRoot = "outputs/";
std::string outputNameFct = "";

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

const char * file_name = g_log.Name();
std::string script_name = file_name;

int counter = 0;


//TRACE SINKS

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
//  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

static void
RxDropPcap (Ptr<PcapFileWrapper> file, Ptr<const Packet> packet)
{
//	Ptr<PcapFileWrapper> file OLD VERSION
  //NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());

  file->Write (Simulator::Now (), packet);
}

static void
RxDropAscii (Ptr<OutputStreamWrapper> file, Ptr<const Packet> packet)
{
//	Ptr<PcapFileWrapper> file OLD VERSION
  //NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());

	Ptr<Packet> p = packet->Copy();

	PppHeader ppp_header;
	p->RemoveHeader(ppp_header);

	Ipv4Header ip_header;
	p->RemoveHeader(ip_header);


  std::ostringstream oss;
  oss << Simulator::Now().GetSeconds() << " "
  		<< ip_header.GetSource() << " "
      << ip_header.GetDestination() << " "
      << int(ip_header.GetProtocol()) << " ";

	if (ip_header.GetProtocol() == uint8_t(17)){ //udp
    UdpHeader udpHeader;
    p->PeekHeader(udpHeader);
    oss << int(udpHeader.GetSourcePort()) << " "
        << int(udpHeader.GetDestinationPort()) << " ";

	}
	else if (ip_header.GetProtocol() == uint8_t(6)) {//tcp
    TcpHeader tcpHeader;
    p->PeekHeader(tcpHeader);
    oss << int(tcpHeader.GetSourcePort()) << " "
        << int(tcpHeader.GetDestinationPort()) << " ";
	}

	oss << packet->GetSize() << "\n";
	*(file->GetStream()) << oss.str();
  (file->GetStream())->flush();

//  file->Write (Simulator::Now (), p);
}

static void
TxDrop (std::string s, Ptr<const Packet> p){
	static int counter = 0;
	NS_LOG_UNCOND (counter++ << " " << s << " at " << Simulator::Now ().GetSeconds ()) ;
}

void PrintQueueSize(Ptr<Queue<Packet>> q){
	uint32_t size = q->GetNPackets();
	if (size > 0){
		NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " " <<  size);
	}
	Simulator::Schedule(Seconds(0.001), &PrintQueueSize, q);
}


int
main (int argc, char *argv[])
{

	//INITIALIZATION

	//Set simulator's time resolution (click)
	Time::SetResolution (Time::NS);

  //Fat tree parameters
  int k =  4;
  DataRate linkBandiwdth("10Mbps");

  //Command line arguments
  std::string ecmpMode = "ECMP_NONE";
  std::string protocol = "TCP";
  std::string simulationName = "default";
  std::string outputFolder = "";

  uint16_t queue_size = 100;
  uint64_t runStep = 1;
  double simulationTime = 10;
  uint64_t interArrivalFlowsTime = (k/2) * (k/2) * k; //at the same amout per seconds as hosts we have in the network
  double intraPodProb = 0;
  double interPodProb = 1;
  std::string sizeDistributionFile = "distributions/default.txt";
  std::string trafficPattern = "distribution";

  uint64_t delay = 50;  //milliseconds

	//12 because there the packet crosses 12 interfaces every RTT. Should also add the time it takes to send 1 packet to the network.
  // my estimated RTT
	int rtt = 12*delay + (12*12);

	uint32_t minRTO = rtt*2;

  int64_t flowlet_gap = rtt/2;
  double flowlet_gap_scaling = 1;


  bool animation = false;
  bool monitor = false;
  bool debug = false;

  //error model
  std::string errorLink = "r_0_a0->r_c0";
  double errorRate = 0;
  bool both_directions;

  CommandLine cmd;

  //Error model
  cmd.AddValue("ErrorLink", "Link that will suffer error", errorLink);
  cmd.AddValue("ErrorRate", "Rate of drop per packet", errorRate);

  //Misc
  cmd.AddValue("Animation", "Enable animation module" , animation);
  cmd.AddValue("Monitor", "" , monitor);
  cmd.AddValue("Debug", "" , debug);

  //Naming
  cmd.AddValue("SimulationName", "Name of the experiment: " , simulationName);
  cmd.AddValue("OutputFolder", "Set it to something if you want to store the simulation output "
  		"in a particular folder inside outputs", outputFolder);

  cmd.AddValue("Protocol", "Protocol used by the traffic Default is: "+protocol , protocol);

  //General
  cmd.AddValue ("EcmpMode", "EcmpMode: (0 none, 1 random, 2 flow, 3 Round_Robin)", ecmpMode);
	cmd.AddValue("LinkBandwidth", "Bandwidth of link, used in multiple experiments", linkBandiwdth);
	cmd.AddValue("Delay", "Added delay between nodes", delay);

	//Experiment
	cmd.AddValue("SimulationTime", "Total simulation time (flow starting time is scheduled until that time, "
			"the simulation time will be extended by the longest flow scheduled close to simulationTime", simulationTime);
	cmd.AddValue("IntraPodProb","Probability of picking a destination inside the same pod", intraPodProb);
	cmd.AddValue("InterPodProb", "Probability of picking a destination in another pod", interPodProb);
	cmd.AddValue("InterArrivalFlowTime", "flows we start per second", interArrivalFlowsTime);
	cmd.AddValue("SizeDistribution", "File with the flows size distribution", sizeDistributionFile);
  cmd.AddValue("QueueSize", "Interfaces Queue length", queue_size);
  cmd.AddValue("FlowletGapScaling", "Inter-arrival packet time for flowlet expiration", flowlet_gap_scaling);
  cmd.AddValue("K", "Fat tree size", k);
  cmd.AddValue("RunStep", "Random generator starts at", runStep);
  cmd.AddValue("TrafficPattern","stride or distribution", trafficPattern);
  cmd.AddValue("BothDirections", "" , both_directions);

  cmd.Parse (argc, argv);

	//Change that if i want to get different random values each run otherwise i will always get the same.
	RngSeedManager::SetRun (runStep);   // Changes run number from default of 1 to 7

	if (debug){
		LogComponentEnable("Ipv4GlobalRouting", LOG_DEBUG);
		//LogComponentEnable("Ipv4GlobalRouting", LOG_ERROR);
		LogComponentEnable("tcp-test", LOG_ERROR);
		LogComponentEnable("utils", LOG_ERROR);
		LogComponentEnable("traffic-generation", LOG_DEBUG);
		LogComponentEnable("custom-bulk-app", LOG_DEBUG);
		//LogComponentEnable("PacketSink", LOG_ALL);
		//LogComponentEnable("TcpSocketBase", LOG_ALL);


	}

  //Update root name
	std::ostringstream run;
	run << runStep;

  outputNameRoot = outputNameRoot + outputFolder + "/" + fileNameRoot;
  outputNameFct = outputNameRoot + "-" +  simulationName + "_" +  std::string(run.str()) + ".fct";

  NS_LOG_UNCOND(outputNameRoot << " " <<  outputNameFct <<  " " << simulationName);

  //General default configurations
  //putting 1500bytes into the wire
  double packetDelay = double(1500*8)/DataRate(linkBandiwdth).GetBitRate();
	double small_packetDelay = double(58*8)/DataRate(linkBandiwdth).GetBitRate();

	rtt = 8*delay + (4*Seconds(packetDelay).GetMicroSeconds()) + (4*Seconds(small_packetDelay).GetMicroSeconds());

	int small_rtt = 8*delay + (8*Seconds(small_packetDelay).GetMicroSeconds());

	minRTO = rtt*2;

  flowlet_gap = rtt*flowlet_gap_scaling; //milliseconds



  //TCP
  NS_LOG_UNCOND("rtt: " << rtt << " minRTO: " << minRTO << " flowlet_gap: " << flowlet_gap
  		<< " delay: " << delay << " packetDelay: " << Seconds(packetDelay).GetMicroSeconds()
			<< "small_rtt: "<< small_rtt <<" Flowlet gap:" << flowlet_gap << " Microseconds");

  //GLOBAL CONFIGURATION

  //Routing
  Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", StringValue(ecmpMode));
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  //Flowlet Switching
  Config::SetDefault("ns3::Ipv4GlobalRouting::FlowletGap", IntegerValue(MicroSeconds(flowlet_gap).GetNanoSeconds()));

  //Dirll LB
  Config::SetDefault("ns3::Ipv4GlobalRouting::DrillRandomChecks", UintegerValue(2));
  Config::SetDefault("ns3::Ipv4GlobalRouting::DrillMemoryUnits", UintegerValue(1));


	//Tcp Socket (general socket conf)
//  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1500000000));
//  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1500000000));
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446)); //MTU
	Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(4294967295));
	Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
	//Can be much slower than my rtt because packet size of syn is 60bytes
	Config::SetDefault ("ns3::TcpSocket::ConnTimeout",TimeValue(MicroSeconds(10*small_rtt))); // connection retransmission timeout
	Config::SetDefault ("ns3::TcpSocket::ConnCount",UintegerValue(10)); //retrnamissions during connection
	Config::SetDefault ("ns3::TcpSocket::DataRetries", UintegerValue (10)); //retranmissions
	Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue(MicroSeconds(rtt)));
	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(2));
	Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue(true)); //disable nagle's algorithm
	Config::SetDefault ("ns3::TcpSocket::PersistTimeout", TimeValue(NanoSeconds(6000000000))); //persist timeout to porbe for rx window

	//Tcp Socket Base: provides connection orientation, sliding window, flow control; congestion control is delegated to the subclasses (i.e new reno)

	Config::SetDefault ("ns3::TcpSocketBase::MaxSegLifetime",DoubleValue(10));
//	Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue(true)); //enable sack
	Config::SetDefault ("ns3::TcpSocketBase::MinRto",TimeValue(MicroSeconds(minRTO))); //min RTO value that can be set
  Config::SetDefault ("ns3::TcpSocketBase::ClockGranularity", TimeValue(MicroSeconds(1)));
  Config::SetDefault ("ns3::TcpSocketBase::ReTxThreshold", UintegerValue(3)); //same than DupAckThreshold
//	Config::SetDefault ("ns3::TcpSocketBase::LimitedTransmit",BooleanValue(true)); //enable sack

	//TCP L4
	Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
	Config::SetDefault ("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(rtt)));

	//QUEUES
	//PFIFO
	Config::SetDefault ("ns3::PfifoFastQueueDisc::Limit", UintegerValue (queue_size));

	//RED configuration
        //Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_BYTES"));
        // Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1000));
        //Config::SetDefault ("ns3::RedQueueDisc::Wait", BooleanValue (true));
        //Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
        //Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (0.005));
        //Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (25));
        //Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (50));
        //Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queue_size*1500));





  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", DataRateValue (DataRate (linkBandiwdth)),
                           "LinkDelay", TimeValue(MicroSeconds(delay)));

  //Define acsii helper
  AsciiTraceHelper asciiTraceHelper;


  //Define Interfaces

  //Define the csma channel

//  CsmaHelper csma;
//  csma.SetChannelAttribute("DataRate", DataRateValue (dataRate));
//  csma.SetChannelAttribute("Delay", StringValue ("0.5ms"));
//  csma.SetChannelAttribute("FullDuplex", BooleanValue("True"));
//  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
//  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));

  //Define point to point
  PointToPointHelper csma;

//   create point-to-point link from A to R
  csma.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (linkBandiwdth)));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(delay)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  csma.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"));
//  csma.SetQueue("ns3::DropTailQueue", "Mode", EnumValue(DropTailQueue::QUEUE_MODE_BYTES));
//  csma.SetQueue("ns3::DropTailQueue", "MaxBytes", UintegerValue(queue_size*1500));
  csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));


  uint32_t num_hosts =  8;

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
  	tch.Uninstall(it.second);
//  	tchRed.Install(it.second);
  }

  //Create a ip to node mapping
  std::unordered_map<std::string, Ptr<Node>> ipToNode;

  for (uint32_t host_i = 0; host_i < hosts.GetN(); host_i++){
  	Ptr<Node> host = hosts.Get(host_i);
  	ipToNode[ipv4AddressToString(GetNodeIp(host))] = host;
  }

  //Store in a file ip -> node name
  Ptr<OutputStreamWrapper> ipToName_file = asciiTraceHelper.CreateFileStream (outputNameRoot+"-ipToName");
  for (auto it = ipToNode.begin();  it != ipToNode.end(); it++){
  	*(ipToName_file->GetStream()) << it->first << " " << GetNodeName(it->second) << "\n";
  }
  ipToName_file->GetStream()->flush();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


///////////////////
//START TRAFFIC
///////////////////

//  //Prepare sink app
  uint32_t sinks = 20;
  std::unordered_map <std::string, std::vector<uint16_t>> hostToPort = installSinks(hosts, sinks, 1000 , protocol);

  Ptr<OutputStreamWrapper> flowsCompletionTime = asciiTraceHelper.CreateFileStream (outputNameFct);

  Ptr<Socket> sockets[num_hosts*2];
	std::stringstream sender;
	std::stringstream receiver;
 for (uint32_t i = 0 ; i < num_hosts ; i++){

 	sender << "s" << i;
 	receiver << "d" << i;

 	sockets[i] = installBulkSend(GetNode(sender.str()), GetNode(receiver.str()), hostToPort[receiver.str()][0],
 			BytesFromRate(DataRate(linkBandiwdth),10), 1, flowsCompletionTime, 0);

 	//opposite direction
 	if (both_directions == true){
 		installBulkSend(GetNode(receiver.str()), GetNode(sender.str()), hostToPort[sender.str()][0],
 			BytesFromRate(DataRate(linkBandiwdth),10), 1, flowsCompletionTime, 0);}

 	//Enable pcaps
 	sender << "->r0";
 	receiver << "->r" << (num_hosts +1);
        csma.EnablePcap(outputNameFct, links[sender.str()].Get(0), bool(1));
        csma.EnablePcap(outputNameFct, links[receiver.str()].Get(0), bool(1));


 	sender.str("");
 	receiver.str("");
 }


//start a lot of small flows
//   Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable> ();

//   for (uint32_t i = 0 ; i < num_hosts ; i++){

//   	for (uint32_t j = 0; j < 1; j++){

//   	sender << "s" << i;
//   	receiver << "d" << i;

//   	sockets[i] = installBulkSend(GetNode(sender.str()), GetNode(receiver.str()), hostToPort[receiver.str()][random_generator->GetInteger(0,sinks-1)],
//   			BytesFromRate(DataRate("150Kbps"),1), 1, flowsCompletionTime, 0);

// //  	opposite direction
//   	if (both_directions == true){
//   		installBulkSend(GetNode(receiver.str()), GetNode(sender.str()), hostToPort[sender.str()][random_generator->GetInteger(0,sinks-1)],
//   			BytesFromRate(DataRate("150Kbps"),1), 1, flowsCompletionTime, 0);}

//   	//Enable pcaps
//   	sender << "->r0";
//   	receiver << "->r" << (num_hosts +1);
//     csma.EnablePcap(outputNameFct, links[sender.str()].Get(0), bool(1));
//     csma.EnablePcap(outputNameFct, links[receiver.str()].Get(0), bool(1));


//   	sender.str("");
//   	receiver.str("");
//   	}
//   }
//	sockets[0] = installBulkSend(GetNode("s0"), GetNode("d0"), hostToPort["d0"][0],
//			1000000, 1, flowsCompletionTime, 0);

//	csma.EnablePcap(outputNameFct, links["s0->r0"].Get(0), bool(1));

	//Queue size
//	 Ptr<Queue> queue_s0 = DynamicCast<PointToPointNetDevice>(links["s0->r0"].Get(0))->GetQueue();
//
//		for (uint32_t i = 0; i < num_hosts; i++){
//
//					host_name << "s" << i;
//					router_name << "d" << i;
//
//					NetDeviceContainer interface = links[host_name.str() + "->" + "r0"];
//
//					//Get Device queues TODO: if we add RED queues.... this will not work....
//					Ptr<Queue> queue_rx = DynamicCast<PointToPointNetDevice>(interface.Get(0))->GetQueue();
//					Ptr<Queue> queue_tx = DynamicCast<PointToPointNetDevice>(interface.Get(1))->GetQueue();
//
//					MeasureInterfaceLoad(queue_rx, 0, 1, host_name.str() + "_rx");
//					MeasureInterfaceLoad(queue_tx, 0, 1, host_name.str() + "_tx");
//
//					interface = links[router_name.str() + "->" + "r9"];
//
//					//Get Device queues TODO: if we add RED queues.... this will not work....
//					queue_rx = DynamicCast<PointToPointNetDevice>(interface.Get(0))->GetQueue();
//					queue_tx = DynamicCast<PointToPointNetDevice>(interface.Get(1))->GetQueue();
//
//					MeasureInterfaceLoad(queue_rx, 0, 1, router_name.str() + "_rx");
//					MeasureInterfaceLoad(queue_tx, 0, 1, router_name.str() + "_tx");
//
//					host_name.str("");
//					router_name.str("");
//		}



//	 MeasureInOutLoad(links, uint32_t(k) , 1);

//	 LinkBandwidth(queue_s0, 0, 1);


//	 PrintQueueSize(queue_s0);

//
//	sockets[1] = installBulkSend(GetNode("s1"), GetNode("d1"), hostToPort["d1"][0],
//			BytesFromRate(DataRate(linkBandiwdth),10), 1, flowsCompletionTime, 0);
//
//	sockets[2] = installBulkSend(GetNode("s1"), GetNode("d2"), hostToPort["d2"][0],
//			BytesFromRate(DataRate(linkBandiwdth),10), 1, flowsCompletionTime, 0);


	/////////
	//PCAPS
	///////////

  //////////////////
  //TRACES
  ///////////////////

////
	Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (outputNameRoot+ "-" +  simulationName+"_0.cwnd");
//	Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream (outputNameRoot+ "-" +  simulationName+"_1.cwnd");
	sockets[0]->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));
//	sockets[1]->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream1));


//	links["s0->r0"]

//  csma.EnablePcap(outputNameFct, links["s0->r0"].Get(0), bool(1));

	errorLink = "d0->r2";
  //Install Error model
  if (errorRate > 0){
		Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
		em->SetAttribute ("ErrorRate", DoubleValue (errorRate));
		em->SetAttribute ("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

//		links[errorLink].Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
		links[errorLink].Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

		PcapHelper pcapHelper;
		Ptr<PcapFileWrapper> drop_pcap = pcapHelper.CreateFile (outputNameRoot+"-drops.pcap", std::ios::out, PcapHelper::DLT_PPP);

		Ptr<OutputStreamWrapper> drop_ascii = asciiTraceHelper.CreateFileStream (outputNameRoot+"-drops");

//		links[errorLink].Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropAscii, drop_ascii));
		links[errorLink].Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropAscii, drop_ascii));

//		links[errorLink].Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropPcap, drop_pcap));
		links[errorLink].Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropPcap, drop_pcap));
  }



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

	AnimationInterface anim(outputNameRoot+".anim");
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

  ////////////////////
  //Flow Monitor
  ////////////////////

  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor>  flowMonitor;
  Ptr<Ipv4FlowClassifier> classifier;
  std::map<FlowId, FlowMonitor::FlowStats> stats;

  if (monitor)
  {
  	flowMonitor = flowHelper.InstallAll ();
  }

  Simulator::Schedule(Seconds(1), &printNow, 1);

  Simulator::Stop (Seconds (100));

  Simulator::Run ();



  if (monitor){
		classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
		stats = flowMonitor->GetFlowStats ();

		//File where we write FCT
		Ptr<OutputStreamWrapper> fct = asciiTraceHelper.CreateFileStream (outputNameRoot + ".monitor");

		for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i){

			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
			double last_t = i->second.timeLastRxPacket.GetSeconds();
			double first_t = i->second.timeFirstTxPacket.GetSeconds();
			double duration = (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());

			*fct->GetStream() << duration << " " << first_t << " " << last_t <<  "\n";

			std::cout << "Flow " << i->first << " (" << (GetNodeName(ipToNode[ipv4AddressToString(t.sourceAddress)])) << "-> " << GetNodeName(ipToNode[ipv4AddressToString(t.destinationAddress)]) << ")\n";

//			std::cout << "Tx Bytes:   " << i->second.txBytes << "\n";
//			std::cout << "Rx Bytes:   " << i->second.rxBytes << "\n";
			std::cout << "Flow duration:   " << duration << " " << first_t << " " << last_t <<  "\n";

			std::cout << "Throughput: " << i->second.rxBytes * 8.0 / duration /1024/1024  << " Mbps\n";}


//  flowHelper.SerializeToXmlFile (std::string("outputs/") + "flowmonitor", true, true);

  }

  Simulator::Destroy ();


  return 0;
}

