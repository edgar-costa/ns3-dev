/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef TRAFFIC_GENERATION_H
#define TRAFFIC_GENERATION_H


#include <string.h>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include <unordered_map>
#include <vector>

namespace ns3 {

void installSink(Ptr<Node> node, uint16_t sinkPort, uint32_t duration, std::string protocol);
std::unordered_map <std::string, std::vector<uint16_t>> installSinks(NodeContainer hosts, uint16_t sinksPerHost, uint32_t duration, std::string protocol);
Ptr<Socket> installSimpleSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t sinkPort, DataRate dataRate, uint32_t numPackets, std::string protocol);

void installBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime,
		Ptr<OutputStreamWrapper> fctFile, Ptr<OutputStreamWrapper> counterFile, uint64_t flowId, uint64_t * recordedFlowsCounter = NULL, double *startRecordingTime = NULL, double recordingTime= -1);

void startStride(NodeContainer hosts, std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts,
		uint64_t flowSize, uint16_t nFlows, uint16_t offset, Ptr<OutputStreamWrapper> fctFile, Ptr<OutputStreamWrapper> counterFile);

void startRandom(NodeContainer hosts, std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts,
		DataRate sendingRate, uint16_t flowsPerHost, uint16_t k, Ptr<OutputStreamWrapper> fctFile, Ptr<OutputStreamWrapper> counterFile);

void sendFromDistribution(NodeContainer hosts, std::unordered_map <std::string, std::vector<uint16_t>> hostsToPorts,
		uint16_t k, Ptr<OutputStreamWrapper> fctFile, Ptr<OutputStreamWrapper> counterFile, std::string distributionFile,uint32_t seed,
		uint32_t interArrivalFlow, double sameNetProb, double interPodProb, double simulationTime, double *startRecordingTime,
		double recordingTime, uint64_t * recordedFlowsCounter);

}

#endif /* TRAFFIC_GENERATION_H */

