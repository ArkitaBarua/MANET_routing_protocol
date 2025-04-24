#include "frta-collision-detector.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <algorithm>
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("FrtaCollisionDetector");

FrtaCollisionDetector::FrtaCollisionDetector()
    : m_collisionProbabilityCache(0.0),
      m_cacheValid(false),
      m_successCount(0),
      m_totalCount(0)
{
  NS_LOG_FUNCTION(this);
}

FrtaCollisionDetector::~FrtaCollisionDetector()
{
  NS_LOG_FUNCTION(this);
}

bool
FrtaCollisionDetector::DetectPotentialCollision(Ptr<const Packet> packet,
                                               Ipv4Address sender,
                                               Ipv4Address receiver)
{
  NS_LOG_FUNCTION(this << sender << receiver);
  
  // Get transmission stats for sender
  auto& senderStats = m_transmissionStats[sender];
  
  // Check if sender has transmitted too frequently
  if (Simulator::Now() - senderStats.lastTransmission < MicroSeconds(100))
  {
    return true;
  }
  
  // Check collision history for this link
  auto linkPair = std::make_pair(sender, receiver);
  auto collisionCount = m_collisionCounts[linkPair];
  
  // If collision count is high, consider it risky
  if (collisionCount > 5)
  {
    return true;
  }
  
  // Check collision probability
  if (senderStats.collisionProbability > 0.5)
  {
    return true;
  }
  
  return false;
}

void
FrtaCollisionDetector::UpdateTransmissionStats(Ipv4Address sender, bool success)
{
  NS_LOG_FUNCTION(this << sender << success);
  
  auto& stats = m_transmissionStats[sender];
  stats.lastTransmission = Simulator::Now();
  stats.packetCount++;
  
  // Update collision probability using exponential moving average
  double alpha = 0.1; // Smoothing factor
  if (success)
  {
    stats.collisionProbability = (1 - alpha) * stats.collisionProbability;
  }
  else
  {
    stats.collisionProbability = alpha + (1 - alpha) * stats.collisionProbability;
  }

  // Update global statistics
  m_totalCount++;
  if (success)
  {
    m_successCount++;
  }
  m_cacheValid = false;
}

std::vector<Ipv4Address>
FrtaCollisionDetector::GetOptimalPath(const std::vector<std::vector<Ipv4Address>>& paths)
{
  NS_LOG_FUNCTION(this);

  if (paths.empty())
  {
    return std::vector<Ipv4Address>();
  }

  std::vector<double> pathProbabilities;
  for (const auto& path : paths)
  {
    pathProbabilities.push_back(CalculatePathCollisionProbability(path));
  }

  // Find the path with minimum collision probability
  auto minProbIt = std::min_element(pathProbabilities.begin(), pathProbabilities.end());
  size_t minIndex = std::distance(pathProbabilities.begin(), minProbIt);

  return paths[minIndex];
}

double
FrtaCollisionDetector::GetCollisionProbability()
{
  NS_LOG_FUNCTION(this);

  if (!m_cacheValid)
  {
    if (m_totalCount == 0)
    {
      m_collisionProbabilityCache = 0.0;
    }
    else
    {
      m_collisionProbabilityCache = 1.0 - (static_cast<double>(m_successCount) / m_totalCount);
    }
    m_cacheValid = true;
  }
  return m_collisionProbabilityCache;
}

double
FrtaCollisionDetector::CalculatePathCollisionProbability(const std::vector<Ipv4Address>& path)
{
  NS_LOG_FUNCTION(this);

  if (path.empty())
  {
    return 1.0; // Empty path has 100% collision probability
  }

  // Basic implementation: probability increases with path length
  // More sophisticated implementations could consider node density, traffic patterns, etc.
  double baseProb = GetCollisionProbability();
  double pathLength = static_cast<double>(path.size());
  
  // Collision probability increases with path length but is capped at 1.0
  return std::min(1.0, baseProb * (1.0 + std::log(pathLength)));
}

} // namespace ns3 