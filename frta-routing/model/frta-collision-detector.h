#ifndef FRTA_COLLISION_DETECTOR_H
#define FRTA_COLLISION_DETECTOR_H

#include "ns3/ipv4-address.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include <vector>
#include <map>

namespace ns3 {

/**
 * \brief Collision detection and path optimization for FRTA routing protocol
 *
 * This class implements collision detection and path optimization algorithms
 * for the FRTA (Fault-Resistant Traffic-Aware) routing protocol. It maintains
 * statistics about transmission successes and failures to estimate collision
 * probabilities and determine optimal routing paths.
 */
class FrtaCollisionDetector : public Object
{
public:
  /**
   * \brief Constructor
   */
  FrtaCollisionDetector();
  virtual ~FrtaCollisionDetector();

  /**
   * \brief Get the optimal path from available paths based on collision probability
   * \param paths Vector of available paths, where each path is a vector of IPv4 addresses
   * \return The optimal path with minimum collision probability
   */
  std::vector<Ipv4Address> GetOptimalPath(const std::vector<std::vector<Ipv4Address>>& paths);

  /**
   * \brief Update transmission statistics
   * \param sender The sender's address
   * \param success Whether the transmission was successful
   */
  void UpdateTransmissionStats(Ipv4Address sender, bool success);

  /**
   * \brief Get the current collision probability based on historical data
   * \return Collision probability as a value between 0 and 1
   */
  double GetCollisionProbability();

  /**
   * \brief Detect potential collision for a packet transmission
   * \param packet The packet to be transmitted
   * \param sender The sender's address
   * \param receiver The receiver's address
   * \return True if collision is likely, false otherwise
   */
  bool DetectPotentialCollision(Ptr<const Packet> packet,
                               Ipv4Address sender,
                               Ipv4Address receiver);

private:
  /**
   * \brief Calculate collision probability for a specific path
   * \param path Vector of IPv4 addresses representing the path
   * \return Collision probability for the path
   */
  double CalculatePathCollisionProbability(const std::vector<Ipv4Address>& path);

  double m_collisionProbabilityCache; //!< Cached collision probability
  bool m_cacheValid;                  //!< Whether the cache is valid
  uint32_t m_successCount;            //!< Number of successful transmissions
  uint32_t m_totalCount;              //!< Total number of transmission attempts

  // New members for transmission statistics
  struct TransmissionStats {
    Time lastTransmission;
    uint32_t packetCount;
    double collisionProbability;
  };
  std::map<Ipv4Address, TransmissionStats> m_transmissionStats;
  std::map<std::pair<Ipv4Address, Ipv4Address>, uint32_t> m_collisionCounts;
};

} // namespace ns3

#endif /* FRTA_COLLISION_DETECTOR_H */