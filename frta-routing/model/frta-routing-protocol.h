#ifndef FRTA_ROUTING_PROTOCOL_H
#define FRTA_ROUTING_PROTOCOL_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-route.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ipv4.h"
#include "ns3/socket.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "frta-routing-header.h"
#include "frta-state.h"
#include "frta-collision-detector.h"
#include <map>
#include <vector>
#include <set>

namespace ns3 {

// Forward declarations
class RouteRequestHeader;
class RouteReplyHeader;
class RouteAdvertisementHeader;

/**
 * \brief Trust data tag for FRTA routing protocol
 */
class TrustTag : public Tag
{
public:
  static TypeId GetTypeId(void);
  virtual TypeId GetInstanceTypeId(void) const;
  
  virtual uint32_t GetSerializedSize(void) const;
  virtual void Serialize(TagBuffer i) const;
  virtual void Deserialize(TagBuffer i);
  virtual void Print(std::ostream &os) const;
  
  // Set/get the trust value
  void SetTrust(double trust);
  double GetTrust(void) const;
  
private:
  double m_trust;
};

/**
 * \brief Route entry structure
 */
struct RouteEntry {
  Ipv4Address nextHop;
  double trust;
  Time lastUpdate;
  uint32_t hopCount;
};

/**
 * \brief Fault-Resilient Trust-Aware (FRTA) Routing Protocol
 */
class FrtaRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  // Packet types
  enum PacketType {
    FRTA_ROUTE_REQUEST  = 1,
    FRTA_ROUTE_REPLY    = 2,
    FRTA_ROUTE_ADVERTISEMENT = 3,
    FRTA_TRUST_UPDATE   = 4
  };

  static TypeId GetTypeId(void);
  FrtaRoutingProtocol();
  virtual ~FrtaRoutingProtocol();

  // Inherited from Ipv4RoutingProtocol
  virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif,
                                    Socket::SocketErrno &sockerr) override;
  virtual bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                        const UnicastForwardCallback &ucb, const MulticastForwardCallback &mcb,
                        const LocalDeliverCallback &lcb, const ErrorCallback &ecb) override;
  virtual void NotifyInterfaceUp(uint32_t interface) override;
  virtual void NotifyInterfaceDown(uint32_t interface) override;
  virtual void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  virtual void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  virtual void SetIpv4(Ptr<Ipv4> ipv4) override;
  virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override;

  // Protocol specific methods
  void Start();
  void Stop();
  void SetUpdateInterval(Time interval);

protected:
  virtual void DoInitialize() override;
  virtual void DoDispose() override;

private:
  void InitializeRoutingTable();
  void SendRoutingUpdate();
  void ReceiveRoutingPacket(Ptr<Socket> socket);
  Ptr<Ipv4Route> SelectOptimalPath(Ipv4Address destination);
  bool DetectCollision(Ptr<const Packet> packet, Ipv4Address nextHop);
  void UpdateTrustValue(Ipv4Address node, double trust);
  double CalculateTrustValue(Ipv4Address node);

  // Route discovery and management
  void SendRouteRequest(Ipv4Address destination);
  void SendRouteReply(Ipv4Address destination, Ipv4Address nextHop);
  void ProcessRouteRequest(Ptr<Packet> packet, Ipv4Address sender);
  void ProcessRouteReply(Ptr<Packet> packet, Ipv4Address sender);
  void UpdateRoute(Ipv4Address destination, Ipv4Address nextHop, double trust);
  void BroadcastRouteAdvertisement();
  void ProcessRouteAdvertisement(Ptr<Packet> packet, Ipv4Address sender);
  void HandleRouteRequestTimeout(Ipv4Address destination);
  
  // Trusted path implementation
  std::vector<std::vector<Ipv4Address>> FindAllPaths(Ipv4Address source, Ipv4Address destination);
  std::vector<Ipv4Address> SelectTrustedPath(Ipv4Address source, Ipv4Address destination);
  bool IsPathTrusted(const std::vector<Ipv4Address>& path);
  double CalculatePathTrust(const std::vector<Ipv4Address>& path);
  void UpdatePathTrust(const std::vector<Ipv4Address>& path, bool success);

  // Additional helper functions
  void CleanupRoutingTable();
  void ForwardRouteRequest(Ptr<Packet> packet);
  void SendDelayedReply(Ptr<Packet> packet, Ipv4Address nextHop);

  // Constants
  static const Time ROUTE_REQUEST_TIMEOUT;
  static const Time ROUTE_CACHE_TIMEOUT;
  static const uint32_t MAX_HOP_COUNT = 10;
  static constexpr double MIN_PATH_TRUST = 0.5;
  static const uint32_t MAX_PATHS = 5;

  // Member variables
  Ptr<Ipv4> m_ipv4;
  Ptr<Socket> m_socket;
  Time m_updateInterval;
  Ptr<UniformRandomVariable> m_random;
  bool m_running;
  
  // State management
  FrtaState m_state;
  std::set<Ipv4Address> m_pendingRequests;
  std::map<Ipv4Address, Time> m_routeRequestTime;
  std::map<Ipv4Address, Ptr<Ipv4Route>> m_routingTable;
  std::map<Ipv4Address, double> m_trustValues;
  std::map<Ipv4Address, uint32_t> m_packetCounts;
  std::map<Ipv4Address, RouteEntry> m_routeCache;
  
  // Collision detection and trusted path management
  FrtaCollisionDetector m_collisionDetector;
  std::map<std::vector<Ipv4Address>, double> m_pathTrustValues;
  std::map<Ipv4Address, std::vector<std::vector<Ipv4Address>>> m_cachedPaths;
};

} // namespace ns3

#endif // FRTA_ROUTING_PROTOCOL_H