#include "frta-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/names.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-packet-info-tag.h"
#include "ns3/node.h"
#include "ns3/udp-socket-factory.h"
#include <fstream>
#include <algorithm>
#include <vector>
#include <set>
#include <functional>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FrtaRoutingProtocol");

std::ofstream g_protocolLog("frta-protocol.log", std::ios::out | std::ios::app);

// Initialize static members
const Time FrtaRoutingProtocol::ROUTE_REQUEST_TIMEOUT = Seconds(2.0);
const Time FrtaRoutingProtocol::ROUTE_CACHE_TIMEOUT = Seconds(30.0);

//-----------------------------------------------------------------------------
// TrustTag Implementation
//-----------------------------------------------------------------------------

NS_OBJECT_ENSURE_REGISTERED(TrustTag);

TypeId
TrustTag::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::TrustTag")
    .SetParent<Tag>()
    .SetGroupName("Internet")
    .AddConstructor<TrustTag>();
  return tid;
}

TypeId
TrustTag::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

uint32_t
TrustTag::GetSerializedSize(void) const
{
  return sizeof(double);
}

void
TrustTag::Serialize(TagBuffer i) const
{
  i.WriteDouble(m_trust);
}

void
TrustTag::Deserialize(TagBuffer i)
{
  m_trust = i.ReadDouble();
}

void
TrustTag::Print(std::ostream &os) const
{
  os << "Trust=" << m_trust;
}

void
TrustTag::SetTrust(double trust)
{
  m_trust = trust;
}

double
TrustTag::GetTrust(void) const
{
  return m_trust;
}

//-----------------------------------------------------------------------------
// FrtaRoutingProtocol Implementation
//-----------------------------------------------------------------------------

NS_OBJECT_ENSURE_REGISTERED(FrtaRoutingProtocol);

TypeId
FrtaRoutingProtocol::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::FrtaRoutingProtocol")
                          .SetParent<Ipv4RoutingProtocol>()
                          .SetGroupName("Internet")
                          .AddConstructor<FrtaRoutingProtocol>();
  return tid;
}

FrtaRoutingProtocol::FrtaRoutingProtocol() 
  : m_ipv4(0),
    m_updateInterval(Seconds(30.0)),
    m_running(false)
{
  NS_LOG_FUNCTION(this);
  m_random = CreateObject<UniformRandomVariable>();
  g_protocolLog << "FrtaRoutingProtocol initialized at " << Simulator::Now().GetSeconds() << "s\n";
}

FrtaRoutingProtocol::~FrtaRoutingProtocol()
{
  NS_LOG_FUNCTION(this);
  g_protocolLog << "FrtaRoutingProtocol destroyed at " << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::DoInitialize()
{
  NS_LOG_FUNCTION(this);
  if (m_ipv4)
  {
    InitializeRoutingTable();
  }
  Ipv4RoutingProtocol::DoInitialize();
}

void
FrtaRoutingProtocol::DoDispose()
{
  NS_LOG_FUNCTION(this);
  m_ipv4 = 0;
  if (m_socket)
  {
    m_socket->Close();
    m_socket = 0;
  }
  m_routingTable.clear();
  m_trustValues.clear();
  m_packetCounts.clear();
  Ipv4RoutingProtocol::DoDispose();
}

void
FrtaRoutingProtocol::Start()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
  {
    m_running = true;
    
    // Create socket if not already created
    if (!m_socket)
    {
      Ptr<Node> node = m_ipv4->GetObject<Node>();
      // Create socket for FRTA protocol
      Ptr<Socket> socket = Socket::CreateSocket(node, TypeId::LookupByName("ns3::UdpSocketFactory"));
      socket->SetAllowBroadcast(true);
      socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9));
      socket->SetRecvCallback(MakeCallback(&FrtaRoutingProtocol::ReceiveRoutingPacket, this));
      m_socket = socket;
      
      g_protocolLog << "Created socket for node " << node->GetId() 
                    << " at " << Simulator::Now().GetSeconds() << "s\n";
    }
    
    // Initialize routes and start periodic updates
    InitializeRoutingTable();
    SendRoutingUpdate();
    
    // Schedule periodic route cache cleanup
    Simulator::Schedule(ROUTE_CACHE_TIMEOUT, &FrtaRoutingProtocol::CleanupRoutingTable, this);
  }
}

void
FrtaRoutingProtocol::Stop()
{
  NS_LOG_FUNCTION(this);
  if (m_running)
  {
    m_running = false;
  }
}

void
FrtaRoutingProtocol::SetUpdateInterval(Time interval)
{
  NS_LOG_FUNCTION(this << interval);
  m_updateInterval = interval;
}

void
FrtaRoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4)
{
  NS_LOG_FUNCTION(this << ipv4);
  NS_ASSERT(ipv4 != nullptr);
  NS_ASSERT(m_ipv4 == nullptr);
  
  m_ipv4 = ipv4;
  
  // Create socket
  Ptr<Node> node = m_ipv4->GetObject<Node>();
  NS_ASSERT(node != nullptr);
  
  m_socket = Socket::CreateSocket(node, TypeId::LookupByName("ns3::UdpSocketFactory"));
  NS_ASSERT(m_socket != nullptr);
  
  m_socket->SetAllowBroadcast(true);
  m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9));
  m_socket->SetRecvCallback(MakeCallback(&FrtaRoutingProtocol::ReceiveRoutingPacket, this));
  
  if (m_running)
  {
    InitializeRoutingTable();
  }
}

Ptr<Ipv4Route>
FrtaRoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                                Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION(this << header.GetDestination());
  
  Ipv4Address destination = header.GetDestination();
  
  // If this is a broadcast packet, create a route for it
  if (destination.IsBroadcast())
  {
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(destination);
    route->SetGateway(Ipv4Address::GetZero());
    route->SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
    route->SetOutputDevice(m_ipv4->GetNetDevice(1));
    return route;
  }
  
  // Check if we have a route in cache
  auto it = m_routeCache.find(destination);
  if (it != m_routeCache.end() &&
      Simulator::Now() - it->second.lastUpdate < ROUTE_CACHE_TIMEOUT)
  {
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(destination);
    route->SetGateway(it->second.nextHop);
    route->SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
    route->SetOutputDevice(m_ipv4->GetNetDevice(1));
    
    sockerr = Socket::ERROR_NOTERROR;
    return route;
  }
  
  // No route found, initiate route discovery
  if (m_pendingRequests.find(destination) == m_pendingRequests.end())
  {
    SendRouteRequest(destination);
    g_protocolLog << "Initiating route discovery for " << destination
                  << " at " << Simulator::Now().GetSeconds() << "s\n";
  }
  
  sockerr = Socket::ERROR_NOROUTETOHOST;
  return nullptr;
}

bool
FrtaRoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header,
                               Ptr<const NetDevice> idev, const UnicastForwardCallback &ucb,
                               const MulticastForwardCallback &mcb,
                               const LocalDeliverCallback &lcb, const ErrorCallback &ecb)
{
  NS_LOG_FUNCTION(this << header.GetDestination());
  
  // Don't forward broadcast packets
  if (header.GetDestination().IsBroadcast())
  {
    lcb(p, header, idev->GetIfIndex());
    return true;
  }
  
  // Check if packet is destined for this node
  if (m_ipv4->IsDestinationAddress(header.GetDestination(), idev->GetIfIndex()))
  {
    lcb(p, header, idev->GetIfIndex());
    return true;
  }
  
  // Forward packet if we have a route
  auto it = m_routeCache.find(header.GetDestination());
  if (it != m_routeCache.end() &&
      Simulator::Now() - it->second.lastUpdate < ROUTE_CACHE_TIMEOUT)
  {
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(header.GetDestination());
    route->SetGateway(it->second.nextHop);
    route->SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
    route->SetOutputDevice(m_ipv4->GetNetDevice(1));
    
    ucb(route, p, header);
    return true;
  }
  
  // No route found
  return false;
}

void
FrtaRoutingProtocol::InitializeRoutingTable()
{
  NS_LOG_FUNCTION(this);
  NS_ASSERT(m_ipv4 != nullptr);
  
  // Initialize routing table
  for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++)
  {
    if (m_ipv4->GetNAddresses(i) == 0)
    {
      continue;
    }
    
    Ipv4InterfaceAddress addr = m_ipv4->GetAddress(i, 0);
    if (addr.GetLocal() == Ipv4Address("127.0.0.1"))
    {
      continue;
    }
    
    // Create route for this interface
    Ptr<Ipv4Route> route = Create<Ipv4Route>();
    route->SetDestination(addr.GetLocal());
    route->SetSource(addr.GetLocal());
    route->SetGateway(addr.GetLocal());
    route->SetOutputDevice(m_ipv4->GetNetDevice(i));
    m_routingTable[addr.GetLocal()] = route;
    
    // Initialize trust values
    m_trustValues[addr.GetLocal()] = 1.0;
    m_packetCounts[addr.GetLocal()] = 0;
    
    // Initialize route cache
    RouteEntry entry;
    entry.nextHop = addr.GetLocal();
    entry.trust = 1.0;
    entry.lastUpdate = Simulator::Now();
    entry.hopCount = 0;
    m_routeCache[addr.GetLocal()] = entry;
    
    g_protocolLog << "InitializeRoutingTable: Added route for " << addr.GetLocal()
                  << " on interface " << i << " at " << Simulator::Now().GetSeconds() << "s\n";
  }
  
  if (m_running)
  {
    Simulator::Schedule(m_updateInterval, &FrtaRoutingProtocol::SendRoutingUpdate, this);
    Simulator::Schedule(m_updateInterval, &FrtaRoutingProtocol::BroadcastRouteAdvertisement, this);
  }
}

Ptr<Ipv4Route>
FrtaRoutingProtocol::SelectOptimalPath(Ipv4Address destination)
{
  NS_LOG_FUNCTION(this << destination);
  
  // Check if we have a direct route
  auto it = m_routeCache.find(destination);
  if (it != m_routeCache.end())
  {
    if (it->second.trust > 0.5 && 
        Simulator::Now() - it->second.lastUpdate < ROUTE_CACHE_TIMEOUT)
    {
      Ptr<Ipv4Route> route = Create<Ipv4Route>();
      route->SetDestination(destination);
      route->SetSource(m_ipv4->GetAddress(0, 0).GetLocal());
      route->SetGateway(it->second.nextHop);
      route->SetOutputDevice(m_ipv4->GetNetDevice(0));
      
      g_protocolLog << "Selected optimal path to " << destination
                    << " via " << it->second.nextHop << " (trust: " << it->second.trust
                    << ", hops: " << it->second.hopCount << ") at " 
                    << Simulator::Now().GetSeconds() << "s\n";
      return route;
    }
  }
  
  // If no route found, initiate route discovery
  if (m_pendingRequests.find(destination) == m_pendingRequests.end())
  {
    SendRouteRequest(destination);
  }
  
  g_protocolLog << "No optimal path to " << destination
                << " at " << Simulator::Now().GetSeconds() << "s\n";
  return nullptr;
}

void
FrtaRoutingProtocol::SendRouteRequest(Ipv4Address destination)
{
  NS_LOG_FUNCTION(this << destination);
  NS_ASSERT(m_socket != nullptr);
  
  // Create packet
  Ptr<Packet> packet = Create<Packet>();
  
  // Add route request header first
  RouteRequestHeader reqHeader;
  reqHeader.SetDestination(destination);
  reqHeader.SetSource(m_ipv4->GetAddress(1, 0).GetLocal());
  reqHeader.SetHopCount(0);
  packet->AddHeader(reqHeader);
  
  // Add FRTA header last (will be first when receiving)
  FrtaHeader frtaHeader;
  frtaHeader.SetMessageType(FrtaHeader::FRTA_ROUTE_REQUEST);
  packet->AddHeader(frtaHeader);
  
  // Add to pending requests
  m_pendingRequests.insert(destination);
  m_routeRequestTime[destination] = Simulator::Now();
  
  g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                << " broadcasting route request for " << destination
                << " at " << Simulator::Now().GetSeconds() << "s\n";
  
  // Broadcast the request
  m_socket->SendTo(packet, 0, InetSocketAddress(Ipv4Address::GetBroadcast(), 9));
  
  // Schedule timeout
  Simulator::Schedule(ROUTE_REQUEST_TIMEOUT, &FrtaRoutingProtocol::HandleRouteRequestTimeout,
                     this, destination);
}

void
FrtaRoutingProtocol::ProcessRouteRequest(Ptr<Packet> packet, Ipv4Address sender)
{
  NS_LOG_FUNCTION(this << sender);
  NS_ASSERT(m_socket != nullptr);
  
  // Remove FRTA header first
  FrtaHeader frtaHeader;
  packet->RemoveHeader(frtaHeader);
  
  // Verify this is actually a route request
  if (frtaHeader.GetMessageType() != FrtaHeader::FRTA_ROUTE_REQUEST)
  {
    g_protocolLog << "ProcessRouteRequest received wrong packet type: " 
                  << (int)frtaHeader.GetMessageType() << " at "
                  << Simulator::Now().GetSeconds() << "s\n";
    return;
  }
  
  // Get route request header
  RouteRequestHeader reqHeader;
  packet->RemoveHeader(reqHeader);
  
  Ipv4Address destination = reqHeader.GetDestination();
  Ipv4Address source = reqHeader.GetSource();
  uint32_t hopCount = reqHeader.GetHopCount();
  
  g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                << " processing route request from " << sender
                << " for destination " << destination
                << " (hop count: " << hopCount << ") at "
                << Simulator::Now().GetSeconds() << "s\n";
  
  // Avoid processing our own requests or requests we've seen before
  if (source == m_ipv4->GetAddress(1, 0).GetLocal())
  {
    g_protocolLog << "Ignoring own request at " << Simulator::Now().GetSeconds() << "s\n";
    return;
  }
  
  // Create reverse route to source
  RouteEntry sourceEntry;
  sourceEntry.nextHop = sender;
  sourceEntry.trust = 0.7;
  sourceEntry.lastUpdate = Simulator::Now();
  sourceEntry.hopCount = hopCount + 1;
  m_routeCache[source] = sourceEntry;
  
  // Update trust for the sender
  UpdateTrustValue(sender, 0.7);
  
  // Check if we are the destination
  if (destination == m_ipv4->GetAddress(1, 0).GetLocal())
  {
    g_protocolLog << "We are destination, sending reply to " << source
                  << " via " << sender << " at " << Simulator::Now().GetSeconds() << "s\n";
    SendRouteReply(source, sender);
    return;
  }
  
  // Check if we have a valid route to destination
  auto it = m_routeCache.find(destination);
  if (it != m_routeCache.end() &&
      Simulator::Now() - it->second.lastUpdate < ROUTE_CACHE_TIMEOUT)
  {
    g_protocolLog << "Found route to " << destination
                  << " via " << it->second.nextHop << ", sending reply to " << source
                  << " at " << Simulator::Now().GetSeconds() << "s\n";
    SendRouteReply(source, sender);
    return;
  }
  
  // Forward the route request if hop count is within limit
  if (hopCount < MAX_HOP_COUNT)
  {
    // Add small random delay to avoid collisions
    Time delay = MicroSeconds(m_random->GetInteger(0, 1000));
    
    // Create new packet with updated hop count
    Ptr<Packet> forwardPacket = Create<Packet>();
    FrtaHeader newFrtaHeader;
    newFrtaHeader.SetMessageType(FrtaHeader::FRTA_ROUTE_REQUEST);
    forwardPacket->AddHeader(newFrtaHeader);
    
    reqHeader.SetHopCount(hopCount + 1);
    forwardPacket->AddHeader(reqHeader);
    
    g_protocolLog << "Forwarding request for " << destination
                  << " (hop count: " << (hopCount + 1) << ") with delay " 
                  << delay.GetMicroSeconds() << "us at "
                  << Simulator::Now().GetSeconds() << "s\n";
    
    Simulator::Schedule(delay, &FrtaRoutingProtocol::ForwardRouteRequest, this, forwardPacket);
  }
}

void
FrtaRoutingProtocol::ForwardRouteRequest(Ptr<Packet> packet)
{
  NS_LOG_FUNCTION(this);
  
  // Get packet details for logging
  RouteRequestHeader reqHeader;
  packet->PeekHeader(reqHeader);
  
  g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                << " forwarding route request to destination " << reqHeader.GetDestination()
                << " at " << Simulator::Now().GetSeconds() << "s\n";
  
  // Send to broadcast address
  m_socket->SendTo(packet, 0, InetSocketAddress(Ipv4Address::GetBroadcast(), 9));
}

void
FrtaRoutingProtocol::SendRouteReply(Ipv4Address destination, Ipv4Address nextHop)
{
  NS_LOG_FUNCTION(this << destination << nextHop);
  
  // Create packet with FRTA header
  Ptr<Packet> packet = Create<Packet>();
  FrtaHeader frtaHeader;
  frtaHeader.SetMessageType(FrtaHeader::FRTA_ROUTE_REPLY);
  packet->AddHeader(frtaHeader);
  
  // Add route reply header
  RouteReplyHeader replyHeader;
  replyHeader.SetDestination(destination);
  replyHeader.SetNextHop(nextHop);
  replyHeader.SetTrust(m_trustValues[nextHop]);
  packet->AddHeader(replyHeader);
  
  // Add small random delay to avoid collisions
  Time delay = MicroSeconds(m_random->GetInteger(0, 1000));
  
  g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                << " scheduling route reply to " << destination
                << " via " << nextHop << " with delay "
                << delay.GetMicroSeconds() << "us at " 
                << Simulator::Now().GetSeconds() << "s\n";
  
  Simulator::Schedule(delay, &FrtaRoutingProtocol::SendDelayedReply, this, packet, nextHop);
}

void
FrtaRoutingProtocol::SendDelayedReply(Ptr<Packet> packet, Ipv4Address nextHop)
{
  NS_LOG_FUNCTION(this << nextHop);
  m_socket->SendTo(packet, 0, InetSocketAddress(nextHop, 9));
}

void
FrtaRoutingProtocol::ProcessRouteReply(Ptr<Packet> packet, Ipv4Address sender)
{
  NS_LOG_FUNCTION(this << sender);
  
  // Get FRTA header
  FrtaHeader frtaHeader;
  packet->RemoveHeader(frtaHeader);
  
  // Get route reply header
  RouteReplyHeader replyHeader;
  packet->RemoveHeader(replyHeader);
  
  Ipv4Address destination = replyHeader.GetDestination();
  Ipv4Address nextHop = replyHeader.GetNextHop();
  double trust = replyHeader.GetTrust();
  
  g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                << " processing route reply from " << sender
                << " for destination " << destination
                << " via " << nextHop << " at "
                << Simulator::Now().GetSeconds() << "s\n";
  
  // Update trust values with received trust information
  UpdateTrustValue(sender, trust);
  UpdateTrustValue(nextHop, trust);
  
  // Update route cache with new route
  RouteEntry entry;
  entry.nextHop = sender;  // Use sender as next hop
  entry.trust = trust;
  entry.lastUpdate = Simulator::Now();
  entry.hopCount = 1;  // Direct hop to next node
  m_routeCache[destination] = entry;
  
  g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                << " updated route cache for " << destination
                << " via " << sender << " (trust: " << trust << ")"
                << " at " << Simulator::Now().GetSeconds() << "s\n";
  
  // If we're not the final destination, forward the reply
  if (destination != m_ipv4->GetAddress(1, 0).GetLocal())
  {
    auto it = m_routeCache.find(destination);
    if (it != m_routeCache.end() && it->second.nextHop != destination)
    {
      g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                    << " forwarding reply to " << destination
                    << " via " << it->second.nextHop
                    << " at " << Simulator::Now().GetSeconds() << "s\n";
      SendRouteReply(destination, it->second.nextHop);
    }
  }
  
  // Remove from pending requests if this was our request
  m_pendingRequests.erase(destination);
}

void
FrtaRoutingProtocol::UpdateRoute(Ipv4Address destination, Ipv4Address nextHop, double trust)
{
  NS_LOG_FUNCTION(this << destination << nextHop << trust);
  
  RouteEntry entry;
  entry.nextHop = nextHop;
  entry.trust = trust;
  entry.lastUpdate = Simulator::Now();
  entry.hopCount = 1; // Direct hop
  
  m_routeCache[destination] = entry;
  
  // Update trust value for next hop
  UpdateTrustValue(nextHop, trust);
  
  g_protocolLog << "UpdateRoute: Updated route to " << destination
                << " via " << nextHop << " (trust: " << trust
                << ") at " << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::BroadcastRouteAdvertisement()
{
  NS_LOG_FUNCTION(this);
  
  for (const auto& entry : m_routeCache)
  {
    if (entry.second.trust > 0.5 && 
        Simulator::Now() - entry.second.lastUpdate < ROUTE_CACHE_TIMEOUT)
    {
      Ptr<Packet> packet = Create<Packet>();
      FrtaHeader frtaHeader;
      frtaHeader.SetMessageType(FrtaHeader::FRTA_ROUTE_ADVERTISEMENT);
      packet->AddHeader(frtaHeader);
      
      RouteAdvertisementHeader advHeader;
      advHeader.SetDestination(entry.first);
      advHeader.SetNextHop(entry.second.nextHop);
      advHeader.SetTrust(entry.second.trust);
      advHeader.SetHopCount(entry.second.hopCount);
      packet->AddHeader(advHeader);
      
      m_socket->SendTo(packet, 0, InetSocketAddress(Ipv4Address::GetBroadcast(), 9));
      
      g_protocolLog << "Broadcasted route advertisement for " << entry.first
                    << " via " << entry.second.nextHop << " at " 
                    << Simulator::Now().GetSeconds() << "s\n";
    }
  }
  
  Simulator::Schedule(m_updateInterval, &FrtaRoutingProtocol::BroadcastRouteAdvertisement, this);
}

void
FrtaRoutingProtocol::ProcessRouteAdvertisement(Ptr<Packet> packet, Ipv4Address sender)
{
  NS_LOG_FUNCTION(this << sender);
  
  // Get FRTA header
  FrtaHeader frtaHeader;
  packet->RemoveHeader(frtaHeader);
  
  // Get route advertisement header
  RouteAdvertisementHeader advHeader;
  packet->RemoveHeader(advHeader);
  
  Ipv4Address destination = advHeader.GetDestination();
  Ipv4Address nextHop = advHeader.GetNextHop();
  double trust = advHeader.GetTrust();
  uint32_t hopCount = advHeader.GetHopCount();
  
  // Update route if it's better than existing one
  auto it = m_routeCache.find(destination);
  if (it == m_routeCache.end() || 
      (trust > it->second.trust && hopCount < it->second.hopCount))
  {
    RouteEntry entry;
    entry.nextHop = nextHop;
    entry.trust = trust;
    entry.lastUpdate = Simulator::Now();
    entry.hopCount = hopCount + 1;
    m_routeCache[destination] = entry;
    
    g_protocolLog << "Updated route from advertisement: " << destination
                  << " via " << nextHop << " (trust: " << trust
                  << ", hops: " << entry.hopCount << ") at " 
                  << Simulator::Now().GetSeconds() << "s\n";
  }
}

void
FrtaRoutingProtocol::HandleRouteRequestTimeout(Ipv4Address destination)
{
  NS_LOG_FUNCTION(this << destination);
  
  if (m_pendingRequests.find(destination) != m_pendingRequests.end())
  {
    g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                  << " route request timeout for " << destination
                  << " at " << Simulator::Now().GetSeconds() << "s\n"
                  << "  Pending requests: " << m_pendingRequests.size()
                  << ", Route cache entries: " << m_routeCache.size() << "\n";
    
    m_pendingRequests.erase(destination);
    m_routeRequestTime.erase(destination);
  }
}

void
FrtaRoutingProtocol::UpdateTrustValue(Ipv4Address node, double trust)
{
  NS_LOG_FUNCTION(this << node << trust);
  
  // Get current trust value or default to 0.5
  auto it = m_trustValues.find(node);
  double currentTrust = (it != m_trustValues.end()) ? it->second : 0.5;
  
  // Weighted average of current and new trust values
  double alpha = 0.7;  // Weight for new trust value
  double newTrust = (alpha * trust) + ((1 - alpha) * currentTrust);
  
  // Ensure trust stays within bounds
  m_trustValues[node] = std::max(0.1, std::min(1.0, newTrust));
  
  g_protocolLog << "Updated trust for " << node << " from " << currentTrust 
                << " to " << m_trustValues[node] << " at " << Simulator::Now().GetSeconds() << "s\n";
}

double
FrtaRoutingProtocol::CalculateTrustValue(Ipv4Address node)
{
  NS_LOG_FUNCTION(this << node);
  auto it = m_packetCounts.find(node);
  double trust = (it != m_packetCounts.end()) ? 1.0 - (it->second / 100.0) : 0.5;
  g_protocolLog << "Calculated trust for " << node << " as " << trust
                << " at " << Simulator::Now().GetSeconds() << "s\n";
  return trust;
}

void
FrtaRoutingProtocol::SendRoutingUpdate()
{
  NS_LOG_FUNCTION(this);
  if (!m_running)
  {
    return;
  }
  
  for (const auto& entry : m_routingTable)
  {
    Ptr<Packet> packet = Create<Packet>();
    TrustTag trustTag;
    trustTag.SetTrust(m_trustValues[entry.first]);
    packet->AddPacketTag(trustTag);
    
    m_socket->SendTo(packet, 0, InetSocketAddress(Ipv4Address::GetBroadcast(), 9));
    g_protocolLog << "Sent routing update for " << entry.first
                  << " (trust: " << m_trustValues[entry.first] << ") at " 
                  << Simulator::Now().GetSeconds() << "s\n";
  }
  
  Simulator::Schedule(m_updateInterval, &FrtaRoutingProtocol::SendRoutingUpdate, this);
}

void
FrtaRoutingProtocol::ReceiveRoutingPacket(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION(this);
  Ptr<Packet> packet;
  Address from;
  
  while ((packet = socket->RecvFrom(from)))
  {
    InetSocketAddress inetAddr = InetSocketAddress::ConvertFrom(from);
    Ipv4Address sender = inetAddr.GetIpv4();
    
    // Peek at FRTA header without removing it
    FrtaHeader frtaHeader;
    packet->PeekHeader(frtaHeader);
    
    g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                  << " received packet type " << (int)frtaHeader.GetMessageType()
                  << " from " << sender
                  << " at " << Simulator::Now().GetSeconds() << "s\n";
    
    // Process based on packet type
    switch (frtaHeader.GetMessageType())
    {
      case FrtaHeader::FRTA_ROUTE_REQUEST:
        ProcessRouteRequest(packet, sender);
        break;
      case FrtaHeader::FRTA_ROUTE_REPLY:
        ProcessRouteReply(packet, sender);
        break;
      case FrtaHeader::FRTA_ROUTE_ADVERTISEMENT:
        ProcessRouteAdvertisement(packet, sender);
        break;
      case FrtaHeader::FRTA_TRUST_UPDATE:
      {
        TrustTag trustTag;
        double trust = 0.5;  // Default value
        if (packet->PeekPacketTag(trustTag))
        {
          trust = trustTag.GetTrust();
        }
        UpdateTrustValue(sender, trust);
        g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                      << " received trust update from " << sender
                      << " (trust: " << trust << ") at "
                      << Simulator::Now().GetSeconds() << "s\n";
        break;
      }
      default:
        g_protocolLog << "Node " << m_ipv4->GetObject<Node>()->GetId()
                      << " received unknown packet type " << (int)frtaHeader.GetMessageType()
                      << " from " << sender
                      << " at " << Simulator::Now().GetSeconds() << "s\n";
        break;
    }
  }
}

void
FrtaRoutingProtocol::NotifyInterfaceUp(uint32_t interface)
{
  NS_LOG_FUNCTION(this << interface);
  InitializeRoutingTable();
  g_protocolLog << "Interface " << interface << " up at " << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::NotifyInterfaceDown(uint32_t interface)
{
  NS_LOG_FUNCTION(this << interface);
  g_protocolLog << "Interface " << interface << " down at " << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION(this << interface << address);
  InitializeRoutingTable();
  g_protocolLog << "Added address " << address << " on interface " << interface
                << " at " << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION(this << interface << address);
  g_protocolLog << "Removed address " << address << " on interface " << interface
                << " at " << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
  NS_LOG_FUNCTION(this);
  *stream->GetStream() << "FrtaRoutingProtocol Routing Table\n";
  for (const auto& entry : m_routingTable)
  {
    *stream->GetStream() << "Destination: " << entry.first
                         << ", Route: " << *entry.second
                         << ", Trust: " << m_trustValues.at(entry.first) << "\n";
    g_protocolLog << "Printed route to " << entry.first
                  << " (trust: " << m_trustValues.at(entry.first)
                  << ") at " << Simulator::Now().GetSeconds() << "s\n";
  }
}

bool
FrtaRoutingProtocol::DetectCollision(Ptr<const Packet> packet, Ipv4Address nextHop)
{
  NS_LOG_FUNCTION(this << packet << nextHop);
  
  // Check if the next hop is in our trust values
  auto it = m_trustValues.find(nextHop);
  if (it == m_trustValues.end())
  {
    // For unknown nodes, give them a chance
    m_trustValues[nextHop] = 0.5;
    return false;
  }
  
  // More lenient trust threshold for collision detection
  if (it->second < 0.3)  // Lowered from 0.5
  {
    g_protocolLog << "DetectCollision: Low trust value (" << it->second
                  << ") for " << nextHop << " at " << Simulator::Now().GetSeconds() << "s\n";
    return true;
  }
  
  // More lenient packet count threshold
  auto countIt = m_packetCounts.find(nextHop);
  if (countIt != m_packetCounts.end() && countIt->second > 200)  // Increased from 100
  {
    g_protocolLog << "DetectCollision: High packet count (" << countIt->second
                  << ") for " << nextHop << " at " << Simulator::Now().GetSeconds() << "s\n";
    return true;
  }
  
  g_protocolLog << "DetectCollision: No collision detected for " << nextHop
                << " at " << Simulator::Now().GetSeconds() << "s\n";
  return false;
}

std::vector<std::vector<Ipv4Address>>
FrtaRoutingProtocol::FindAllPaths(Ipv4Address source, Ipv4Address destination)
{
  NS_LOG_FUNCTION(this << source << destination);
  
  // Check cache first
  auto cacheIt = m_cachedPaths.find(destination);
  if (cacheIt != m_cachedPaths.end())
  {
    Time cacheAge = Simulator::Now() - m_routeRequestTime[destination];
    if (cacheAge < ROUTE_CACHE_TIMEOUT)
    {
      return cacheIt->second;
    }
  }
  
  std::vector<std::vector<Ipv4Address>> paths;
  std::vector<Ipv4Address> currentPath;
  std::set<Ipv4Address> visited;
  
  // Helper lambda for DFS path finding
  std::function<void(Ipv4Address)> findPaths = [&](Ipv4Address current) {
    if (paths.size() >= MAX_PATHS)
    {
      return;
    }
    
    currentPath.push_back(current);
    visited.insert(current);
    
    if (current == destination)
    {
      paths.push_back(currentPath);
    }
    else
    {
      // Check neighbors from routing table
      for (const auto& entry : m_routeCache)
      {
        if (visited.find(entry.first) == visited.end())
        {
          findPaths(entry.first);
        }
      }
    }
    
    currentPath.pop_back();
    visited.erase(current);
  };
  
  findPaths(source);
  
  // Cache the found paths
  m_cachedPaths[destination] = paths;
  m_routeRequestTime[destination] = Simulator::Now();
  
  return paths;
}

std::vector<Ipv4Address>
FrtaRoutingProtocol::SelectTrustedPath(Ipv4Address source, Ipv4Address destination)
{
  NS_LOG_FUNCTION(this << source << destination);
  
  // First try direct route if available
  auto directIt = m_routeCache.find(destination);
  if (directIt != m_routeCache.end() && 
      Simulator::Now() - directIt->second.lastUpdate < ROUTE_CACHE_TIMEOUT)
  {
    std::vector<Ipv4Address> directPath;
    directPath.push_back(source);
    directPath.push_back(directIt->second.nextHop);
    directPath.push_back(destination);
    return directPath;
  }
  
  // If no direct route, try finding all paths
  auto paths = FindAllPaths(source, destination);
  if (!paths.empty())
  {
    // Find path with highest minimum trust value
    double bestTrust = -1;
    std::vector<Ipv4Address> bestPath;
    
    for (const auto& path : paths)
    {
      double pathTrust = CalculatePathTrust(path);
      if (pathTrust > bestTrust)
      {
        bestTrust = pathTrust;
        bestPath = path;
      }
    }
    
    if (!bestPath.empty())
    {
      return bestPath;
    }
  }
  
  // If no path found, return empty vector
  return std::vector<Ipv4Address>();
}

bool
FrtaRoutingProtocol::IsPathTrusted(const std::vector<Ipv4Address>& path)
{
  NS_LOG_FUNCTION(this);
  
  if (path.empty())
  {
    return false;
  }
  
  double pathTrust = CalculatePathTrust(path);
  return pathTrust >= MIN_PATH_TRUST;
}

double
FrtaRoutingProtocol::CalculatePathTrust(const std::vector<Ipv4Address>& path)
{
  NS_LOG_FUNCTION(this);
  
  if (path.empty())
  {
    return 0.0;
  }
  
  // Check if we have a cached trust value
  auto it = m_pathTrustValues.find(path);
  if (it != m_pathTrustValues.end())
  {
    return it->second;
  }
  
  // Calculate trust as minimum of node trust values along path
  double minTrust = 1.0;
  for (const auto& node : path)
  {
    auto trustIt = m_trustValues.find(node);
    if (trustIt != m_trustValues.end())
    {
      minTrust = std::min(minTrust, trustIt->second);
    }
    else
    {
      minTrust = std::min(minTrust, 0.5); // Default trust for unknown nodes
    }
  }
  
  // Cache the calculated trust value
  m_pathTrustValues[path] = minTrust;
  return minTrust;
}

void
FrtaRoutingProtocol::UpdatePathTrust(const std::vector<Ipv4Address>& path, bool success)
{
  NS_LOG_FUNCTION(this << success);
  
  if (path.empty())
  {
    return;
  }
  
  // Update trust values for all nodes in the path
  for (const auto& node : path)
  {
    auto& trust = m_trustValues[node];
    if (success)
    {
      trust = std::min(1.0, trust + 0.1);
    }
    else
    {
      trust = std::max(0.0, trust - 0.2);
    }
    
    // Update collision statistics for each node
    m_collisionDetector.UpdateTransmissionStats(node, success);
  }
  
  // Update path trust value
  double newTrust = CalculatePathTrust(path);
  m_pathTrustValues[path] = newTrust;
  
  g_protocolLog << "Updated path trust: " << newTrust
                << " (success: " << success << ") at "
                << Simulator::Now().GetSeconds() << "s\n";
}

void
FrtaRoutingProtocol::CleanupRoutingTable()
{
  NS_LOG_FUNCTION(this);
  
  Time now = Simulator::Now();
  std::vector<Ipv4Address> toRemove;
  
  // Find expired routes
  for (const auto& entry : m_routeCache)
  {
    if (now - entry.second.lastUpdate >= ROUTE_CACHE_TIMEOUT)
    {
      toRemove.push_back(entry.first);
    }
  }
  
  // Remove expired routes
  for (const auto& addr : toRemove)
  {
    m_routeCache.erase(addr);
    g_protocolLog << "Removed expired route to " << addr 
                  << " at " << now.GetSeconds() << "s\n";
  }
  
  // Schedule next cleanup
  Simulator::Schedule(ROUTE_CACHE_TIMEOUT, &FrtaRoutingProtocol::CleanupRoutingTable, this);
}

} // namespace ns3