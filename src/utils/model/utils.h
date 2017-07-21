/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef UTILS_H
#define UTILS_H

#include <string.h>
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
uint64_t hash_string(std::string message);
void MeasureInterfaceLoad(Ptr<Queue<Packet>> q, uint32_t previous_counter, double next_schedule, std::string name);
void MeasureInOutLoad(std::unordered_map<std::string, NetDeviceContainer> links, uint32_t k , double next_schedule);

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

template <typename T>
T randomFromVector(std::vector<T> & vect){
	return vect[random_variable->GetInteger(0, vect.size() -1 )];
}

}

#endif /* UTILS_H */

