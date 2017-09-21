/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"



namespace ns3 {

/* ... */
extern Ptr<UniformRandomVariable> random_variable;


struct network_load{
	double stopThreshold;
	double *startTime;
};

struct network_metadata{
	uint32_t k;
	DataRate linkBandwidth;
};

struct flow_size_metadata{
	uint32_t packets;
	uint32_t duration;
	uint64_t bytes;
};

Ipv4Address GetNodeIp(std::string node_name);
Ipv4Address GetNodeIp(Ptr<Node> node);

std::string ipToString(uint8_t first,uint8_t second, uint8_t third, uint8_t fourth);
Ptr<Node> GetNode(std::string name);
void allocateNodesFatTree(int k);
std::string GetNodeName(Ptr<Node> node);
std::string ipv4AddressToString(Ipv4Address address);
uint64_t BytesFromRate(DataRate dataRate, double time);
std::vector< std::pair<double,uint64_t>> GetDistribution(std::string distributionFile);
uint64_t GetFlowSizeFromDistribution(std::vector< std::pair<double,uint64_t>> distribution, double uniformSample);
std::pair<uint16_t, uint16_t> GetHostPositionPair(std::string name);
void printNow(double delay);
void saveNow(double delay, Ptr<OutputStreamWrapper> file);

uint64_t hash_string(std::string message);
void MeasureInOutLoad(std::unordered_map<std::string, NetDeviceContainer> links, std::unordered_map<std::string, double> linkToPreviousLoad,
		network_metadata metadata, double next_schedule, network_load load_data);

double MeasureInterfaceLoad(Ptr<Queue<Packet>> q,  double next_schedule, std::string name, DataRate linkBandwidth);

// trace sinks
void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd);

 void
RxDropPcap (Ptr<PcapFileWrapper> file, Ptr<const Packet> packet);

 void
RxDropAscii (Ptr<OutputStreamWrapper> file, Ptr<const Packet> packet);

 void
TxDrop (std::string s, Ptr<const Packet> p);

void PrintQueueSize(Ptr<Queue<Packet>> q);


std::vector<double> getRtts(std::string rttsFile, uint32_t max_lines = 0);
std::vector<flow_size_metadata> getFlowSizes(std::string flowSizeFile, uint32_t max_lines = 0);

std::pair<Ptr<Node>, Ptr<Node>> rttToNodePair(std::unordered_map<uint64_t, std::vector<Ptr<Node>>> rtt_to_senders,
																							std::unordered_map<uint64_t, std::vector<Ptr<Node>>> rtt_to_receivers,
																							double rtt);
uint64_t leftMostPowerOfTen(uint64_t number);

//gets a random element from a vector!
template <typename T>
T randomFromVector(std::vector<T> & vect){
	return vect[random_variable->GetInteger(0, vect.size() -1 )];
}

}

#endif /* UTILS_H */

