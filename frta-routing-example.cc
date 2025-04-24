#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/frta-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FrtaRoutingExample");

void LogFlowMonitorMetrics(Ptr<FlowMonitor> monitor, Ptr<Ipv4FlowClassifier> classifier)
{
  std::ofstream metricsLog("frta-metrics.log", std::ios::out | std::ios::app);
  metricsLog << "FlowMonitor Metrics at " << Simulator::Now().GetSeconds() << "s\n";

  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto it = stats.begin(); it != stats.end(); ++it)
  {
    Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(it->first);
    double throughput = (it->second.rxBytes * 8.0) / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000;
    double delay = it->second.delaySum.GetSeconds() / it->second.rxPackets;
    double packetLossRatio = (it->second.txPackets - it->second.rxPackets) / (double)it->second.txPackets;

    metricsLog << "Flow " << it->first
               << " (" << tuple.sourceAddress << ":" << tuple.sourcePort
               << " -> " << tuple.destinationAddress << ":" << tuple.destinationPort << ")\n"
               << "  Throughput: " << throughput << " kbps\n"
               << "  Delay: " << delay << " s\n"
               << "  Packet Loss Ratio: " << packetLossRatio * 100 << "%\n";
  }
  metricsLog << "----------------------------------------\n";
  metricsLog.close();

  Simulator::Schedule(Seconds(1.0), &LogFlowMonitorMetrics, monitor, classifier);
}

int
main(int argc, char *argv[])
{
  // Enable logging components
  LogComponentEnable("FrtaRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("FrtaRoutingExample", LOG_LEVEL_INFO);

  // Allow command line arguments
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Creating nodes");
  // Create nodes
  NodeContainer nodes;
  nodes.Create(5);

  NS_LOG_INFO("Configuring WiFi");
  // Configure WiFi
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("DsssRate11Mbps"),
                              "ControlMode", StringValue("DsssRate11Mbps"));

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel",
                                "MaxRange", DoubleValue(100.0));
  wifiPhy.SetChannel(wifiChannel.Create());
  wifiPhy.Set("TxPowerStart", DoubleValue(20.0));
  wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));
  wifiPhy.Set("RxSensitivity", DoubleValue(-85.0));

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  NS_LOG_INFO("Configuring mobility");
  // Configure mobility for MANET scenario
  MobilityHelper mobility;
  
  // Create position allocator
  Ptr<GridPositionAllocator> positionAlloc = CreateObject<GridPositionAllocator>();
  positionAlloc->SetAttribute("MinX", DoubleValue(0.0));
  positionAlloc->SetAttribute("MinY", DoubleValue(0.0));
  positionAlloc->SetAttribute("DeltaX", DoubleValue(30.0));
  positionAlloc->SetAttribute("DeltaY", DoubleValue(30.0));
  positionAlloc->SetAttribute("GridWidth", UintegerValue(3));
  positionAlloc->SetAttribute("LayoutType", StringValue("RowFirst"));
  
  mobility.SetPositionAllocator(positionAlloc);

  // Then make them move using Random Waypoint model with slower speed for better visualization
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                           "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=5.0]"),
                           "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                           "PositionAllocator", PointerValue(positionAlloc));
  
  mobility.Install(nodes);

  NS_LOG_INFO("Creating and configuring the FRTA routing helper");
  // Create and configure the FRTA routing helper
  FrtaRoutingHelper frtaRouting;
  frtaRouting.SetUpdateInterval(Seconds(30.0));
  
  NS_LOG_INFO("Installing internet stack with FRTA routing");
  // Install internet stack with FRTA routing
  InternetStackHelper stack;
  stack.SetRoutingHelper(frtaRouting);
  stack.Install(nodes);

  NS_LOG_INFO("Assigning IP addresses");
  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  NS_LOG_INFO("Setting up server application");
  // Set up server application - use port 10 instead of 9 to avoid conflict with FRTA protocol
  UdpEchoServerHelper echoServer(10);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  NS_LOG_INFO("Setting up client application");
  // Set up client application - use port 10 to match server
  UdpEchoClientHelper echoClient(interfaces.GetAddress(4), 10);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  // Start client after server is ready
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  NS_LOG_INFO("Configuring NetAnim");
  // Configure NetAnim with better packet visualization
  AnimationInterface anim("frta-animation.xml");
  
  // Enable packet metadata and configure update intervals
  anim.EnablePacketMetadata(true);
  anim.SetMobilityPollInterval(Seconds(0.1));
  anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);  // Fix source node position
  anim.SetConstantPosition(nodes.Get(4), 120.0, 120.0);  // Fix destination node position
  
  // Set different colors and sizes for nodes and packets
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
    
    if (i == 0) {
      // Source node (green)
      anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0);
    } else if (i == 4) {
      // Destination node (red)
      anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    } else {
      // Intermediate nodes (blue)
      anim.UpdateNodeColor(nodes.Get(i), 0, 0, 255);
    }
    anim.UpdateNodeSize(nodes.Get(i), 5.0, 5.0);
  }

  // Configure packet counters and routing visualization
  anim.EnableIpv4L3ProtocolCounters(Seconds(0), Seconds(20), Seconds(0.1));
  anim.EnableWifiMacCounters(Seconds(0), Seconds(20));
  anim.EnableWifiPhyCounters(Seconds(0), Seconds(20));

  NS_LOG_INFO("Setting up FlowMonitor");
  // Set up FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  Simulator::Schedule(Seconds(1.0), &LogFlowMonitorMetrics, monitor, classifier);

  NS_LOG_INFO("Enabling pcap tracing");
  // Enable pcap tracing
  wifiPhy.EnablePcap("frta-routing", devices);

  NS_LOG_INFO("Running simulation");
  // Run simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  NS_LOG_INFO("Saving flow monitor results");
  // Save flow monitor results
  monitor->SerializeToXmlFile("frta-flowmon.xml", true, true);

  NS_LOG_INFO("Destroying simulation");
  Simulator::Destroy();
  return 0;
}