#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/propagation-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("NakagamiPropagationLoss");

//Node 0 <-----------> Node 1
//Nakagami Propagation Loss Model

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval ) {
  if (pktCount > 0) {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
    } else {
      socket->Close ();
    }
}

int main (int argc, char *argv[]) {
  string phyMode ("DsssRate11Mbps");
  double distance = 500;
  double loss = 3;
  double freq = 2400000000;
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1000;
  double interval = 0.5; // seconds
  Time interPacketInterval = Seconds (interval);

  CommandLine cmd;
  cmd.AddValue ("distance", "distance", distance);
  cmd.Parse (argc,argv);

  //Creates the nodes
  NodeContainer nodes;
  nodes.Create (2);

  //Creates and sets up helpers
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue(0)); //no gain
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

 // Ptr<ThreeLogDistancePropagationLossModel> lossModel = CreateObject<ThreeLogDistancePropagationLossModel> ();
  Ptr<NakagamiPropagationLossModel> nkg = CreateObject<NakagamiPropagationLossModel> ();
  Ptr<FriisPropagationLossModel> lossg = CreateObject<FriisPropagationLossModel> ();
  lossg->SetMinLoss (loss); // set default loss to 3 dB
  lossg->SetFrequency(freq); //802.11B is 2.4 GHz
  nkg->SetNext(lossg);
  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (nkg);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());
  wifiPhy.SetChannel(wifiChannel);

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  //Configures position of nodes
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  pos->Add (Vector (0.0, 0.0, 0.0));
  pos->Add (Vector (distance, 0.0, 0.0));
  mobility.SetPositionAllocator(pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
/*  //Creates a mobile node
  MobilityHelper mobility2;
  Ptr<RandomWalk2dMobilityModel> move = nodes.Get(1)->GetObject
                                            <RandomWalk2dMobilityModel> ();
  Ptr<ListPositionAllocator> pot = CreateObject<ListPositionAllocator> ();
  pot->Add (Vector (distance, 0, 0));
  mobility2.SetPositionAllocator(pot);
  mobility2.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|1000|0|0"));
  mobility2.Install(nodes.Get(1));
*/
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);

  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (nodes);

  //Assigns IP
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assigning IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (1), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
  recvSink->Bind (local);
  Ptr<Socket> source = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (i.GetAddress (1, 0), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  Simulator::Schedule (Seconds (1), &GenerateTraffic,
                       source, packetSize, numPackets, interPacketInterval);

  //Installs flow monitor on all nodes
  Ptr<FlowMonitor> monitor;
  FlowMonitorHelper flowmon;
  monitor = flowmon.Install(nodes);

  NS_LOG_UNCOND ("Testing Transmission at distance " << distance);
  //Runs the simulation for '25' seconds
  Simulator::Stop (Seconds (505.0));
  Simulator::Run ();

  //Print chosen flow monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    uint32_t t_packet = i->second.txPackets;
    uint32_t t_byte = i->second.txBytes;
    uint32_t r_packet = i->second.rxPackets;
    uint32_t r_byte = i->second.rxBytes;
    Time begin = i->second.timeFirstRxPacket;
    Time end   = i->second.timeLastRxPacket;
    Time delay = i->second.delaySum;
    Time jitter = i->second.jitterSum;
    Time min = delay - jitter;
    Time max = delay + jitter;
    Time avg = 1000*(delay/distance);
    uint32_t lost = i->second.lostPackets;
    uint32_t through = (8*r_byte)/delay.GetSeconds();
    cout << "Flow (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    cout << "\n FLOW MONITOR METRICS:\n";
    cout << "1.Tx Packets:           " << t_packet << "\n";
    cout << "2.Tx Bytes:             " << t_byte << "\n";
    cout << "3.Rx Packets:           " << r_packet << "\n";
    cout << "4.Rx Bytes:             " << r_byte << "\n";
    cout << "5.Delay Sum:            " << delay.As (Time::S) << "\n";
    cout << "6.Jitter Sum:           " << jitter.As (Time::S) << "\n";
    cout << "7.Lost Packets:         " << lost << "\n";
    cout << "\n CUSTOM METRICS:\n";
    cout << "1.Throughput:           " << through << "bps \n";
    cout << "THROUGH DIST            " << through/distance << "\n";
    cout << "2.Link Utilization:     " << through/110000 << "% \n";
    cout << "3.Mean Delay:           " << delay/r_byte << "\n";
    cout << "AVG DELAY:              " << avg.As (Time::S) << "\n";
    cout << "4.Minimum Delay:        " << min.As (Time::S) << "\n";
    cout << "5.Maximum Delay:        " << max.As (Time::S) << "\n";
    cout << "6.Round Trip Delay:     " << (delay*2).As (Time::S) << "\n";
  }
  Simulator::Destroy ();
  return 0;
}

