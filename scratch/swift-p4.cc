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
#include <ctime>
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

// TOPOLOGY
//+---+                                                 +---+
//|s1 |                                           +-----+d1 |
//+---+----+                                      |     +---+
//         |                                      |
//  .      |    +------+              +------+    |       .
//  .      +----+      |              |      +----+       .
//  .           |  sw1 +--------------+  sw2 |            .
//  .           |      |              |      +---+        .
//  .      +----+------+              +------+   |        .
//         |                                     |
//+---+    |                                     |      +---+
//|sN +----+                                     +------+dN |
//+---+                                                 +---+



using namespace ns3;

std::string fileNameRoot = "swift-p4";    // base name for trace files, etc
std::string outputNameRoot = "outputs/";

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

const char * file_name = g_log.Name();
std::string script_name = file_name;


int
main (int argc, char *argv[])
{

	//INITIALIZATION

	//Set simulator's time resolution (click)
	Time::SetResolution (Time::NS);

  //Fat tree parameters
  DataRate networkBandwidth("100Gbps");
  DataRate sendersBandwidth("4Gbps");
  DataRate receiversBandwidth("4Gbps");


  //Command line arguments
  std::string ecmpMode = "ECMP_NONE";
  std::string simulationName = "";
  std::string outputFolder = "";

  uint16_t queue_size = 100;
  uint16_t num_hosts = 100;

  uint64_t runStep = 1;

  std::string sizeDistributionFile = "distributions/default.txt";
  std::string trafficPattern = "distribution";

  uint64_t network_delay = 1;

  double error_rate = 0.001;


  bool debug = false;


  CommandLine cmd;

  //Misc
  cmd.AddValue("Debug", "" , debug);

  //Naming
  cmd.AddValue("SimulationName", "Name of the experiment: " , simulationName);
  cmd.AddValue("OutputFolder", "Set it to something if you want to store the simulation output "
  		"in a particular folder inside outputs", outputFolder);


  //General
  cmd.AddValue ("EcmpMode", "EcmpMode: (0 none, 1 random, 2 flow, 3 Round_Robin)", ecmpMode);

  //Links properties
	cmd.AddValue("LinkBandwidth", "Bandwidth of link, used in multiple experiments", networkBandwidth);
	cmd.AddValue("NetworkDelay", "Added delay between nodes", network_delay);
  cmd.AddValue("ErrorRate", "Rate of drop per packet", error_rate);


	//Experiment
	cmd.AddValue("SizeDistribution", "File with the flows size distribution", sizeDistributionFile);
  cmd.AddValue("QueueSize", "Interfaces Queue length", queue_size);
  cmd.AddValue("RunStep", "Random generator starts at", runStep);
  cmd.AddValue("NumHosts", "Number of hosts in each side", num_hosts);

  cmd.Parse (argc, argv);

	//Change that if i want to get different random values each run otherwise i will always get the same.
	RngSeedManager::SetRun (runStep);   // Changes run number from default of 1 to 7

	if (debug){
//		LogComponentEnable("Ipv4GlobalRouting", LOG_DEBUG);
		//LogComponentEnable("Ipv4GlobalRouting", LOG_ERROR);
		LogComponentEnable("swift-p4", LOG_DEBUG);
		LogComponentEnable("utils", LOG_DEBUG);
		LogComponentEnable("traffic-generation", LOG_DEBUG);
		//LogComponentEnable("custom-bulk-app", LOG_DEBUG);
		//LogComponentEnable("PacketSink", LOG_ALL);
		//LogComponentEnable("TcpSocketBase", LOG_ALL);

	}

  //Update root name
	std::ostringstream run;
	run << runStep;

	//Timeout calculations (in milliseconds)
	uint16_t rtt = 200;

  outputNameRoot = outputNameRoot + outputFolder + "/" + fileNameRoot + "-" + simulationName + "_" + std::string(run.str());

  //TCP

  //GLOBAL CONFIGURATION

   //Routing
   Config::SetDefault("ns3::Ipv4GlobalRouting::EcmpMode", StringValue(ecmpMode));
   Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));


 	//Tcp Socket (general socket conf)
   Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(150000000));
   Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(150000000));
 	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446)); //MTU
 	Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(4294967295));
 	Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(1));

 	//Can be much slower than my rtt because packet size of syn is 60bytes
 	Config::SetDefault ("ns3::TcpSocket::ConnTimeout",TimeValue(MilliSeconds(rtt))); // connection retransmission timeout
 	Config::SetDefault ("ns3::TcpSocket::ConnCount",UintegerValue(10)); //retrnamissions during connection
 	Config::SetDefault ("ns3::TcpSocket::DataRetries", UintegerValue (10)); //retranmissions
 	Config::SetDefault ("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(rtt/50)));
// 	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(2));
 	Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue(true)); //disable nagle's algorithm
 	Config::SetDefault ("ns3::TcpSocket::PersistTimeout", TimeValue(NanoSeconds(6000000000))); //persist timeout to porbe for rx window

 	//Tcp Socket Base: provides connection orientation, sliding window, flow control; congestion control is delegated to the subclasses (i.e new reno)

 	Config::SetDefault ("ns3::TcpSocketBase::MaxSegLifetime",DoubleValue(10));
 //	Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue(true)); //enable sack
 	Config::SetDefault ("ns3::TcpSocketBase::MinRto",TimeValue(MilliSeconds(rtt))); //min RTO value that can be set
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

  //Define acsii helper
  AsciiTraceHelper asciiTraceHelper;


  //Define Interfaces

  //Define point to point
  PointToPointHelper p2p;

//   create point-to-point link from A to R
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (networkBandwidth)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MicroSeconds(network_delay)));
  p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));

  p2p.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"));
//  p2p.SetQueue("ns3::DropTailQueue", "Mode", EnumValue(DropTailQueue::QUEUE_MODE_BYTES));
//  p2p.SetQueue("ns3::DropTailQueue", "MaxBytes", UintegerValue(queue_size*1500));
  p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));

  //Compute Fat Tree Devices


  int num_senders = num_hosts;
  int num_receivers = num_hosts;

  //Compute bandwidth per hosts.
  uint64_t net_bps = networkBandwidth.GetBitRate();
  uint64_t hosts_bps = net_bps / num_hosts;
  sendersBandwidth = DataRate(hosts_bps);
  receiversBandwidth =  DataRate(hosts_bps);

  //Senders
  NodeContainer senders;
  senders.Create(num_senders);

  //Receivers
  NodeContainer receivers;
  receivers.Create(num_receivers);

  //Switches
  Ptr<Node> sw1 = CreateObject<Node>();
  Ptr<Node> sw2 = CreateObject<Node>();

  //Add two switches
  Names::Add("sw1", sw1);
  Names::Add("sw2", sw2);

  //Install net devices between nodes (so add links) would be good to save them somewhere I could maybe use the namesystem or map
  //Install internet stack to nodes, very easy
  std::unordered_map<std::string, NetDeviceContainer> links;

  //SETTING LINKS: delay and bandwidth.

  //sw1 -> sw2
  //Interconnect middle switches : sw1 -> sw2
	NS_LOG_DEBUG("Adding link between: " << GetNodeName(sw1) << " -> "  << GetNodeName(sw2));
  links[GetNodeName(sw1)+"->"+GetNodeName(sw2)] = p2p.Install (NodeContainer(sw1, sw2));
  //Set delay and bandwdith
  links[GetNodeName(sw1)+"->"+GetNodeName(sw2)].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue (MicroSeconds(network_delay)));
  links[GetNodeName(sw1)+"->"+GetNodeName(sw2)].Get(0)->SetAttribute("DataRate", DataRateValue(networkBandwidth));


  //Setting up min and max latencies.
  Time senders_min_latency = MicroSeconds(1);
  Time senders_current_latency = senders_min_latency;
  Time senders_min_ceil = senders_min_latency * 10;
  Time senders_max_latency = Seconds(10);

  std::unordered_map<uint64_t, std::vector<Ptr<Node>>> senders_latency_to_node;

  //Give names to hosts using names class and connect them to the respective switch.
  //Senders
	int host_count = 0;
  for (NodeContainer::Iterator host = senders.Begin(); host != senders.End(); host++ ){

  		std::stringstream host_name;
  		host_name << "s_" << host_count;
  		NS_LOG_DEBUG(host_name.str());
  		Names::Add(host_name.str(), (*host));

  		//add link host-> sw1
  		NS_LOG_DEBUG("Adding link between: " << host_name.str() << " -> "  << GetNodeName(sw1));
		  links[host_name.str()+"->"+GetNodeName(sw1)] = p2p.Install (NodeContainer(*host, sw1));

		  //set link delay
		  //double sender_delay = random_variable->GetValue(5, 25);
		  Time delay = senders_current_latency;

		  links[host_name.str()+"->"+GetNodeName(sw1)].Get(0)->SetAttribute("DataRate", DataRateValue(sendersBandwidth));
		  links[host_name.str()+"->"+GetNodeName(sw1)].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue (delay));

  		NS_LOG_DEBUG("Link " << host_name.str() << "->" << GetNodeName(sw1) << " delay: " << delay << " bandwidth: " << sendersBandwidth);

		  //Store this node in the latency to node map
		  if (senders_latency_to_node.count(delay.GetInteger()) > 0 )
		  {
		  	senders_latency_to_node[delay.GetInteger()].push_back(*host);
		  }
		  else
		  {
		  	senders_latency_to_node[delay.GetInteger()] = std::vector<Ptr<Node>>();
		  	senders_latency_to_node[delay.GetInteger()].push_back(*host);
		  }


		  //Update delay
  		if (senders_current_latency >= senders_max_latency)
  		{
  			senders_current_latency = senders_min_latency;
  			senders_min_ceil = senders_current_latency * 10;
  		}
  		else
  		{
  			if (senders_current_latency != senders_min_ceil)
  			{
    			senders_current_latency += senders_min_ceil/10;
    		}
    		else
    		{
    			senders_current_latency += senders_min_ceil;
    			senders_min_ceil = senders_min_ceil * 10;
    		}
  		}
   		host_count++;
  }

  //Receivers

  //Setting up min and max latencies.
  Time receivers_min_latency = MicroSeconds(1);
  Time receivers_current_latency = receivers_min_latency;
  Time receivers_min_ceil = receivers_min_latency * 10;
  Time receivers_max_latency = Seconds(10);
  std::unordered_map<uint64_t, std::vector<Ptr<Node>>> receivers_latency_to_node;


	host_count = 0;
  for (NodeContainer::Iterator host = receivers.Begin(); host != receivers.End(); host++ ){

  		std::stringstream host_name;
  		host_name << "d_" << host_count;
  		NS_LOG_DEBUG(host_name.str());
  		Names::Add(host_name.str(), (*host));

  		//Add link sw2->host
  		NS_LOG_DEBUG("Adding link between: " << GetNodeName(sw2) << " -> " << host_name.str());
		  links[GetNodeName(sw2)+"->"+host_name.str()] = p2p.Install (NodeContainer(sw2, *host));

		  //set link delay
//		  double receiver_delay = random_variable->GetValue(5, 10);
		  Time delay = receivers_current_latency;

		  links[GetNodeName(sw2)+"->"+host_name.str()].Get(0)->SetAttribute("DataRate", DataRateValue(receiversBandwidth));
		  links[GetNodeName(sw2)+"->"+host_name.str()].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue (MilliSeconds(delay)));
  		NS_LOG_DEBUG("Link " << GetNodeName(sw2)<<"->"<<host_name.str() << " delay: " << delay << " bandwidth: " << sendersBandwidth);

		  //Store this node in the latency to node map
		  if (receivers_latency_to_node.count(delay.GetInteger()) > 0 )
		  {
		  	receivers_latency_to_node[delay.GetInteger()].push_back(*host);
		  }
		  else
		  {
		  	receivers_latency_to_node[delay.GetInteger()] = std::vector<Ptr<Node>>();
		  	receivers_latency_to_node[delay.GetInteger()].push_back(*host);
		  }

		  //Update delay
  		if (receivers_current_latency >= receivers_max_latency)
  		{
  			receivers_current_latency = receivers_min_latency;
  			receivers_min_ceil = receivers_current_latency * 10;
  		}
  		else
  		{
  			if (receivers_current_latency != receivers_min_ceil)
  			{
    			receivers_current_latency += receivers_min_ceil/10;
    		}
    		else
    		{
    			receivers_current_latency += receivers_min_ceil;
    			receivers_min_ceil = receivers_min_ceil * 10;
    		}
  		}
  		host_count++;
  }


  // Install Internet stack and assing ips
  InternetStackHelper stack;
  stack.Install (senders);
  stack.Install (receivers);
  stack.Install(sw1);
  stack.Install(sw2);


  //Assign IPS
  //Uninstall FIFO queue //  //uninstall qdiscs
  TrafficControlHelper tch;

  Ipv4AddressHelper address("10.0.1.0", "255.255.255.0");
  for (auto it : links){
  	address.Assign(it.second);
  	address.NewNetwork();
  	tch.Uninstall(it.second);
  }

  //Create a ip to node mapping
  std::unordered_map<std::string, Ptr<Node>> ipToNode;

  for (uint32_t host_i = 0; host_i < senders.GetN(); host_i++){
  	Ptr<Node> host = senders.Get(host_i);
  	ipToNode[ipv4AddressToString(GetNodeIp(host))] = host;
  }
  for (uint32_t host_i = 0; host_i < receivers.GetN(); host_i++){
  	Ptr<Node> host = receivers.Get(host_i);
  	ipToNode[ipv4AddressToString(GetNodeIp(host))] = host;
  }
  //Store in a file ip -> node name
  Ptr<OutputStreamWrapper> ipToName_file = asciiTraceHelper.CreateFileStream (outputNameRoot+"-ipToName");
  for (auto it = ipToNode.begin();  it != ipToNode.end(); it++){
  	*(ipToName_file->GetStream()) << it->first << " " << GetNodeName(it->second) << "\n";
  }
  ipToName_file->GetStream()->flush();

  //Add Error Model
	if (error_rate > 0){

		//Link where we add the error rate..
		std::string errorLink = GetNodeName(sw1)+"->"+GetNodeName(sw2);

		//Create the error model
		Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
		em->SetAttribute ("ErrorRate", DoubleValue (error_rate));
		em->SetAttribute ("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

//		links[errorLink].Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
		links[errorLink].Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

//		PcapHelper pcapHelper;
//		Ptr<PcapFileWrapper> drop_pcap = pcapHelper.CreateFile (outputNameRoot+"-drops.pcap", std::ios::out, PcapHelper::DLT_PPP);
//
		//Ptr<OutputStreamWrapper> drop_ascii = asciiTraceHelper.CreateFileStream (outputNameRoot+".drops");

		//links[errorLink].Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropAscii, drop_ascii));
		//links[errorLink].Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropAscii, drop_ascii));
//
//		links[errorLink].Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropPcap, drop_pcap));
//		links[errorLink].Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropPcap, drop_pcap));

	}


  //Populate tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //START TRAFFIC

  //Install Traffic sinks at receivers


  //  links["s_40->sw1"].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue (MicroSeconds(1)));
  //  links["sw2->d_31"].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue (MicroSeconds(1)));

  TimeValue time_test;
  links["sw2->d_31"].Get(0)->GetChannel()->GetAttribute("Delay", time_test);
  NS_LOG_UNCOND("we got a delay of: " << time_test.Get().GetSeconds());



  std::unordered_map <std::string, std::vector<uint16_t>> hostToPort = installSinks(receivers, 10, 0 , "TCP");

  Ptr<Socket> sock = installSimpleSend(GetNode("s_40"), GetNode("d_31"), randomFromVector(hostToPort["d_31"]), DataRate("100Mbps"), 10, "TCP");


//  sendSwiftTraffic(senders_latency_to_node, receivers_latency_to_node, hostToPort, "only_rtt.txt", "",runStep ,1, 1);

  //Senders function

  //////////////////
  //TRACES

  ///////////////////
  p2p.EnablePcap(fileNameRoot, links[GetNodeName(sw1)+"->"+GetNodeName(sw2)].Get(0), bool(1));
//  p2p.EnablePcap(fileNameRoot, links[GetNodeName(sw2)+"->"+"d_31"].Get(0), bool(1));

  Simulator::Stop (Seconds (500));
  Simulator::Run ();
  Simulator::Destroy ();


    return 0;
  }
