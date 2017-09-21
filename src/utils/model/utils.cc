/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "utils.h"

NS_LOG_COMPONENT_DEFINE ("utils");



namespace ns3 {

/* ... */

Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable> ();



Ipv4Address
GetNodeIp(std::string node_name)
{

	Ptr<Node> node = GetNode(node_name);

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

//* Returns the amount of bytes needed to send at dataRate during time seconds.
uint64_t BytesFromRate(DataRate dataRate, double time){

		double bytes = ((double)dataRate.GetBitRate()/8) * time;
		//NS_LOG_DEBUG("Bytes to send: " << bytes);
		return bytes;
}

uint64_t hash_string(std::string message){
  Hasher hasher;
  hasher.clear();
  return hasher.GetHash64(message);

}

std::vector< std::pair<double,uint64_t>> GetDistribution(std::string distributionFile) {

  std::vector< std::pair<double,uint64_t>> cumulativeDistribution;
  std::ifstream infile(distributionFile);

  NS_ASSERT_MSG(infile, "Please provide a valid file for reading the flow size distribution!");
  double cumulativeProbability;
  double size;
  while (infile >> size >> cumulativeProbability)
  {
    cumulativeDistribution.push_back(std::make_pair(cumulativeProbability, int(size)));
  }

//  for(uint32_t i = 0; i < cumulativeDistribution.size(); i++)
//  {
//    NS_LOG_FUNCTION(cumulativeDistribution[i].first << " " << cumulativeDistribution[i].second);
//  }

  return cumulativeDistribution;
}

//*
//Get size from cumulative traffic distribution
//

uint64_t GetFlowSizeFromDistribution(std::vector< std::pair<double,uint64_t>> distribution, double uniformSample){

  NS_ASSERT_MSG(uniformSample <= 1.0, "Provided sampled number is bigger than 1.0!");

  uint64_t size =  50; //at least 1 packet

  uint64_t previous_size = 0;
  double previous_prob = 0.0;

  for (uint32_t i= 0; i < distribution.size(); i++){
//  	NS_LOG_UNCOND(uniformSample << " " << distribution[i].first);

  	if (uniformSample <= distribution[i].first){
  		//compute the proportional size depending on the position
  		if (i > 0){
  			previous_size = distribution[i-1].second;
  			previous_prob = distribution[i-1].first;
  		}
  		double relative_distance = (uniformSample - previous_prob)/(distribution[i].first - previous_prob);
//  		NS_LOG_UNCOND(relative_distance << " " << uniformSample << " " << previous_prob << " " << distribution[i].first << " " << previous_size);
  		size = previous_size + (relative_distance * (distribution[i].second - previous_size));

  		break;
  	}
  }

  //avoid setting a size of 0
  //set packet size to 50 at least..
  if (size == 0){
  	size = 50;
  }

  return size;
}

//Assume a fat tree and the following naming h_x_y
std::pair<uint16_t, uint16_t> GetHostPositionPair(std::string name){

	std::stringstream text(name);
	std::string segment;
	std::pair<uint16_t, uint16_t> result;

	std::getline(text, segment, '_');
	std::getline(text, segment, '_');
	result.first = (uint16_t)std::stoi(segment);
	std::getline(text, segment, '_');
	result.second = (uint16_t)std::stoi(segment);

	return result;
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

// Function to create address string from numbers
//
std::string ipToString(uint8_t first,uint8_t second, uint8_t third, uint8_t fourth)
{
	std::string address = std::to_string(first) + "." + std::to_string(second) + "." + std::to_string(third) + "." + std::to_string(fourth);

	return address;
}

 std::string ipv4AddressToString(Ipv4Address address){

	 std::stringstream ip;
	 ip << address;
	 return ip.str();

//  	return ipToString((address.m_address & 0xff000000) >> 24, (address.m_address & 0x00ff0000) >> 16, (address.m_address & 0x0000ff00) >> 8, address.m_address & 0x000000ff);
 }

//Returns node if added to the name system , 0 if it does not exist
Ptr<Node> GetNode(std::string name){
	return Names::Find<Node>(name);
}

std::string GetNodeName(Ptr<Node> node){
	return Names::FindName(node);
}

void
allocateNodesFatTree(int k){


//  Ptr<ConstantPositionMobilityModel> loc = CreateObject<ConstantPositionMobilityModel>();
//  GetNode(node_name)->AggregateObject(loc);
//	loc->SetPosition(Vector(2,5,0));

	//Locate edge and agg
	Vector host_pos(0,30,0);
	Vector edge_pos(0,25,0);
	Vector agg_pos (0,19.5,0);

	for (int pod= 0; pod < k ; pod++){

		for (int pod_router = 0; pod_router < k/2; pod_router++){

			//Allocate edges
			Ptr<ConstantPositionMobilityModel> loc = CreateObject<ConstantPositionMobilityModel>();

  		std::stringstream router_name;
  		router_name << "r_" << pod << "_e" << pod_router;

  		//Update edge pos
  		edge_pos.x = (edge_pos.x + 3);

  		GetNode(router_name.str())->AggregateObject(loc);
  		loc->SetPosition(edge_pos);
  		NS_LOG_DEBUG("Pos: " << router_name.str() << " " << edge_pos);


//  		Allocate hosts
  		for (int host_i= 0; host_i < k/2 ; host_i++){
  			Ptr<ConstantPositionMobilityModel> loc1 = CreateObject<ConstantPositionMobilityModel>();

    		std::stringstream host_name;
    		host_name << "h_" << pod << "_" << (host_i + (k/2*pod_router));

    		double hosts_distance = 1.6;
    		host_pos.x = edge_pos.x -(hosts_distance/2) + (host_i * (hosts_distance/((k/2)-1)));

    		GetNode(host_name.str())->AggregateObject(loc1);
    		loc1->SetPosition(host_pos);
    		NS_LOG_DEBUG("Pos: " << host_name.str() << " " << host_pos);

  		}

			//Allocate aggregations
			Ptr<ConstantPositionMobilityModel> loc2 = CreateObject<ConstantPositionMobilityModel>();

  		router_name.str("");
  		router_name << "r_" << pod << "_a" << pod_router;

  		//Update edge pos
  		agg_pos.x = (agg_pos.x + 3);

  		GetNode(router_name.str())->AggregateObject(loc2);
  		loc2->SetPosition(agg_pos);
  		NS_LOG_DEBUG("Pos: " << router_name.str() << " " << agg_pos);

		}
		edge_pos.x = edge_pos.x + 3;
		agg_pos.x = agg_pos.x + 3;

	}

	//Allocate Core routers
	int num_cores = (k/2) * (k/2);
	int offset = 6;
	double distance = (edge_pos.x -offset*2);
	double step = distance/(num_cores-1);
	Vector core_pos_i(offset,10,0);
	Vector core_pos(offset,10,0);


	for (int router_i = 0; router_i < num_cores; router_i++){

		Ptr<ConstantPositionMobilityModel> loc3 = CreateObject<ConstantPositionMobilityModel>();

		std::stringstream router_name;
		router_name << "r_c" << router_i;

		//Update edge pos
		core_pos.x =core_pos_i.x + (router_i * step);

		GetNode(router_name.str())->AggregateObject(loc3);
		loc3->SetPosition(core_pos);
		NS_LOG_DEBUG("Pos: " << router_name.str() << " " << core_pos);


	}

}

//Just works for a fat tree.
void MeasureInOutLoad(std::unordered_map<std::string, NetDeviceContainer> links, std::unordered_map<std::string, double> linkToPreviousLoad,
		network_metadata metadata, double next_schedule, network_load load_data){


	uint32_t k = metadata.k;
	DataRate linkBandwidth = metadata.linkBandwidth;

	std::stringstream host_name;
	std::stringstream router_name;

	double sum_of_capacities = 0;
	double current_total_load = 0;
	double capacity_difference = 0;
	int interfaces_count = 0;

	for (uint32_t pod = 0; pod < k; pod++){
		for (uint32_t edge_i = 0; edge_i < k/2; edge_i++){
			for (uint32_t host_i = 0; host_i < k/2; host_i++){

				uint32_t real_host_i = host_i + (edge_i * k/2);

				host_name << "h_" << pod << "_" << real_host_i;
				router_name << "r_" << pod << "_e" << edge_i;

				NetDeviceContainer interface = links[host_name.str() + "->" + router_name.str()];

				//Get Device queues TODO: if we add RED queues.... this will not work....
//
//			  PointerValue p2p_queue;
//			  interface.Get(0)->GetAttribute("TxQueue", p2p_queue);
//			  Ptr<Queue<Packet>> queue_rx = p2p_queue.Get<Queue<Packet>>();
//
//			  PointerValue p2p_queue2;
//			  interface.Get(1)->GetAttribute("TxQueue", p2p_queue2);
//			  Ptr<Queue<Packet>> queue_tx = p2p_queue2.Get<Queue<Packet>>();

				//ONLY THE FAT TREE WILL FIND THE INTERFACES
				Ptr<Queue<Packet>> queue_rx = DynamicCast<PointToPointNetDevice>(interface.Get(0))->GetQueue();
				Ptr<Queue<Packet>> queue_tx = DynamicCast<PointToPointNetDevice>(interface.Get(1))->GetQueue();

				//RX Link
				current_total_load =MeasureInterfaceLoad(queue_rx,
						next_schedule, host_name.str() + "_rx", linkBandwidth);

				capacity_difference= current_total_load - linkToPreviousLoad[host_name.str() + "->" + router_name.str() + "_rx"];
				linkToPreviousLoad[host_name.str() + "->" + router_name.str() + "_rx"] = current_total_load;

				//Ignore counter warps
				if (capacity_difference < 0){
					capacity_difference = current_total_load;
				}
				sum_of_capacities += capacity_difference;


				//TX Link
				current_total_load =MeasureInterfaceLoad(queue_tx,
						next_schedule, host_name.str() + "_tx", linkBandwidth);

				capacity_difference= current_total_load - linkToPreviousLoad[host_name.str() + "->" + router_name.str() + "_tx"];
				linkToPreviousLoad[host_name.str() + "->" + router_name.str() + "_tx"] = current_total_load;

				//wrap
				if (capacity_difference < 0){
					capacity_difference = current_total_load;
				}
				sum_of_capacities += capacity_difference;

				interfaces_count +=2;

				host_name.str("");
				router_name.str("");
			}
		}
	}

	//compute average network usage
	double average_usage = sum_of_capacities / interfaces_count;

	NS_LOG_UNCOND("Netowork Load: " <<  " " << average_usage);

	if (average_usage >= load_data.stopThreshold)
	{
		double now = Simulator::Now().GetSeconds();
		*(load_data.startTime) = now;
		NS_LOG_DEBUG("Above threshold: Account Flows From: " << now);
		return;
	}
	else
	{
		//Reschedule the function until it reaches a certain load
		Simulator::Schedule(Seconds(next_schedule), &MeasureInOutLoad, links,linkToPreviousLoad, metadata, next_schedule, load_data);

	}
}


double MeasureInterfaceLoad(Ptr<Queue<Packet>> q,  double next_schedule, std::string name, DataRate linkBandwidth){

	uint32_t current_counter = q->GetTotalReceivedBytes();

	double total_load = double(current_counter)/BytesFromRate(DataRate(linkBandwidth), next_schedule);

//	NS_LOG_DEBUG(name <<  " " <<  total_load);


//	Simulator::Schedule(Seconds(next_schedule), &MeasureInterfaceLoad, q, current_counter, next_schedule, name);

	return total_load;
}


  //TRACE SINKS

  void
  CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
  {
  //  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
    *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
  }

   void
  RxDropPcap (Ptr<PcapFileWrapper> file, Ptr<const Packet> packet)
  {
  //	Ptr<PcapFileWrapper> file OLD VERSION
    //NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());

    file->Write (Simulator::Now (), packet);
  }

   void
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

  void
  TxDrop (std::string s, Ptr<const Packet> p){
  	static int counter = 0;
  	NS_LOG_UNCOND (counter++ << " " << s << " at " << Simulator::Now ().GetSeconds ()) ;
  }


void printNow(double delay){
	NS_LOG_UNCOND("\nSimulation Time: " << Simulator::Now().GetSeconds() << "\n");
	Simulator::Schedule (Seconds(delay), &printNow, delay);
}

void saveNow(double delay, Ptr<OutputStreamWrapper> file){

	*(file->GetStream()) << Simulator::Now().GetSeconds() << "\n";
	(file->GetStream())->flush();

	Simulator::Schedule (Seconds(delay), &saveNow, delay, file);
}


void PrintQueueSize(Ptr<Queue<Packet>> q){
	uint32_t size = q->GetNPackets();
	if (size > 0){
		NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " " <<  size);
	}
	Simulator::Schedule(Seconds(0.001), &PrintQueueSize, q);
}

//SWIFT-P4 utils

//Reads a file with RTTs.
std::vector<double> getRtts(std::string rttsFile, uint32_t max_lines){

	std::vector<double> rttVector;
	std::ifstream infile(rttsFile);

  NS_ASSERT_MSG(infile, "Please provide a valid file for reading RTT values");
  double rtt;
  uint32_t count_limit = 0;
  while (infile >> rtt and (count_limit < max_lines or max_lines == 0))
  {
  	rttVector.push_back(rtt);
  	count_limit++;
  }

  infile.close();

  return rttVector;
}

std::vector<flow_size_metadata> getFlowSizes(std::string flowSizeFile, uint32_t max_lines){
	std::vector<flow_size_metadata> flows;

	std::ifstream infile(flowSizeFile);

  NS_ASSERT_MSG(infile, "Please provide a valid file for the flow Size values");

  flow_size_metadata flow_metadata;

  uint32_t count_limit = 0;
  while (infile >> flow_metadata.packets >> flow_metadata.duration >> flow_metadata.bytes  and (count_limit < max_lines or max_lines == 0))
  {
  	flows.push_back(flow_metadata);
  	count_limit++;
  }
  infile.close();

	return flows;
}

uint64_t leftMostPowerOfTen(uint64_t number){
	uint64_t leftMost = 0;
	uint64_t power_of_10 = 0;

	//Handle an exception
	if (number == 0){
		return 0;
	}

	while(number != 0)
	{
		leftMost = number;
		power_of_10++;
		number /=10;
	}
	return leftMost * std::pow(10, power_of_10 -1);
}

std::pair<Ptr<Node>, Ptr<Node>> rttToNodePair(std::unordered_map<uint64_t, std::vector<Ptr<Node>>> rtt_to_senders,
																							std::unordered_map<uint64_t, std::vector<Ptr<Node>>> rtt_to_receivers,
																							double rtt){


	//I assume that the RTT is in seconds so first we convert it to time
	//Since rtt = propagation time * 2 , we devide rtt time by 2
	Time rtt_t = Time((rtt/2)*1e9);

	std::pair<Ptr<Node>, Ptr<Node>> src_and_dst;

	uint64_t sender_delay = leftMostPowerOfTen(rtt_t.GetInteger());
	uint64_t receiver_delay = leftMostPowerOfTen(rtt_t.GetInteger() - sender_delay);

	NS_LOG_DEBUG("Sender's Link Delay: " << Time(sender_delay).GetSeconds() << " --- Receiver's Link Delay: " << Time(receiver_delay).GetSeconds());

	//Assumes that the desired delay exists in the unordered map, that is a big assumption....
	//multiple hosts could be set with the same delay, so we pick up one randomly

	std::unordered_map<uint64_t, std::vector<Ptr<Node>>>::iterator iter = rtt_to_senders.find(sender_delay);

	if (iter != rtt_to_senders.end()){
		src_and_dst.first  = randomFromVector<Ptr<Node>>(iter->second);
	}

	iter = rtt_to_receivers.find(receiver_delay);
	if (iter != rtt_to_receivers.end()){
		src_and_dst.second  = randomFromVector<Ptr<Node>>(iter->second);
	}
//	src_and_dst.first  = randomFromVector(rtt_to_senders[sender_delay]);
//	src_and_dst.second = randomFromVector(rtt_to_receivers[receiver_delay]);

	return src_and_dst;

}


}
