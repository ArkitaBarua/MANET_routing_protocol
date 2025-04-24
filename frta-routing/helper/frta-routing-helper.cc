#include "frta-routing-helper.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/udp-l4-protocol.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FrtaRoutingHelper");

FrtaRoutingHelper::FrtaRoutingHelper() : m_updateInterval(Seconds(30.0))
{
  NS_LOG_FUNCTION(this);
}

FrtaRoutingHelper::FrtaRoutingHelper(const FrtaRoutingHelper &o)
  : m_updateInterval(o.m_updateInterval)
{
  NS_LOG_FUNCTION(this);
}

FrtaRoutingHelper* 
FrtaRoutingHelper::Copy() const
{
  NS_LOG_FUNCTION(this);
  return new FrtaRoutingHelper(*this);
}

Ptr<Ipv4RoutingProtocol>
FrtaRoutingHelper::Create(Ptr<Node> node) const
{
  NS_LOG_FUNCTION(this << node);
  
  // Ensure UDP protocol is installed
  Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol>();
  if (!udp)
  {
    udp = CreateObject<UdpL4Protocol>();
    node->AggregateObject(udp);
  }
  
  Ptr<FrtaRoutingProtocol> protocol = CreateObject<FrtaRoutingProtocol>();
  protocol->SetUpdateInterval(m_updateInterval);
  
  node->AggregateObject(protocol);
  return protocol;
}

void
FrtaRoutingHelper::SetUpdateInterval(Time interval)
{
  NS_LOG_FUNCTION(this << interval);
  m_updateInterval = interval;
}

} // namespace ns3