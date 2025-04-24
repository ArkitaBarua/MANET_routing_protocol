#ifndef FRTA_ROUTING_HEADER_H
#define FRTA_ROUTING_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"

namespace ns3 {

/**
 * \brief Common header for all FRTA routing packets
 */
class FrtaHeader : public Header
{
public:
  enum MessageType {
    FRTA_ROUTE_REQUEST  = 1,
    FRTA_ROUTE_REPLY    = 2,
    FRTA_ROUTE_ADVERTISEMENT = 3,
    FRTA_TRUST_UPDATE   = 4
  };

  FrtaHeader();
  virtual ~FrtaHeader();

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  virtual void Print(std::ostream &os) const;
  virtual uint32_t GetSerializedSize(void) const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);

  void SetMessageType(MessageType type);
  MessageType GetMessageType(void) const;

private:
  uint8_t m_type;  // Message type
};

/**
 * \brief Route Request header
 */
class RouteRequestHeader : public Header
{
public:
  RouteRequestHeader();
  virtual ~RouteRequestHeader();

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  virtual void Print(std::ostream &os) const;
  virtual uint32_t GetSerializedSize(void) const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);

  void SetDestination(Ipv4Address destination);
  void SetSource(Ipv4Address source);
  void SetHopCount(uint32_t hopCount);

  Ipv4Address GetDestination(void) const;
  Ipv4Address GetSource(void) const;
  uint32_t GetHopCount(void) const;

private:
  Ipv4Address m_destination;
  Ipv4Address m_source;
  uint32_t m_hopCount;
};

/**
 * \brief Route Reply header
 */
class RouteReplyHeader : public Header
{
public:
  RouteReplyHeader();
  virtual ~RouteReplyHeader();

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  virtual void Print(std::ostream &os) const;
  virtual uint32_t GetSerializedSize(void) const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);

  void SetDestination(Ipv4Address destination);
  void SetNextHop(Ipv4Address nextHop);
  void SetTrust(double trust);

  Ipv4Address GetDestination(void) const;
  Ipv4Address GetNextHop(void) const;
  double GetTrust(void) const;

private:
  Ipv4Address m_destination;
  Ipv4Address m_nextHop;
  double m_trust;
};

/**
 * \brief Header for Route Advertisement messages
 */
class RouteAdvertisementHeader : public Header
{
public:
  RouteAdvertisementHeader();
  virtual ~RouteAdvertisementHeader();

  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  virtual uint32_t GetSerializedSize(void) const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);
  virtual void Print(std::ostream &os) const;

  void SetDestination(Ipv4Address destination);
  Ipv4Address GetDestination(void) const;
  void SetNextHop(Ipv4Address nextHop);
  Ipv4Address GetNextHop(void) const;
  void SetTrust(double trust);
  double GetTrust(void) const;
  void SetHopCount(uint32_t hopCount);
  uint32_t GetHopCount(void) const;

private:
  Ipv4Address m_destination;
  Ipv4Address m_nextHop;
  double m_trust;
  uint32_t m_hopCount;
};

} // namespace ns3

#endif /* FRTA_ROUTING_HEADER_H */ 