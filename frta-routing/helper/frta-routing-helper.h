#ifndef FRTA_ROUTING_HELPER_H
#define FRTA_ROUTING_HELPER_H

#include "ns3/ipv4-routing-helper.h"
#include "ns3/frta-routing-protocol.h"

namespace ns3 {

/**
 * \brief Helper class that adds ns3::FrtaRoutingProtocol objects
 */
class FrtaRoutingHelper : public Ipv4RoutingHelper
{
public:
  FrtaRoutingHelper();
  FrtaRoutingHelper(const FrtaRoutingHelper &);
  
  /**
   * \returns pointer to clone of this FrtaRoutingHelper
   */
  FrtaRoutingHelper* Copy() const;
  
  /**
   * \param node the node on which the routing protocol will run
   * \returns a newly-created routing protocol
   */
  virtual Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const;
  
  /**
   * \param updateInterval the interval between periodic updates
   */
  void SetUpdateInterval(Time interval);

private:
  Time m_updateInterval;
};

} // namespace ns3

#endif // FRTA_ROUTING_HELPER_H
