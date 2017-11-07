// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2008 University of Washington
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <vector>
#include <iomanip>
#include <algorithm>
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/boolean.h"
#include "ns3/node.h"
#include "ipv4-global-routing.h"
#include "global-route-manager.h"

//ADDED EDGAR
#include "udp-header.h"
#include "tcp-header.h"
#include "ns3/node.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/pointer.h"
#include "ns3/queue.h"
//#include "ns3/utils.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Ipv4GlobalRouting");

NS_OBJECT_ENSURE_REGISTERED (Ipv4GlobalRouting);

const uint8_t TCP_PROT_NUMBER = 6;
const uint8_t UDP_PROT_NUMBER = 17;

Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable> ();

TypeId 
Ipv4GlobalRouting::GetTypeId (void)
{ 
  static TypeId tid = TypeId ("ns3::Ipv4GlobalRouting")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
//    .AddAttribute ("RandomEcmpRouting",
//                   "Set to true if packets are randomly routed among ECMP; set to false for using only one route consistently",
//                   BooleanValue (false),
//                   MakeBooleanAccessor (&Ipv4GlobalRouting::m_randomEcmpRouting),
//                   MakeBooleanChecker ())
    .AddAttribute ("EcmpMode",
                   "Used to chose one the several impemented multi-path modes",
                   EnumValue(ECMP_NONE),
                   MakeEnumAccessor(&Ipv4GlobalRouting::m_ecmpMode),
                   MakeEnumChecker(ECMP_NONE,"ECMP_NONE",
                  		 	 	 	 	 	 	 ECMP_RANDOM,"ECMP_RANDOM",
																	 ECMP_PER_FLOW, "ECMP_PER_FLOW",
																	 ECMP_RR, "ECMP_RR",
																	 ECMP_RANDOM_FLOWLET, "ECMP_RANDOM_FLOWLET",
																	 ECMP_DRILL, "ECMP_DRILL"))

    .AddAttribute ("RespondToInterfaceEvents",
                   "Set to true if you want to dynamically recompute the global routes upon Interface notification events (up/down, or add/remove address)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Ipv4GlobalRouting::m_respondToInterfaceEvents),
                   MakeBooleanChecker ())

	  .AddAttribute("FlowletGap",
	  		          "Time Gap between flowlets. In nanoseconds",
									IntegerValue(50000000),
									MakeIntegerAccessor(&Ipv4GlobalRouting::m_flowletGap),
									MakeIntegerChecker<int64_t>())

		.AddAttribute("DrillRandomChecks",
 	  		          "How many random interfaces are checked",
									UintegerValue(2),
									MakeIntegerAccessor(&Ipv4GlobalRouting::m_drillRandomChecks),
									MakeUintegerChecker<uint16_t>())

		.AddAttribute("DrillMemoryUnits",
				          "Store the best n previously obseved ports",
		   						UintegerValue(1),
									MakeIntegerAccessor(&Ipv4GlobalRouting::m_drillMemoryUnits),
									MakeUintegerChecker<uint16_t>())

  ;
  return tid;
}

Ipv4GlobalRouting::Ipv4GlobalRouting () 
  : m_randomEcmpRouting (false),
    m_respondToInterfaceEvents (false),
		m_lastInterfaceUsed(0),
		m_ecmpMode(ECMP_NONE),
    m_flowletGap(50000000),
		m_drillMemoryUnits(1),
		m_drillRandomChecks(2)

{
  NS_LOG_FUNCTION (this);

  Simulator::Now ().GetSeconds ();


  //uniform variable
  m_rand = CreateObject<UniformRandomVariable> ();
  m_seed = m_rand->GetInteger (0,(uint32_t)-1);

  //NS_LOG_UNCOND("node " << " " << m_seed);

  //hasher for ecmp
  hasher = Hasher();
}

Ipv4GlobalRouting::~Ipv4GlobalRouting ()
{
  NS_LOG_FUNCTION (this);
}

void 
Ipv4GlobalRouting::AddHostRouteTo (Ipv4Address dest, 
                                   Ipv4Address nextHop, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION (this << dest << nextHop << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateHostRouteTo (dest, nextHop, interface);
  m_hostRoutes.push_back (route);
}

void 
Ipv4GlobalRouting::AddHostRouteTo (Ipv4Address dest, 
                                   uint32_t interface)
{
  NS_LOG_FUNCTION (this << dest << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateHostRouteTo (dest, interface);
  m_hostRoutes.push_back (route);
}

void 
Ipv4GlobalRouting::AddNetworkRouteTo (Ipv4Address network, 
                                      Ipv4Mask networkMask, 
                                      Ipv4Address nextHop, 
                                      uint32_t interface)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        nextHop,
                                                        interface);
  m_networkRoutes.push_back (route);
}

void 
Ipv4GlobalRouting::AddNetworkRouteTo (Ipv4Address network, 
                                      Ipv4Mask networkMask, 
                                      uint32_t interface)
{
  NS_LOG_FUNCTION (this << network << networkMask << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        interface);
  m_networkRoutes.push_back (route);
}

void 
Ipv4GlobalRouting::AddASExternalRouteTo (Ipv4Address network, 
                                         Ipv4Mask networkMask,
                                         Ipv4Address nextHop,
                                         uint32_t interface)
{
  NS_LOG_FUNCTION (this << network << networkMask << nextHop << interface);
  Ipv4RoutingTableEntry *route = new Ipv4RoutingTableEntry ();
  *route = Ipv4RoutingTableEntry::CreateNetworkRouteTo (network,
                                                        networkMask,
                                                        nextHop,
                                                        interface);
  m_ASexternalRoutes.push_back (route);
}

uint64_t
Ipv4GlobalRouting::GetFlowHash(const Ipv4Header &header, Ptr<const Packet> ipPayload)
{
  NS_LOG_FUNCTION(header);

  Ptr<Node> node = m_ipv4->GetObject<Node>();
  //node ID for polarization
  //uint32_t node_id = node->GetId();

  hasher.clear();
  std::ostringstream oss;
  oss << header.GetSource()
      << header.GetDestination()
      << header.GetProtocol()
      << m_seed;

  switch (header.GetProtocol())
    {
  case UDP_PROT_NUMBER:
    {
      UdpHeader udpHeader;
      ipPayload->PeekHeader(udpHeader);
//      NS_LOG_DEBUG ("FiveTuple() -> UDP: (src, dst, protNb, sPort, dPort) - "
//          << header.GetSource() << " , "
//          << header.GetDestination() << " , "
//          << (int)header.GetProtocol() << " , "
//          << (int)udpHeader.GetSourcePort () << " , "
//          << (int)udpHeader.GetDestinationPort ());

      oss << udpHeader.GetSourcePort()
          << udpHeader.GetDestinationPort();

      break;
    }
  case TCP_PROT_NUMBER:
    {
      TcpHeader tcpHeader;
      ipPayload->PeekHeader(tcpHeader);
//      NS_LOG_DEBUG ("FiveTuple() -> TCP: (src, dst, protNb, sPort, dPort) -  "
//          << header.GetSource() << " , "
//          << header.GetDestination() << " , "
//          << (int)header.GetProtocol() << " , "
//          << (int)tcpHeader.GetSourcePort () << " , "
//          << (int)tcpHeader.GetDestinationPort ());

      oss << tcpHeader.GetSourcePort()
          << tcpHeader.GetDestinationPort();

      break;
    }
  default:
    {
    	//TODO maybe this brings us problems with other protcols no?
    	//What about not even doing this... we can hash using just src,dst,proto, id
      NS_FATAL_ERROR("Udp or Tcp header not found " << (int) header.GetProtocol());
      break;
    }
    }

  std::string data = oss.str();
  uint32_t hash = hasher.GetHash32(data);
  oss.str("");
  //NS_LOG_UNCOND("hash value node: " << node_id << " " << hash << " seed: " << m_seed);
  return hash;
}

std::string
Ipv4GlobalRouting::GetFlowTuple(const Ipv4Header &header, Ptr<const Packet> ipPayload)
{
  NS_LOG_FUNCTION(header);

  std::ostringstream tuple;

  switch (header.GetProtocol())
    {
  case UDP_PROT_NUMBER:
    {
      UdpHeader udpHeader;
      ipPayload->PeekHeader(udpHeader);

      tuple << header.GetSource() << ":" << udpHeader.GetSourcePort()
      		<< ":" << header.GetDestination() << ":" << udpHeader.GetDestinationPort()
					<< ":" << uint32_t(header.GetProtocol());

      break;
    }
  case TCP_PROT_NUMBER:
    {
      TcpHeader tcpHeader;
      ipPayload->PeekHeader(tcpHeader);

      tuple << header.GetSource() << ":" << tcpHeader.GetSourcePort()
      		<< ":" << header.GetDestination() << ":" << tcpHeader.GetDestinationPort()
					<< ":" << uint32_t(header.GetProtocol());

      break;
    }
  default:
    {
    	//TODO maybe this brings us problems with other protcols no?
    	//What about not even doing this... we can hash using just src,dst,proto, id
      NS_FATAL_ERROR("Udp or Tcp header not found " << (int) header.GetProtocol());
      break;
    }
    }

  return tuple.str();
}


//TODO review this function I think we should not iterate over all Ninterfaces, but over all possible next hops.
uint32_t
Ipv4GlobalRouting::GetNextInterface(uint32_t m_lastInterfaceUsed)
{
  uint32_t nextInterface;

  nextInterface = (m_lastInterfaceUsed + 1) % m_ipv4->GetNInterfaces();
  if (nextInterface == 0)
    nextInterface++;
  return nextInterface;
}

uint32_t Ipv4GlobalRouting::GetQueueSize(std::vector<Ipv4RoutingTableEntry*> allRoutes, uint32_t selectIndex ){

  Ipv4RoutingTableEntry* route_i = allRoutes.at (selectIndex);
  uint32_t interfaceIndex = route_i->GetInterface ();

  Ptr<NetDevice> device = m_ipv4->GetNetDevice (interfaceIndex);

  PointerValue p2p_queue;
  device->GetAttribute("TxQueue", p2p_queue);
  Ptr<Queue<Packet>> txQueue = p2p_queue.Get<Queue<Packet>>();


  uint32_t currentSize = txQueue->GetNPackets();
  return currentSize;
}




Ptr<Ipv4Route>
Ipv4GlobalRouting::LookupGlobal (Ipv4Address dest, Ptr<NetDevice> oif)
{
  NS_LOG_FUNCTION (this << dest << oif);
  NS_LOG_LOGIC ("Looking for route for destination " << dest);
  Ptr<Ipv4Route> rtentry = 0;
  // store all available routes that bring packets to their destination
  typedef std::vector<Ipv4RoutingTableEntry*> RouteVec_t;
  RouteVec_t allRoutes;

  NS_LOG_LOGIC ("Number of m_hostRoutes = " << m_hostRoutes.size ());
  for (HostRoutesCI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i++) 
    {
      NS_ASSERT ((*i)->IsHost ());
      if ((*i)->GetDest ().IsEqual (dest)) 
        {
          if (oif != 0)
            {
              if (oif != m_ipv4->GetNetDevice ((*i)->GetInterface ()))
                {
                  NS_LOG_LOGIC ("Not on requested interface, skipping");
                  continue;
                }
            }
          allRoutes.push_back (*i);
          NS_LOG_LOGIC (allRoutes.size () << "Found global host route" << *i); 
        }
    }
  if (allRoutes.size () == 0) // if no host route is found
    {
      NS_LOG_LOGIC ("Number of m_networkRoutes" << m_networkRoutes.size ());
      for (NetworkRoutesI j = m_networkRoutes.begin (); 
           j != m_networkRoutes.end (); 
           j++) 
        {
          Ipv4Mask mask = (*j)->GetDestNetworkMask ();
          Ipv4Address entry = (*j)->GetDestNetwork ();
          if (mask.IsMatch (dest, entry)) 
            {
              if (oif != 0)
                {
                  if (oif != m_ipv4->GetNetDevice ((*j)->GetInterface ()))
                    {
                      NS_LOG_LOGIC ("Not on requested interface, skipping");
                      continue;
                    }
                }
              allRoutes.push_back (*j);
              NS_LOG_LOGIC (allRoutes.size () << "Found global network route" << *j);
            }
        }
    }
  if (allRoutes.size () == 0)  // consider external if no host/network found
    {
      for (ASExternalRoutesI k = m_ASexternalRoutes.begin ();
           k != m_ASexternalRoutes.end ();
           k++)
        {
          Ipv4Mask mask = (*k)->GetDestNetworkMask ();
          Ipv4Address entry = (*k)->GetDestNetwork ();
          if (mask.IsMatch (dest, entry))
            {
              NS_LOG_LOGIC ("Found external route" << *k);
              if (oif != 0)
                {
                  if (oif != m_ipv4->GetNetDevice ((*k)->GetInterface ()))
                    {
                      NS_LOG_LOGIC ("Not on requested interface, skipping");
                      continue;
                    }
                }
              allRoutes.push_back (*k);
              break;
            }
        }
    }
  if (allRoutes.size () > 0 ) // if route(s) is found
    {
      // pick up one of the routes uniformly at random if random
      // ECMP routing is enabled, or always select the first route
      // consistently if random ECMP routing is disabled
      uint32_t selectIndex;
      if (m_randomEcmpRouting)
        {
          selectIndex = m_rand->GetInteger (0, allRoutes.size ()-1);
        }
      else 
        {
          selectIndex = 0;
        }
      Ipv4RoutingTableEntry* route = allRoutes.at (selectIndex); 
      // create a Ipv4Route object from the selected routing table entry
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (route->GetDest ());
      /// \todo handle multi-address case
      rtentry->SetSource (m_ipv4->GetAddress (route->GetInterface (), 0).GetLocal ());
      rtentry->SetGateway (route->GetGateway ());
      uint32_t interfaceIdx = route->GetInterface ();
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
      return rtentry;
    }
  else 
    {
      return 0;
    }
}

Ptr<Ipv4Route>
Ipv4GlobalRouting::LookupGlobal (const Ipv4Header &header, Ptr<const Packet> ipPayload, Ptr<NetDevice> oif)
{
  NS_LOG_FUNCTION (this << header.GetDestination() << oif);
  NS_LOG_LOGIC ("Looking for route for destination " << header.GetDestination());

  Ipv4Address dest = header.GetDestination();

  Ptr<Ipv4Route> rtentry = 0;
  // store all available routes that bring packets to their destination
  typedef std::vector<Ipv4RoutingTableEntry*> RouteVec_t;
  RouteVec_t allRoutes;

  //FILL allRoutes object with routes to dest and with interfasce oif.
  NS_LOG_LOGIC ("Number of m_hostRoutes = " << m_hostRoutes.size ());
  for (HostRoutesCI i = m_hostRoutes.begin (); i != m_hostRoutes.end (); i++)
    {
      NS_ASSERT ((*i)->IsHost ());
      if ((*i)->GetDest ().IsEqual (dest))
        {
          if (oif != 0)
            {
              if (oif != m_ipv4->GetNetDevice ((*i)->GetInterface ()))
                {
                  NS_LOG_LOGIC ("Not on requested interface, skipping");
                  continue;
                }
            }
          allRoutes.push_back (*i);
          NS_LOG_LOGIC (allRoutes.size () << "Found global host route" << *i);
        }
    }
  if (allRoutes.size () == 0) // if no host route is found
    {
      NS_LOG_LOGIC ("Number of m_networkRoutes" << m_networkRoutes.size ());
      for (NetworkRoutesI j = m_networkRoutes.begin ();
           j != m_networkRoutes.end ();
           j++)
        {
          Ipv4Mask mask = (*j)->GetDestNetworkMask ();
          Ipv4Address entry = (*j)->GetDestNetwork ();
          if (mask.IsMatch (dest, entry))
            {
              if (oif != 0)
                {
                  if (oif != m_ipv4->GetNetDevice ((*j)->GetInterface ()))
                    {
                      NS_LOG_LOGIC ("Not on requested interface, skipping");
                      continue;
                    }
                }
              allRoutes.push_back (*j);
              NS_LOG_LOGIC (allRoutes.size () << "Found global network route" << *j);
            }
        }
    }
  if (allRoutes.size () == 0)  // consider external if no host/network found
    {
      for (ASExternalRoutesI k = m_ASexternalRoutes.begin ();
           k != m_ASexternalRoutes.end ();
           k++)
        {
          Ipv4Mask mask = (*k)->GetDestNetworkMask ();
          Ipv4Address entry = (*k)->GetDestNetwork ();
          if (mask.IsMatch (dest, entry))
            {
              NS_LOG_LOGIC ("Found external route" << *k);
              if (oif != 0)
                {
                  if (oif != m_ipv4->GetNetDevice ((*k)->GetInterface ()))
                    {
                      NS_LOG_LOGIC ("Not on requested interface, skipping");
                      continue;
                    }
                }
              allRoutes.push_back (*k);
              break;
            }
        }
    }

  // Output Interface Index decision happens here!

  if (allRoutes.size () > 0 ) // if route(s) is found
    {
      // pick up one of the routes uniformly at random if random
      // ECMP routing is enabled, or always select the first route
      // consistently if random ECMP routing is disabled
      	uint32_t selectIndex = 0;

      	//If there is a single possible route we do not even check the algorithm
      	if (allRoutes.size() > 1){

					switch (m_ecmpMode)
					{
					case ECMP_NONE:
						selectIndex = 0;
						break;

					case ECMP_RANDOM:
						selectIndex = m_rand->GetInteger (0, allRoutes.size ()-1);
//						NS_LOG_UNCOND(Names::FindName(m_ipv4->GetObject<Node>()) << " " << selectIndex);
						break;

					case ECMP_PER_FLOW:
						{
							selectIndex = (GetFlowHash(header, ipPayload) % (allRoutes.size()));

						}
						break;

					case ECMP_RR:
						// Temporary RR implementation has to be improved.
						uint32_t nextInterface;
						nextInterface = (m_lastInterfaceUsed + 1) % (allRoutes.size());

						selectIndex = nextInterface;
						m_lastInterfaceUsed = nextInterface;

						break;

					case ECMP_RANDOM_FLOWLET:

						selectIndex = 0;
						uint16_t key;
						key = GetFlowHash(header, ipPayload);
						flowlet_t flowlet_data;

						if (m_flowlet_table.count(key) > 0){ //already existing entry
							flowlet_data = m_flowlet_table[key];

							//check if the packet gap is bigger than threshold. In that case select a new random port.
							int64_t now  = Simulator::Now().GetTimeStep();

							if ((now - flowlet_data.time) > m_flowletGap){
								NS_LOG_DEBUG("Flowlet expired in node: " <<  Names::FindName(m_ipv4->GetObject<Node>()));
								NS_LOG_DEBUG("At " << Simulator::Now().GetSeconds() << " Inter packe gap is :" << (NanoSeconds(now-flowlet_data.time)).GetMilliSeconds());

								//We output the size in packets of the flowlet
								NS_LOG_DEBUG("flowletSize: "<< flowlet_data.packet_count << " " << GetFlowTuple(header, ipPayload));

								//Generate a new random port and update flowlet table
								selectIndex = m_rand->GetInteger (0, allRoutes.size ()-1);
								flowlet_data.time = now; flowlet_data.out_port = selectIndex;
								flowlet_data.packet_count = 1;
								m_flowlet_table[key] = flowlet_data;



							}
							else{
								//select port and update flowlet table

								//Here we have to do an extra check. It could be that a packet from a different flow hashed
								//into the same position. If that happens the output port index could not even exist. So to be sure
								//we select a valid port we do modulo with all the available routes. However we dont modify entry in the
								//flowlet table
								selectIndex = (flowlet_data.out_port % allRoutes.size());

								flowlet_data.time = now;
								flowlet_data.packet_count++;
								m_flowlet_table[key] = flowlet_data;
							}
						}
						else { //new entry
							int64_t now  = Simulator::Now().GetTimeStep();
							selectIndex = m_rand->GetInteger (0, allRoutes.size ()-1);
							flowlet_data.time = now; flowlet_data.out_port = selectIndex;
							flowlet_data.packet_count = 1;
							m_flowlet_table[key] = flowlet_data;

						}

						break;

					case ECMP_DRILL:
					{
						selectIndex = 0;

						uint32_t numNextHops = allRoutes.size();

						std::stringstream ip;
						ip << header.GetDestination();
						std::string dstAddr = ip.str();

						std::unordered_set<uint32_t> previousBestOuts = m_drill_table[dstAddr];

//						min (next hop interfaces, drillPicks)
						uint16_t num_random_picks = std::min(m_drillRandomChecks+m_drillMemoryUnits, numNextHops);

//						If never explored that output.
						uint32_t sample;
						while (previousBestOuts.size() < num_random_picks){
							//a bit ineficient but ok
							sample = random_variable->GetInteger(0, numNextHops-1);
							previousBestOuts.insert(sample);
						}

						std::vector<std::pair<uint32_t, uint32_t>> q_size_to_index;
						//here we should get the queues for every index
						for (auto it = previousBestOuts.begin(); it != previousBestOuts.end(); it++){
							q_size_to_index.push_back(std::make_pair(GetQueueSize(allRoutes, *it), *it));
						}

						//Sort Vector
						std::sort(q_size_to_index.begin(), q_size_to_index.end());

						//debug
//						NS_LOG_DEBUG("Start");
//						for (uint32_t i = 0; i < q_size_to_index.size(); i++){
//							NS_LOG_DEBUG("Drill Debug: " << q_size_to_index[i].first << " " << q_size_to_index[i].second);
//						}
//						NS_LOG_DEBUG("Finish");

						//Get Final output port
						selectIndex = q_size_to_index[0].second;

						//Store drillMemory best units to the memory map
						previousBestOuts.clear();
						for (uint32_t i= 0; i < m_drillMemoryUnits; i++){
							previousBestOuts.insert(q_size_to_index[i].second);
						}

						//Update m_drill_table
						m_drill_table[dstAddr] = previousBestOuts;

						}


						break;

					default:
						selectIndex = 0;
						break;
					}
      	}

//				uint32_t size = GetQueueSize(allRoutes, selectIndex);
//				if (size > 0 ){
//					NS_LOG_UNCOND("Chosen Queue Size: " << size );
//				}

      Ipv4RoutingTableEntry* route = allRoutes.at (selectIndex);
      // create a Ipv4Route object from the selected routing table entry
      rtentry = Create<Ipv4Route> ();
      rtentry->SetDestination (route->GetDest ());

//      NS_LOG_UNCOND(*route);
//      NS_LOG_UNCOND(Simulator::Now().GetSeconds() << " " <<  route->GetDestNetwork()
//      		<< " " << route->GetDest() << " " << selectIndex << " " << allRoutes.size());

      /// \todo handle multi-address case
      rtentry->SetSource (m_ipv4->GetAddress (route->GetInterface (), 0).GetLocal ());
      rtentry->SetGateway (route->GetGateway ());
      uint32_t interfaceIdx = route->GetInterface ();
      rtentry->SetOutputDevice (m_ipv4->GetNetDevice (interfaceIdx));
      return rtentry;
    }
  else
    {
      return 0;
    }
}

uint32_t 
Ipv4GlobalRouting::GetNRoutes (void) const
{
  NS_LOG_FUNCTION (this);
  uint32_t n = 0;
  n += m_hostRoutes.size ();
  n += m_networkRoutes.size ();
  n += m_ASexternalRoutes.size ();
  return n;
}

Ipv4RoutingTableEntry *
Ipv4GlobalRouting::GetRoute (uint32_t index) const
{
  NS_LOG_FUNCTION (this << index);
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesCI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              return *i;
            }
          tmp++;
        }
    }
  index -= m_hostRoutes.size ();
  uint32_t tmp = 0;
  if (index < m_networkRoutes.size ())
    {
      for (NetworkRoutesCI j = m_networkRoutes.begin (); 
           j != m_networkRoutes.end ();
           j++)
        {
          if (tmp == index)
            {
              return *j;
            }
          tmp++;
        }
    }
  index -= m_networkRoutes.size ();
  tmp = 0;
  for (ASExternalRoutesCI k = m_ASexternalRoutes.begin (); 
       k != m_ASexternalRoutes.end (); 
       k++) 
    {
      if (tmp == index)
        {
          return *k;
        }
      tmp++;
    }
  NS_ASSERT (false);
  // quiet compiler.
  return 0;
}
void 
Ipv4GlobalRouting::RemoveRoute (uint32_t index)
{
  NS_LOG_FUNCTION (this << index);
  if (index < m_hostRoutes.size ())
    {
      uint32_t tmp = 0;
      for (HostRoutesI i = m_hostRoutes.begin (); 
           i != m_hostRoutes.end (); 
           i++) 
        {
          if (tmp  == index)
            {
              NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_hostRoutes.size ());
              delete *i;
              m_hostRoutes.erase (i);
              NS_LOG_LOGIC ("Done removing host route " << index << "; host route remaining size = " << m_hostRoutes.size ());
              return;
            }
          tmp++;
        }
    }
  index -= m_hostRoutes.size ();
  uint32_t tmp = 0;
  for (NetworkRoutesI j = m_networkRoutes.begin (); 
       j != m_networkRoutes.end (); 
       j++) 
    {
      if (tmp == index)
        {
          NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_networkRoutes.size ());
          delete *j;
          m_networkRoutes.erase (j);
          NS_LOG_LOGIC ("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size ());
          return;
        }
      tmp++;
    }
  index -= m_networkRoutes.size ();
  tmp = 0;
  for (ASExternalRoutesI k = m_ASexternalRoutes.begin (); 
       k != m_ASexternalRoutes.end ();
       k++)
    {
      if (tmp == index)
        {
          NS_LOG_LOGIC ("Removing route " << index << "; size = " << m_ASexternalRoutes.size ());
          delete *k;
          m_ASexternalRoutes.erase (k);
          NS_LOG_LOGIC ("Done removing network route " << index << "; network route remaining size = " << m_networkRoutes.size ());
          return;
        }
      tmp++;
    }
  NS_ASSERT (false);
}

int64_t
Ipv4GlobalRouting::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_rand->SetStream (stream);
  return 1;
}

void
Ipv4GlobalRouting::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  for (HostRoutesI i = m_hostRoutes.begin (); 
       i != m_hostRoutes.end (); 
       i = m_hostRoutes.erase (i)) 
    {
      delete (*i);
    }
  for (NetworkRoutesI j = m_networkRoutes.begin (); 
       j != m_networkRoutes.end (); 
       j = m_networkRoutes.erase (j)) 
    {
      delete (*j);
    }
  for (ASExternalRoutesI l = m_ASexternalRoutes.begin (); 
       l != m_ASexternalRoutes.end ();
       l = m_ASexternalRoutes.erase (l))
    {
      delete (*l);
    }

  Ipv4RoutingProtocol::DoDispose ();
}

// Formatted like output of "route -n" command
void
Ipv4GlobalRouting::PrintRoutingTable (Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  NS_LOG_FUNCTION (this << stream);
  std::ostream* os = stream->GetStream ();

  *os << "Node: " << m_ipv4->GetObject<Node> ()->GetId ()
   << ", Time: " << Now().As (unit)
   << ", Local time: " << GetObject<Node> ()->GetLocalTime ().As (unit)
   << ", Ipv4GlobalRouting table" << std::endl;

  if (GetNRoutes () > 0)
    {
      *os << "Destination     Gateway         Genmask         Flags Metric Ref    Use Iface" << std::endl;
      for (uint32_t j = 0; j < GetNRoutes (); j++)
        {
          std::ostringstream dest, gw, mask, flags;
          Ipv4RoutingTableEntry route = GetRoute (j);
          dest << route.GetDest ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << dest.str ();
          gw << route.GetGateway ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << gw.str ();
          mask << route.GetDestNetworkMask ();
          *os << std::setiosflags (std::ios::left) << std::setw (16) << mask.str ();
          flags << "U";
          if (route.IsHost ())
            {
              flags << "H";
            }
          else if (route.IsGateway ())
            {
              flags << "G";
            }
          *os << std::setiosflags (std::ios::left) << std::setw (6) << flags.str ();
          // Metric not implemented
          *os << "-" << "      ";
          // Ref ct not implemented
          *os << "-" << "      ";
          // Use not implemented
          *os << "-" << "   ";
          if (Names::FindName (m_ipv4->GetNetDevice (route.GetInterface ())) != "")
            {
              *os << Names::FindName (m_ipv4->GetNetDevice (route.GetInterface ()));
            }
          else
            {
              *os << route.GetInterface ();
            }
          *os << std::endl;
        }
    }
  *os << std::endl;
}

Ptr<Ipv4Route>
Ipv4GlobalRouting::RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << p << &header << oif << &sockerr);
//
// First, see if this is a multicast packet we have a route for.  If we
// have a route, then send the packet down each of the specified interfaces.
//
  if (header.GetDestination ().IsMulticast ())
    {
      NS_LOG_LOGIC ("Multicast destination-- returning false");
      return 0; // Let other routing protocols try to handle this
    }
//
// See if this is a unicast packet we have a route for.
//
  NS_LOG_LOGIC ("Unicast destination- looking up");
  //Ptr<Ipv4Route> rtentry = LookupGlobal (header.GetDestination (), oif);
  Ptr<Ipv4Route> rtentry = LookupGlobal (header, p, oif);
  if (rtentry)
    {
      sockerr = Socket::ERROR_NOTERROR;
    }
  else
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
    }
  return rtentry;
}

bool 
Ipv4GlobalRouting::RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,                             UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                LocalDeliverCallback lcb, ErrorCallback ecb)
{ 
  NS_LOG_FUNCTION (this << p << header << header.GetSource () << header.GetDestination () << idev << &lcb << &ecb);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  uint32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  if (m_ipv4->IsDestinationAddress (header.GetDestination (), iif))
    {
      if (!lcb.IsNull ())
        {
          NS_LOG_LOGIC ("Local delivery to " << header.GetDestination ());
          lcb (p, header, iif);
          return true;
        }
      else
        {
          // The local delivery callback is null.  This may be a multicast
          // or broadcast packet, so return false so that another
          // multicast routing protocol can handle it.  It should be possible
          // to extend this to explicitly check whether it is a unicast
          // packet, and invoke the error callback if so
          return false;
        }
    }

  // Check if input device supports IP forwarding
  if (m_ipv4->IsForwarding (iif) == false)
    {
      NS_LOG_LOGIC ("Forwarding disabled for this interface");
      ecb (p, header, Socket::ERROR_NOROUTETOHOST);
      return true;
    }
  // Next, try to find a route
  NS_LOG_LOGIC ("Unicast destination- looking up global route");
  Ptr<Ipv4Route> rtentry = LookupGlobal (header, p);
  if (rtentry != 0)
    {
      NS_LOG_LOGIC ("Found unicast destination- calling unicast callback");
      ucb (rtentry, p, header);
      return true;
    }
  else
    {
      NS_LOG_LOGIC ("Did not find unicast destination- returning false");
      return false; // Let other routing protocols try to handle this
                    // route request.
    }
}
void 
Ipv4GlobalRouting::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << i);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << interface << address);
  if (m_respondToInterfaceEvents && Simulator::Now ().GetSeconds () > 0)  // avoid startup events
    {
      GlobalRouteManager::DeleteGlobalRoutes ();
      GlobalRouteManager::BuildGlobalRoutingDatabase ();
      GlobalRouteManager::InitializeRoutes ();
    }
}

void 
Ipv4GlobalRouting::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION (this << ipv4);
  NS_ASSERT (m_ipv4 == 0 && ipv4 != 0);
  m_ipv4 = ipv4;
}


} // namespace ns3
