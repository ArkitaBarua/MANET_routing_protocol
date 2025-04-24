#include "frta-state.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FrtaState");

NS_OBJECT_ENSURE_REGISTERED(FrtaState);

TypeId
FrtaState::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::FrtaState")
    .SetParent<Object>()
    .SetGroupName("Internet")
    .AddConstructor<FrtaState>();
  return tid;
}

FrtaState::FrtaState()
  : m_lastUpdate(Simulator::Now())
{
  NS_LOG_FUNCTION(this);
}

FrtaState::~FrtaState()
{
  NS_LOG_FUNCTION(this);
  Clear();
}

void
FrtaState::AddRoute(Ipv4Address destination, const RouteEntry& entry)
{
  NS_LOG_FUNCTION(this << destination);
  m_routes[destination] = entry;
}

void
FrtaState::RemoveRoute(Ipv4Address destination)
{
  NS_LOG_FUNCTION(this << destination);
  m_routes.erase(destination);
}

const FrtaState::RouteEntry*
FrtaState::GetRoute(Ipv4Address destination) const
{
  NS_LOG_FUNCTION(this << destination);
  auto it = m_routes.find(destination);
  if (it != m_routes.end())
  {
    return &(it->second);
  }
  return nullptr;
}

void
FrtaState::UpdateTrust(Ipv4Address node, double trust)
{
  NS_LOG_FUNCTION(this << node << trust);
  m_trustValues[node] = std::max(0.0, std::min(1.0, trust));
}

double
FrtaState::GetTrust(Ipv4Address node) const
{
  NS_LOG_FUNCTION(this << node);
  auto it = m_trustValues.find(node);
  return (it != m_trustValues.end()) ? it->second : 0.5;
}

void
FrtaState::Clear()
{
  NS_LOG_FUNCTION(this);
  m_routes.clear();
  m_trustValues.clear();
  m_nodeStates.clear();
}

void
FrtaState::UpdateNodeState(Ipv4Address node, bool active)
{
  NS_LOG_FUNCTION(this << node << active);
  m_nodeStates[node] = active;
  m_lastUpdate = Simulator::Now();
}

bool
FrtaState::IsNodeActive(Ipv4Address node) const
{
  NS_LOG_FUNCTION(this << node);
  auto it = m_nodeStates.find(node);
  return (it != m_nodeStates.end()) ? it->second : false;
}

std::vector<Ipv4Address>
FrtaState::GetActiveNodes() const
{
  NS_LOG_FUNCTION(this);
  std::vector<Ipv4Address> activeNodes;
  for (const auto& pair : m_nodeStates)
  {
    if (pair.second)
    {
      activeNodes.push_back(pair.first);
    }
  }
  return activeNodes;
}

} // namespace ns3 