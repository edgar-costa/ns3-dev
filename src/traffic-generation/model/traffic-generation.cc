/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "traffic-generation.h"
#include "simple-send.h"
#include "ns3/applications-module.h"
#include "ns3/utils.h"
#include "ns3/traffic-generation-module.h"
#include <random>

NS_LOG_COMPONENT_DEFINE ("traffic-generation");


namespace ns3 {

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



Ptr<Socket> installSimpleSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t sinkPort, DataRate dataRate, uint32_t numPackets, std::string protocol){

  Ptr<Socket> ns3Socket;
  uint32_t num_packets = numPackets;

  //had to put an initial value
  if (protocol == "TCP")
  {
    ns3Socket = Socket::CreateSocket (srcHost, TcpSocketFactory::GetTypeId ());
  }
  else
	{
    ns3Socket = Socket::CreateSocket (srcHost, UdpSocketFactory::GetTypeId ());
	}


  Ipv4Address addr = GetNodeIp(dstHost);

  Address sinkAddress (InetSocketAddress (addr, sinkPort));

  Ptr<SimpleSend> app = CreateObject<SimpleSend> ();
  app->Setup (ns3Socket, sinkAddress, 1440, num_packets, dataRate);

  srcHost->AddApplication (app);

  app->SetStartTime (Seconds (1.));
  //Stop not needed since the simulator stops when there are no more packets to send.
  //app->SetStopTime (Seconds (1000.));
  return ns3Socket;
}

//DO THE SAME WITH THE BULK APP, WHICH IS PROBABLY WHAT WE WANT TO HAVE.
void installBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime,
		Ptr<OutputStreamWrapper> fctFile, Ptr<OutputStreamWrapper> counterFile, uint64_t flowId, uint64_t * recordedFlowsCounter, double *startRecordingTime, double recordingTime){

  Ipv4Address addr = GetNodeIp(dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<CustomBulkApplication> bulkSender = CreateObject<CustomBulkApplication>();

//  Ptr<Socket> socket;
//  socket = Socket::CreateSocket (srcHost, TcpSocketFactory::GetTypeId ());
//  bulkSender->SetSocket(socket);

  bulkSender->SetAttribute("Protocol", TypeIdValue(TcpSocketFactory::GetTypeId()));
  bulkSender->SetAttribute("MaxBytes", UintegerValue(size));
  bulkSender->SetAttribute("Remote", AddressValue(sinkAddress));
  bulkSender->SetAttribute("FlowId", UintegerValue(flowId));

  bulkSender->SetOutputFile(fctFile);
  bulkSender->SetCounterFile(counterFile);
  bulkSender->SetStartRecordingTime(startRecordingTime);
  bulkSender->SetRecordingTime(recordingTime);
  bulkSender->SetRecordedFlowsCounter(recordedFlowsCounter);

  //Install app
  srcHost->AddApplication(bulkSender);

  bulkSender->SetStartTime(Seconds(startTime));
  bulkSender->SetStopTime(Seconds(1000));

  return;
//  return socket;
}



std::unordered_map <std::string, std::vector<uint16_t>> installSinks(NodeContainer hosts, uint16_t sinksPerHost, uint32_t duration, std::string protocol){

	std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts;
  Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable> ();
  uint16_t starting_port;

  for(NodeContainer::Iterator host = hosts.Begin(); host != hosts.End (); host ++){

  	starting_port = random_generator->GetInteger(0, (uint16_t)-2 - sinksPerHost);
  	std::string host_name = GetNodeName((*host));

  	hostsToPorts[host_name] = std::vector<uint16_t>();

  	for (int i = 0; i < sinksPerHost ; i++){
  		installSink((*host), starting_port+i, duration, protocol);
  		//Add port into the vector
  		//NS_LOG_DEBUG("Install Sink: " << host_name << " port:" << starting_port+i);
    	hostsToPorts[host_name].push_back(starting_port+i);
  	}

  }

  return hostsToPorts;
}

//**
//Traffic Patterns
//**

//* Stride

void startStride(NodeContainer hosts, std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts,
		uint64_t flowSize, uint16_t nFlows, uint16_t offset, Ptr<OutputStreamWrapper> fctFile, Ptr<OutputStreamWrapper> counterFile){

//	uint16_t vector_size = hostsToPorts.begin()->second.size();
	uint16_t numHosts =  hosts.GetN();

	uint16_t index = 0;
	uint64_t flowId = 0;
	for (NodeContainer::Iterator host = hosts.Begin(); host != hosts.End(); host++){

		//Get receiver
		Ptr<Node> dst = hosts.Get((index + offset) % numHosts);
		std::vector<uint16_t> availablePorts = hostsToPorts[GetNodeName(dst)];

		for (int flow_i =0; flow_i < nFlows; flow_i++){

			//get random available port
			uint16_t dport = randomFromVector<uint16_t>(availablePorts);

			//create sender
			NS_LOG_DEBUG("Start Sender: src:" << GetNodeName(*host) << " dst:" <<  GetNodeName(dst) << " dport:" << dport);
			installBulkSend((*host), dst, dport, flowSize, 1, fctFile,counterFile, flowId);
			flowId++;
			//installSimpleSend((*host), dst,	dport, sendingRate, 100, "TCP");
		}
		index++;
//	if (index == 1){
//		break;
//	}
	}
}

//* Random: receiver is always in a different pod

void startRandom(NodeContainer hosts, std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts,
		DataRate sendingRate, uint16_t flowsPerHost, uint16_t k, Ptr<OutputStreamWrapper> fctFile,Ptr<OutputStreamWrapper> counterFile){

		Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable> ();
		//	uint16_t vector_size = hostsToPorts.begin()->second.size();
		uint16_t numHosts =  hosts.GetN();
		uint16_t hostsPerPod = ((k/2) * (k/2));

		uint16_t index = 0;
		uint64_t flowId = 0;
		for (NodeContainer::Iterator host = hosts.Begin(); host != hosts.End(); host++){

			//index of the source pod
			uint16_t srcPod = index / (hostsPerPod);

			for (int flow_i =0; flow_i < flowsPerHost; flow_i++){

				//Compute receiver Index
				uint32_t recvIndex = random_generator->GetInteger(0, numHosts-1);
				while (recvIndex/hostsPerPod == srcPod){
					recvIndex = random_generator->GetInteger(0, numHosts-1);
				}

				//Get host
				Ptr<Node> dst = hosts.Get(recvIndex);
				std::vector<uint16_t> availablePorts = hostsToPorts[GetNodeName(dst)];


				//get random available port
				uint16_t dport = randomFromVector<uint16_t>(availablePorts);

				//create sender
				NS_LOG_DEBUG("Start Sender: src:" << GetNodeName(*host) << " dst:" <<  GetNodeName(dst) << " dport:" << dport);
				installBulkSend((*host), dst, dport, BytesFromRate(DataRate("10Mbps"), 10),1, fctFile, counterFile, flowId);
				flowId++;
				//installSimpleSend((*host), dst,	dport, sendingRate, 100, "TCP");
			}
		}

}

//Using distributions...

void sendFromDistribution(NodeContainer hosts, std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts,
		uint16_t k, Ptr<OutputStreamWrapper> fctFile,Ptr<OutputStreamWrapper> counterFile, std::string distributionFile,uint32_t seed, uint32_t interArrivalFlow,
		double intraPodProb, double interPodProb, double simulationTime, double *startRecordingTime, double recordingTime, uint64_t * recordedFlowsCounter){

	NS_ASSERT_MSG(interArrivalFlow >= hosts.GetN(), "Inter arrival flows has to be at least 1 flow per unit");

	// Compute intra pod probability from interpodpod
	// For this tests We set probability to send to the same subnet to 0 since we want to stress a bit the network to create congestion.

	//Random generator to select variables...
  Ptr<UniformRandomVariable> randomGenerator = CreateObject<UniformRandomVariable> ();


	double sameNetProb = 1- interPodProb - intraPodProb;
	std::vector< std::pair<double,uint64_t>> sizeDistribution = GetDistribution(distributionFile);


	//Prepare distributions
	//Uniform distribution to select flows size
	std::random_device rd0;  //Will be used to obtain a seed for the random number engine
  std::mt19937 gen0(rd0()); //Standard mersenne_twister_engine seeded with rd()
  gen0.seed(seed);
  std::uniform_real_distribution<double> uniformDistributionSize(0.0,1.0);

	//Uniform distribution to select flows size
	std::random_device rd;  //Will be used to obtain a seed for the random number engine
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  gen.seed(seed*2);
  std::uniform_real_distribution<double> uniformDistributionHosts(0.0,1.0);

  uint64_t flowSize;

  uint64_t flowId = 0;

//  uint64_t recordedFlowsCounter = 0;
	uint64_t size_counter = 0;

	for (NodeContainer::Iterator host = hosts.Begin(); host != hosts.End(); host++){

		//simulation start time
		double startTime = 1;

	  //Exponential distribution to select flow inter arrival time per sender
	  std::random_device rd2;  //Will be used to obtain a seed for the random number engine
	  std::mt19937 gen2(rd2()); //Standard mersenne_twister_engine seeded with rd()
	  gen2.seed(seed*3);
	  std::exponential_distribution<double> interArrivalTime(interArrivalFlow/hosts.GetN());

		Ptr<Node> src = *host;

		//Get node information by name
		std::string src_name = GetNodeName(src);
		//Get node pod and positions
		std::pair<uint16_t, uint16_t> pod_subnet = GetHostPositionPair(src_name);
		//with that we get in which subnet is the hosts (whithin one pod) for example in k =8 there are 4 subnets per POD.
		uint16_t pod_subnet_index = pod_subnet.second / (k/2);

		//dst name
		std::stringstream dst_name;


		while ((startTime -1) < simulationTime){
			//Get the destination range by picking a number from the normal distribution

			double dest_area = uniformDistributionHosts(gen);

			//clear dst_name
			dst_name.str("");
			dst_name << "h_";

			// Destination at SamePod
			if (dest_area < sameNetProb){

				//select destination host
				uint32_t inpod_index;
				uint16_t index_offset = pod_subnet_index * (k/2);
				inpod_index = index_offset + randomGenerator->GetInteger(0, k/2 - 1);
				while (inpod_index == pod_subnet.second){
					inpod_index = index_offset + randomGenerator->GetInteger(0, k/2 -1);
				}

				//Create dst name
				dst_name << pod_subnet.first << "_" << inpod_index;

			}
			//Destination in the same Pod but not in the same subnetwork..
			else if (dest_area < (sameNetProb + intraPodProb)){

				uint32_t inpod_index;
				inpod_index = randomGenerator->GetInteger(0, (k/2 * k/2) -1);

					while (pod_subnet_index == (inpod_index/(k/2))){
						inpod_index = randomGenerator->GetInteger(0, (k/2 * k/2) -1);
					}

			  dst_name << pod_subnet.first << "_" << inpod_index;
			}

			// Destination in another Pod
			else{

				uint32_t pod_index;
				pod_index = randomGenerator->GetInteger(0, k-1);
				while (pod_index == pod_subnet.first){
					pod_index = randomGenerator->GetInteger(0, k-1);
				}

				dst_name << pod_index << "_" << randomGenerator->GetInteger(0, (k/2 * k/2)-1);
			}

			Ptr<Node> dst = GetNode(dst_name.str());

			std::vector<uint16_t> availablePorts = hostsToPorts[dst_name.str()];
			uint16_t dport = randomFromVector<uint16_t>(availablePorts);

			//get flow size and starting time
			flowSize = GetFlowSizeFromDistribution(sizeDistribution, uniformDistributionSize(gen0));
			startTime += interArrivalTime(gen2);


//			if (startTime > 1 and startTime <= 2){
//				size_counter += flowSize;
//				NS_LOG_UNCOND("Total size: " << size_counter << " " << startTime << " " << flowSize << " " << src_name << " " << dst_name.str());
//			}
//			else if (startTime > 3 and startTime <=3.01){
//				NS_LOG_UNCOND("Total size: " << size_counter);
//
//			}

			//Install the application in the host.
			//NS_LOG_DEBUG("Starts flow: src->" << src_name << " dst->" << dst_name.str() << " size->" <<flowSize << " startTime->"<<startTime);

//			std::cout << startTime << " " << flowSize << " " << src_name << " " << dst_name.str() << "\n";

			installBulkSend(src, dst, dport, flowSize, startTime, fctFile,counterFile, flowId, recordedFlowsCounter, startRecordingTime, recordingTime);
			flowId++;

		}
	}
	std::clog << "Flow Count:" << flowId;
}

}

