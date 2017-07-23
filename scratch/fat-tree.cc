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

std::string fileNameRoot = "fat-tree";    // base name for trace files, etc
std::string outputNameRoot = "outputs/";
std::string outputNameFct = "";

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

const char * file_name = g_log.Name();
std::string script_name = file_name;

int counter = 0;



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

	uint32_t minRTO = rtt*1.5;

  int64_t flowlet_gap = rtt;
  double flowlet_gap_scaling = 1;


  bool animation = false;
  bool monitor = false;
  bool debug = false;

  //error model
  std::string errorLink = "r_0_a0->r_c0";
  double errorRate = 0;

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
  cmd.AddValue("FlowletGap", "Inter-arrival packet time for flowlet expiration", flowlet_gap);
  cmd.AddValue("K", "Fat tree size", k);
  cmd.AddValue("RunStep", "Random generator starts at", runStep);
  cmd.AddValue("TrafficPattern","stride or distribution", trafficPattern);

  cmd.Parse (argc, argv);

	//Change that if i want to get different random values each run otherwise i will always get the same.
	RngSeedManager::SetRun (runStep);   // Changes run number from default of 1 to 7

	if (debug){
//		LogComponentEnable("Ipv4GlobalRouting", LOG_DEBUG);
		//LogComponentEnable("Ipv4GlobalRouting", LOG_ERROR);
		LogComponentEnable("fat-tree", LOG_ERROR);
		LogComponentEnable("utils", LOG_DEBUG);
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

	rtt = 12*delay + (6*Seconds(packetDelay).GetMicroSeconds()) + (6*Seconds(small_packetDelay).GetMicroSeconds());

	int small_rtt = 12*delay + (12*Seconds(packetDelay).GetMicroSeconds());

	minRTO = rtt*2;

  flowlet_gap = rtt*flowlet_gap_scaling; //milliseconds


  NS_LOG_UNCOND("rtt: " << rtt << " minRTO: " << minRTO << " flowlet_gap: " << flowlet_gap
   		<< " delay: " << delay << " packetDelay: " << Seconds(packetDelay).GetMicroSeconds()
 			<< "small_rtt: "<< small_rtt <<" Flowlet gap:" << flowlet_gap << " Microseconds");

  //TCP

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
//   Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1500000));
//   Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1500000));
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

  //Compute Fat Tree Devices

  int num_pods = k;
  int num_hosts = k*k*k/4;

  int num_edges = k*k/2;
  int num_agg = k*k/2;
  int num_cores = k*k/4;

  int hosts_per_edge = k/2;
  int hosts_per_pod = k*k/4;
  int routers_per_layer = k/2;

  NS_LOG_DEBUG("Num_pods: " << k << " Num Hosts: " << num_hosts << " Num Edges: "
  		<< num_edges << " Num agg: " << num_agg << " Num Cores: " << num_cores
			<< " Hosts per pod: " << hosts_per_pod << "  Routers per layer: " << routers_per_layer);


  //Hosts
  NodeContainer hosts;
  hosts.Create(num_hosts);

  //Give names to hosts using names class
  int pod = 0; int inpod_num = 0; int host_count = 0; int router_count;

  for (NodeContainer::Iterator host = hosts.Begin(); host != hosts.End(); host++ ){

  		inpod_num =  host_count % hosts_per_pod;
  		pod = host_count / hosts_per_pod;

  		std::stringstream host_name;
  		host_name << "h_" << pod << "_" << inpod_num;
  		NS_LOG_DEBUG(host_name.str());

  		Names::Add(host_name.str(), (*host));

  		host_count++;
  }


  //Edge routers
  NodeContainer edgeRouters;
	edgeRouters.Create (num_edges);

  //Give names to hosts using names class
  pod = 0; inpod_num = 0; router_count = 0;
  for (NodeContainer::Iterator router = edgeRouters.Begin(); router != edgeRouters.End(); router++ ){

  		inpod_num =  router_count % routers_per_layer;
  		pod = router_count / routers_per_layer;

  		std::stringstream router_name;
  		router_name << "r_" << pod << "_e" << inpod_num;
  	  NS_LOG_DEBUG(router_name.str());

  		Names::Add(router_name.str(), (*router));

  		router_count++;
  }

  //Agg routers
  NodeContainer aggRouters;
	aggRouters.Create (num_agg);

  //Give names to hosts using names class
  pod = 0; inpod_num = 0; router_count = 0;
  for (NodeContainer::Iterator router = aggRouters.Begin(); router != aggRouters.End(); router++ ){

  		inpod_num =  router_count % routers_per_layer;
  		pod = router_count / routers_per_layer;

  		std::stringstream router_name;
  		router_name << "r_" << pod << "_a" << inpod_num;
  	  NS_LOG_DEBUG(router_name.str());

  		Names::Add(router_name.str(), (*router));

  		router_count++;
  }


  //Core routers
  NodeContainer coreRouters;
	coreRouters.Create (num_cores);

  //Give names to hosts using names class
  router_count = 0;
  for (NodeContainer::Iterator router = coreRouters.Begin(); router != coreRouters.End(); router++ ){

  		std::stringstream router_name;
  		router_name << "r_c"  <<router_count;
  	  NS_LOG_DEBUG(router_name.str());

  		Names::Add(router_name.str(), (*router));

  		router_count++;
  }


  //Install net devices between nodes (so add links) would be good to save them somewhere I could maybe use the namesystem or map
  //Install internet stack to nodes, very easy

  std::unordered_map<std::string, NetDeviceContainer> links;


  //Add links between fat tree nodes

  //Create PODs

  for (int pod_i=0; pod_i < num_pods; pod_i ++)
  {

  	//Connect edges with hosts
  	for (int edge_i=0; edge_i < routers_per_layer; edge_i++)
  	{
  		std::stringstream edge_name;
  		edge_name << "r_" << pod_i << "_e" << edge_i;

  		for (int host_i=0; host_i < hosts_per_edge; host_i++){

    		std::stringstream host_name;
    		host_name << "h_" << pod_i << "_" << (host_i + (hosts_per_edge*edge_i));

    		NS_LOG_DEBUG("Adding link between " << host_name.str() << " and " << edge_name.str());
  		  links[host_name.str()+"->"+edge_name.str()] = csma.Install (NodeContainer(GetNode(host_name.str()),GetNode(edge_name.str())));
  		}

  		//connect edge with all the agg

  		for (int agg_i = 0; agg_i < routers_per_layer; agg_i++){
    		std::stringstream agg_name;
    		agg_name << "r_" << pod_i << "_a" << agg_i;

    		NS_LOG_DEBUG("Adding link between " << edge_name.str() << " and " << agg_name.str());
  		  links[edge_name.str()+"->"+agg_name.str()] = csma.Install (NodeContainer(GetNode(edge_name.str()),GetNode(agg_name.str())));

  		}

  	}

  	//Connect agg with core layer

		for (int agg_i = 0; agg_i < routers_per_layer; agg_i++){
  		std::stringstream agg_name;
  		agg_name << "r_" << pod_i << "_a" << agg_i;

  		//Connect to every k/2 cores
  		for (int core_i=(agg_i* (k/2)); core_i < ((agg_i+1)*(k/2)) ; core_i++){
    		std::stringstream core_name;
    		core_name << "r_c" << core_i;

    		NS_LOG_DEBUG("Adding link between " << agg_name.str() << " and " << core_name.str());
  		  links[agg_name.str()+"->"+core_name.str()] = csma.Install (NodeContainer(GetNode(agg_name.str()),GetNode(core_name.str())));
  		}

  	}
  }

  // Install Internet stack and assing ips
  InternetStackHelper stack;
  stack.Install (hosts);
  stack.Install (edgeRouters);
  stack.Install (aggRouters);
  stack.Install (coreRouters);


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

//
//  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("node1_tables", std::ios::out);
//  Ipv4GlobalRoutingHelper::PrintRoutingTableAt(Seconds(1), nodes.Get(1), routingStream);
//
//

//START TRAFFIC

  //Special variables for interval tests.
  double recordStartTime = 0;
  double recordingTime = 1;
  uint64_t recordedFlowsCounter = 0;

//  //Prepare sink app
  std::unordered_map <std::string, std::vector<uint16_t>> hostToPort = installSinks(hosts, 2000, 1000 , protocol);

  Ptr<OutputStreamWrapper> flowsCompletionTime = asciiTraceHelper.CreateFileStream (outputNameFct);

  //NodeContainer tmp_hosts;
  //tmp_hosts.Add("h_0_0");
//

  if (trafficPattern == "distribution"){
  	sendFromDistribution(hosts, hostToPort, k , flowsCompletionTime, sizeDistributionFile,runStep,
  			interArrivalFlowsTime, intraPodProb, interPodProb, simulationTime, &recordStartTime, recordingTime, &recordedFlowsCounter);
  }
  else if( trafficPattern == "stride"){
	  startStride(hosts, hostToPort, BytesFromRate(DataRate(linkBandiwdth), 5), 1, 4 ,flowsCompletionTime);
  }

  //Fill a structure with linkName->previousCounter
  std::unordered_map<std::string, double> linkToPreviousLoad;

  for(auto it = links.begin(); it != links.end(); it++){
  	std::size_t found = it->first.find('h');
  	if  (found != std::string::npos){
  		linkToPreviousLoad[it->first + "_rx"] = 0;
  		linkToPreviousLoad[it->first + "_tx"] = 0;

  	}
  }

  network_load load_data;
  load_data.stopThreshold = 0.7;
  load_data.startTime = &recordStartTime;

  MeasureInOutLoad(links, linkToPreviousLoad, uint32_t(k) ,linkBandiwdth, 0.25, load_data);


  //////////////////
  //TRACES
  ///////////////////
//  Ptr<Socket> sock = installSimpleSend(GetNode("h_0_0"), GetNode("h_1_0"), randomFromVector(hostToPort["h_1_0"]), dataRate, num_packets,protocol);
//  Ptr<Socket> sock2 = installSimpleSend(GetNode("h_0_1"), GetNode("h_1_0"), randomFromVector(hostToPort["h_1_0"]), dataRate, num_packets,protocol);
//  installSimpleSend(GetNode("h_3_1"), GetNode("h_2_0"), randomFromVector(hostToPort["h_2_0"]), dataRate, num_packets,protocol);



//   Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (outputNameRoot+".cwnd");
//   Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream (outputNameRoot+"2.cwnd");
//  if (protocol == "TCP"){
//  	sock->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));
//  	sock2->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream2));
//  }

//
//  PcapHelper pcapHelper;
//  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile (outputNameRoot+".pcap", std::ios::out, PcapHelper::DLT_PPP);
//
//
//  n0n1.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));
//
//
//  n0n1.Get (0)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&TxDrop, "PhyTxDrop"));
//  n0n1.Get (0)->TraceConnectWithoutContext ("MacTxDrop", MakeBoundCallback (&TxDrop, "MacTxDrop" ));

//  	links["h_0_0->r_0_e0"].Get (0)->TraceConnectWithoutContext ("MacTx", MakeBoundCallback (&TxDrop, "MacTx h_0_0"));
  //links["h_0_1->r_0_e0"].Get (0)->TraceConnectWithoutContext ("MacTx", MakeBoundCallback (&TxDrop, "MacTx h_0_1"));

//
//  csma.EnablePcap(outputNameFct, links["r_0_a0->r_c0"].Get(0), bool(1));
//  csma.EnablePcap(outputNameFct, links["r_0_a0->r_c1"].Get(0), bool(1));
//  csma.EnablePcap(outputNameFct, links["r_0_a1->r_c2"].Get(0), bool(1));

//  	csma.EnablePcapAll(outputNameRoot, true);

//  csma.EnablePcap(outputNameFct, links["h_0_0->r_0_e0"].Get(0), bool(1));
//  csma.EnablePcap(outputNameFct, links["h_3_1->r_3_e0"].Get(0), bool(1));

//  csma.EnablePcap(outputNameFct, links["h_0_1->r_0_e0"].Get(0), bool(1));
//  csma.EnablePcap(outputNameFct, links["h_0_2->r_0_e1"].Get(0), bool(1));
//  csma.EnablePcap(outputNameFct, links["h_0_3->r_0_e1"].Get(0), bool(1));

  //Install Error model
  if (errorRate > 0){
		Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
		em->SetAttribute ("ErrorRate", DoubleValue (errorRate));
		em->SetAttribute ("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

		links[errorLink].Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
		links[errorLink].Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

		PcapHelper pcapHelper;
		Ptr<PcapFileWrapper> drop_pcap = pcapHelper.CreateFile (outputNameRoot+"-drops.pcap", std::ios::out, PcapHelper::DLT_PPP);

		Ptr<OutputStreamWrapper> drop_ascii = asciiTraceHelper.CreateFileStream (outputNameRoot+"-drops");

		links[errorLink].Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropAscii, drop_ascii));
		links[errorLink].Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropAscii, drop_ascii));

		links[errorLink].Get (0)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropPcap, drop_pcap));
		links[errorLink].Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDropPcap, drop_pcap));

  }



  //Allocate nodes in a fat tree shape
//	allocateNodesFatTree(k);
//	AnimationInterface anim(outputNameRoot+".anim");
//  if (animation){
//		//Animation
//		//AnimationInterface anim(outputNameRoot+".anim");
//		anim.SetMaxPktsPerTraceFile(10000000);
//
//	//  setting colors
//		for (uint32_t i = 0; i < hosts.GetN(); i++){
//					Ptr<Node> host = hosts.Get(i);
//					anim.UpdateNodeColor(host, 0, 0, 255);
//					anim.UpdateNodeSize(host->GetId(), 0.5, 0.5);
//
//		}
//		for (uint32_t i = 0; i < edgeRouters.GetN(); i++)
//		{
//					anim.UpdateNodeColor(edgeRouters.Get(i), 0, 200, 0);
//					anim.UpdateNodeColor(aggRouters.Get(i), 0, 200, 0);
//		}
//
//		for (uint32_t i = 0; i < coreRouters.GetN(); i++)
//					anim.UpdateNodeColor(coreRouters.Get(i), 255, 0, 0);
//
//		anim.EnablePacketMetadata(true);
//  }
//  else{
//  	anim.SetStopTime(Seconds(0));
//  }

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

//  Simulator::Schedule(Seconds(1), &printNow, 1);

  Simulator::Stop (Seconds (500));
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

