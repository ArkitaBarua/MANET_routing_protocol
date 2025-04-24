#ifndef FRTA_STATE_H
#define FRTA_STATE_H

#include "ns3/object.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include <map>
#include <vector>

namespace ns3 {

/**
 * \brief State management for FRTA routing protocol
 *
 * This class maintains the state information needed by the FRTA routing protocol,
 * including route cache, trust values, and network statistics.
 */
class FrtaState : public Object
{
public:
  /**
   * \brief Get the TypeId
   * \return the TypeId for this class
   */
  static TypeId GetTypeId(void);

  /**
   * \brief Constructor
   */
  FrtaState();

  /**
   * \brief Destructor
   */
  virtual ~FrtaState();

  /**
   * \brief Route entry information
   */
  struct RouteEntry
  {
    Ipv4Address nextHop;     //!< Next hop to destination
    double trust;            //!< Trust value for this route
    Time lastUpdate;         //!< Last time this entry was updated
    uint32_t hopCount;       //!< Number of hops to destination
    bool isValid;            //!< Whether this route is currently valid
  };

  /**
   * \brief Add or update a route entry
   * \param destination The destination address
   * \param entry The route entry
   */
  void AddRoute(Ipv4Address destination, const RouteEntry& entry);

  /**
   * \brief Remove a route entry
   * \param destination The destination address
   */
  void RemoveRoute(Ipv4Address destination);

  /**
   * \brief Get a route entry
   * \param destination The destination address
   * \return The route entry if found, nullptr otherwise
   */
  const RouteEntry* GetRoute(Ipv4Address destination) const;

  /**
   * \brief Update trust value for a node
   * \param node The node address
   * \param trust The new trust value
   */
  void UpdateTrust(Ipv4Address node, double trust);

  /**
   * \brief Get trust value for a node
   * \param node The node address
   * \return The trust value
   */
  double GetTrust(Ipv4Address node) const;

  /**
   * \brief Clear all state information
   */
  void Clear();

  /**
   * \brief Update node state
   * \param node Node address
   * \param active Whether the node is active
   */
  void UpdateNodeState(Ipv4Address node, bool active);

  /**
   * \brief Check if a node is active
   * \param node Node address
   * \return true if the node is active
   */
  bool IsNodeActive(Ipv4Address node) const;

  /**
   * \brief Get active nodes
   * \return Vector of active node addresses
   */
  std::vector<Ipv4Address> GetActiveNodes() const;

private:
  std::map<Ipv4Address, RouteEntry> m_routes;        //!< Route entries
  std::map<Ipv4Address, double> m_trustValues;       //!< Trust values for nodes
  std::map<Ipv4Address, bool> m_nodeStates; //!< Map of node states (active/inactive)
  Time m_lastUpdate;                         //!< Time of last state update
};

} // namespace ns3

#endif /* FRTA_STATE_H */ 