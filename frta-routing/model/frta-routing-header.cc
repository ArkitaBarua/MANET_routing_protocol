#include "frta-routing-header.h"
#include "ns3/log.h"
#include "ns3/address-utils.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FrtaRoutingHeader");

//-----------------------------------------------------------------------------
// FrtaHeader
//-----------------------------------------------------------------------------

FrtaHeader::FrtaHeader() : m_type(FRTA_ROUTE_REQUEST)
{
}

FrtaHeader::~FrtaHeader()
{
}

TypeId
FrtaHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::FrtaHeader")
    .SetParent<Header>()
    .SetGroupName("FrtaRouting")
    .AddConstructor<FrtaHeader>();
  return tid;
}

TypeId
FrtaHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

void
FrtaHeader::Print(std::ostream &os) const
{
  os << "MessageType=" << (uint32_t)m_type;
}

uint32_t
FrtaHeader::GetSerializedSize(void) const
{
  return sizeof(uint8_t);  // Message type
}

void
FrtaHeader::Serialize(Buffer::Iterator start) const
{
  start.WriteU8((uint8_t)m_type);
}

uint32_t
FrtaHeader::Deserialize(Buffer::Iterator start)
{
  uint8_t type = start.ReadU8();
  if (type >= 1 && type <= 4)  // Valid message types
  {
    m_type = (MessageType)type;
  }
  else
  {
    m_type = FRTA_ROUTE_REQUEST;  // Default to route request if invalid
    NS_LOG_WARN("Invalid message type received: " << (int)type);
  }
  return GetSerializedSize();
}

void
FrtaHeader::SetMessageType(MessageType type)
{
  NS_ASSERT(type >= FRTA_ROUTE_REQUEST && type <= FRTA_TRUST_UPDATE);
  m_type = type;
}

FrtaHeader::MessageType
FrtaHeader::GetMessageType(void) const
{
  return (MessageType)m_type;
}

//-----------------------------------------------------------------------------
// RouteRequestHeader
//-----------------------------------------------------------------------------

RouteRequestHeader::RouteRequestHeader() : m_hopCount(0)
{
}

RouteRequestHeader::~RouteRequestHeader()
{
}

TypeId
RouteRequestHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::RouteRequestHeader")
    .SetParent<Header>()
    .SetGroupName("FrtaRouting")
    .AddConstructor<RouteRequestHeader>();
  return tid;
}

TypeId
RouteRequestHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

void
RouteRequestHeader::Print(std::ostream &os) const
{
  os << "DestAddr=" << m_destination
     << " SrcAddr=" << m_source
     << " HopCount=" << m_hopCount;
}

uint32_t
RouteRequestHeader::GetSerializedSize(void) const
{
  return 8 + 8 + 4;  // Two IPv4 addresses + hop count
}

void
RouteRequestHeader::Serialize(Buffer::Iterator start) const
{
  start.WriteHtonU32(m_destination.Get());
  start.WriteHtonU32(m_source.Get());
  start.WriteHtonU32(m_hopCount);
}

uint32_t
RouteRequestHeader::Deserialize(Buffer::Iterator start)
{
  m_destination.Set(start.ReadNtohU32());
  m_source.Set(start.ReadNtohU32());
  m_hopCount = start.ReadNtohU32();
  return GetSerializedSize();
}

void
RouteRequestHeader::SetDestination(Ipv4Address destination)
{
  m_destination = destination;
}

void
RouteRequestHeader::SetSource(Ipv4Address source)
{
  m_source = source;
}

void
RouteRequestHeader::SetHopCount(uint32_t hopCount)
{
  m_hopCount = hopCount;
}

Ipv4Address
RouteRequestHeader::GetDestination(void) const
{
  return m_destination;
}

Ipv4Address
RouteRequestHeader::GetSource(void) const
{
  return m_source;
}

uint32_t
RouteRequestHeader::GetHopCount(void) const
{
  return m_hopCount;
}

//-----------------------------------------------------------------------------
// RouteReplyHeader
//-----------------------------------------------------------------------------

RouteReplyHeader::RouteReplyHeader() : m_trust(0.0)
{
}

RouteReplyHeader::~RouteReplyHeader()
{
}

TypeId
RouteReplyHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::RouteReplyHeader")
    .SetParent<Header>()
    .SetGroupName("FrtaRouting")
    .AddConstructor<RouteReplyHeader>();
  return tid;
}

TypeId
RouteReplyHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

void
RouteReplyHeader::Print(std::ostream &os) const
{
  os << "DestAddr=" << m_destination
     << " NextHop=" << m_nextHop
     << " Trust=" << m_trust;
}

uint32_t
RouteReplyHeader::GetSerializedSize(void) const
{
  return 8 + 8 + 8;  // Two IPv4 addresses + trust value
}

void
RouteReplyHeader::Serialize(Buffer::Iterator start) const
{
  start.WriteHtonU32(m_destination.Get());
  start.WriteHtonU32(m_nextHop.Get());
  uint64_t trust = *reinterpret_cast<const uint64_t*>(&m_trust);
  start.WriteHtonU64(trust);
}

uint32_t
RouteReplyHeader::Deserialize(Buffer::Iterator start)
{
  m_destination.Set(start.ReadNtohU32());
  m_nextHop.Set(start.ReadNtohU32());
  uint64_t trust = start.ReadNtohU64();
  m_trust = *reinterpret_cast<double*>(&trust);
  return GetSerializedSize();
}

void
RouteReplyHeader::SetDestination(Ipv4Address destination)
{
  m_destination = destination;
}

void
RouteReplyHeader::SetNextHop(Ipv4Address nextHop)
{
  m_nextHop = nextHop;
}

void
RouteReplyHeader::SetTrust(double trust)
{
  m_trust = trust;
}

Ipv4Address
RouteReplyHeader::GetDestination(void) const
{
  return m_destination;
}

Ipv4Address
RouteReplyHeader::GetNextHop(void) const
{
  return m_nextHop;
}

double
RouteReplyHeader::GetTrust(void) const
{
  return m_trust;
}

//-----------------------------------------------------------------------------
// RouteAdvertisementHeader
//-----------------------------------------------------------------------------

RouteAdvertisementHeader::RouteAdvertisementHeader() : m_trust(0.0), m_hopCount(0)
{
}

RouteAdvertisementHeader::~RouteAdvertisementHeader()
{
}

TypeId
RouteAdvertisementHeader::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::RouteAdvertisementHeader")
    .SetParent<Header>()
    .SetGroupName("FrtaRouting")
    .AddConstructor<RouteAdvertisementHeader>();
  return tid;
}

TypeId
RouteAdvertisementHeader::GetInstanceTypeId(void) const
{
  return GetTypeId();
}

void
RouteAdvertisementHeader::Print(std::ostream &os) const
{
  os << "DestAddr=" << m_destination
     << " NextHop=" << m_nextHop
     << " Trust=" << m_trust
     << " HopCount=" << m_hopCount;
}

uint32_t
RouteAdvertisementHeader::GetSerializedSize(void) const
{
  return 8 + 8 + 8 + 4;  // Two IPv4 addresses + trust value + hop count
}

void
RouteAdvertisementHeader::Serialize(Buffer::Iterator start) const
{
  start.WriteHtonU32(m_destination.Get());
  start.WriteHtonU32(m_nextHop.Get());
  uint64_t trust = *reinterpret_cast<const uint64_t*>(&m_trust);
  start.WriteHtonU64(trust);
  start.WriteHtonU32(m_hopCount);
}

uint32_t
RouteAdvertisementHeader::Deserialize(Buffer::Iterator start)
{
  m_destination.Set(start.ReadNtohU32());
  m_nextHop.Set(start.ReadNtohU32());
  uint64_t trust = start.ReadNtohU64();
  m_trust = *reinterpret_cast<double*>(&trust);
  m_hopCount = start.ReadNtohU32();
  return GetSerializedSize();
}

void
RouteAdvertisementHeader::SetDestination(Ipv4Address destination)
{
  m_destination = destination;
}

Ipv4Address
RouteAdvertisementHeader::GetDestination(void) const
{
  return m_destination;
}

void
RouteAdvertisementHeader::SetNextHop(Ipv4Address nextHop)
{
  m_nextHop = nextHop;
}

Ipv4Address
RouteAdvertisementHeader::GetNextHop(void) const
{
  return m_nextHop;
}

void
RouteAdvertisementHeader::SetTrust(double trust)
{
  m_trust = trust;
}

double
RouteAdvertisementHeader::GetTrust(void) const
{
  return m_trust;
}

void
RouteAdvertisementHeader::SetHopCount(uint32_t hopCount)
{
  m_hopCount = hopCount;
}

uint32_t
RouteAdvertisementHeader::GetHopCount(void) const
{
  return m_hopCount;
}

} // namespace ns3 