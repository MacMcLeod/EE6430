/*
 * Copyright (c) 2011 University of Kansas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Justin Rohrer <rohrej@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

//LAB 2
//IAN ROBERTS AND ANNA CASE

#include <fstream>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/buildings-propagation-loss-model.h"
#include "ns3/buildings-helper.h"

using namespace ns3;
using namespace dsr;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("manet");

class RoutingExperiment {
public:
  RoutingExperiment ();
  void Run (string CSVfileName, int p);
  //static void SetMACParam (ns3::NetDeviceContainer & devices,
  //                                 int slotDistance);
  string CommandSetup (int argc, char **argv);

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);
  void CheckThroughput ();

  uint32_t port;
  uint32_t bytesTotal;
  string CSVfileName;
  int m_proto;
  int m_nSinks;
  double m_txp;
  bool m_traceMobility;
  uint32_t m_time;
  uint32_t m_numP;
  uint32_t m_pSize;
  Time     m_pInt;
  uint32_t m_nSep;
  uint32_t m_pRec;
  uint32_t m_bTot;
};
//Set member variables
RoutingExperiment::RoutingExperiment ()
  : port (9),
    CSVfileName ("manet.output.csv"),
    m_proto(1),
    m_nSinks (1),
    m_txp(30),
    m_traceMobility (false),
    m_time (100),
    m_numP (100),
    m_pSize (50),
    m_pInt (0.1),
    m_nSep (3),
    m_pRec (0),
    m_bTot (0)
{
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));
  return sink;
}

//formats output to terminal
static inline string PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress) {
  ostringstream oss;
  oss << Simulator::Now ().GetSeconds () << " " << socket->GetNode ()->GetId ();
  if (InetSocketAddress::IsMatchingType (senderAddress)) {
    InetSocketAddress addr = InetSocketAddress::ConvertFrom (senderAddress);
    oss << " received one packet from " << addr.GetIpv4 ();
  } else {
    oss << " received one packet!";
  }
  return oss.str ();
}

//Updates count and outputs to terminal
void RoutingExperiment::ReceivePacket (Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress))) {
    m_bTot += packet->GetSize ();
    m_pRec += 1;
    NS_LOG_UNCOND (PrintReceivedPacket (socket, packet, senderAddress));
  }
}

//Prints to CSV file
void RoutingExperiment::CheckThroughput () {
  double kbs = (m_bTot * 8.0) / 1000;
  m_bTot = 0;
  ofstream out (CSVfileName.c_str (), ios::app);
  out << (Simulator::Now ()).GetSeconds () << " , "
      << kbs << " , " << m_pRec << " , "
      << m_nSinks << " , " << m_proto << " , "
      << m_txp << endl;

  out.close ();
  m_pRec = 0;
  Simulator::Schedule (Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

//Parses command line arguments
string RoutingExperiment::CommandSetup (int argc, char **argv) {
  CommandLine cmd;
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", CSVfileName);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
  cmd.AddValue ("power", "Tx power dBm", m_txp);
  cmd.AddValue ("time", "simulation time", m_time);
  cmd.AddValue ("numP", "number of packets", m_numP);
  cmd.AddValue ("pSize", "packet size", m_pSize);
  cmd.AddValue ("pInt", "interpacket interval", m_pInt);
  cmd.AddValue ("nSinks", "number of sinks", m_nSinks);
  cmd.AddValue ("nSep", "separation of nodes", m_nSep);
  cmd.Parse (argc, argv);
  return CSVfileName;
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t m_pSize, uint32_t pktCount, Time m_pInt ) {
  if (pktCount > 0) {
    socket->Send (Create<Packet> (m_pSize));
    Simulator::Schedule (m_pInt, &GenerateTraffic,
                         socket, m_pSize,pktCount - 1, m_pInt);
  } else {
    socket->Close ();
  }
}

int main (int argc, char *argv[]) {
  srand (time(NULL));
  RoutingExperiment experiment;
  string CSVfileName = experiment.CommandSetup (argc,argv);
  //blank out the last output file and write the column headers
  ofstream out (CSVfileName.c_str ());
  out << "SimulationSecond," << "ReceiveRate," <<
  "PacketsReceived," << "NumberOfSinks," <<
  "RoutingProtocol," << "TransmissionPower" << endl;
  out.close ();
  for (int p=1; p<4; p++) {
    experiment.Run (CSVfileName, p);
  }
}

void RoutingExperiment::Run (string CSVfileName, int p) {
  Packet::EnablePrinting ();
  int nBuilding = 5;
  double stories = 30;
  double naught = 0;
  double x1 = 100;
  double x2 = 125;
  double x3 = 225;
  double y1 = 50;
  double y2 = 75;
  double y3 = 125;
  string size ("64");
  string rate ("2048bps");
  string phyMode ("DsssRate11Mbps");
  string tr_name ("manet");
  string pName ("protocol");

  Config::SetDefault  ("ns3::OnOffApplication::PacketSize",StringValue (size));
  Config::SetDefault ("ns3::OnOffApplication::DataRate",  StringValue (rate));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",StringValue (phyMode));

  //Create node containers for each building and one for all nodes
  NodeContainer b1, b2, b3, b4, adhocNodes;
  b1.Create (nBuilding);
  adhocNodes.Add(b1);
  b2.Create (nBuilding);
  adhocNodes.Add(b2);
  b3.Create (nBuilding);
  adhocNodes.Add(b3);
  b4.Create (nBuilding);
  adhocNodes.Add(b4);

  //Instantiates static mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (int one=0; one<nBuilding; one+=m_nSep)
    positionAlloc->Add (Vector (1000, one, 0.0));
  for (int two=0; two<nBuilding; two+=m_nSep)
    positionAlloc->Add (Vector (x2, two, 0.0));
  for (int three=0; three<nBuilding; three+=m_nSep)
    positionAlloc->Add (Vector (x2,three+1000,0.0));
  for (int four=0; four<nBuilding; four+=m_nSep)
    positionAlloc->Add (Vector (x1,four+1000,0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (adhocNodes);

  MobilityHelper walk;
  //Bounds for building 1
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|100|0|50"));
  walk.Install(b1);
  //Bounds for building 2
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("125|225|0|50"));
  walk.Install(b2);
  //Bounds for building 3
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("125|225|75|125"));
  walk.Install(b3);
  //Bounds for building 4
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|100|75|125"));
  walk.Install(b4);


  Ptr<Building> Building1 = CreateObject<Building> ();
  Building1->SetBoundaries (Box (naught, x1, naught, y1, naught, stories));
  Building1->SetBuildingType(Building::Commercial);
  Building1->SetExtWallsType(Building::ConcreteWithWindows);
  Building1->SetNFloors(3);
  Ptr<MobilityBuildingInfo> b1Info = CreateObject<MobilityBuildingInfo> ();
  BuildingsHelper::Install(b1);

  Ptr<Building> Building2 = CreateObject<Building> ();
  Building2->SetBoundaries (Box (x2, x3, naught, y1, naught, stories));
  Building2->SetBuildingType(Building::Commercial);
  Building2->SetExtWallsType(Building::ConcreteWithWindows);
  Building2->SetNFloors(3);
  Ptr<MobilityBuildingInfo> b2Info = CreateObject<MobilityBuildingInfo> ();
  BuildingsHelper::Install(b2);

  Ptr<Building> Building3 = CreateObject<Building> ();
  Building3->SetBoundaries (Box (x2, x3, y2, y3, naught, stories));
  Building3->SetBuildingType(Building::Commercial);
  Building3->SetExtWallsType(Building::ConcreteWithWindows);
  Building3->SetNFloors(3);
  Ptr<MobilityBuildingInfo> b3Info = CreateObject<MobilityBuildingInfo> ();
  BuildingsHelper::Install(b3);

  Ptr<Building> Building4 = CreateObject<Building> ();
  Building4->SetBoundaries (Box (naught, x1, y2, y3, naught, stories));
  Building4->SetBuildingType(Building::Commercial);
  Building4->SetExtWallsType(Building::ConcreteWithWindows);
  Building4->SetNFloors(3);
  Ptr<MobilityBuildingInfo> b4Info = CreateObject<MobilityBuildingInfo> ();
  BuildingsHelper::Install(b4);

  //Performs consistency check on all nodes
  BuildingsHelper::MakeMobilityModelConsistent ();

  Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel> ();

  //set up wifi using helpers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  Ptr<YansWifiChannel> wifiChannel = CreateObject <YansWifiChannel> ();
  wifiChannel->SetPropagationLossModel (lossModel);
  wifiChannel->SetPropagationDelayModel (CreateObject <ConstantSpeedPropagationDelayModel> ());
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel);

  //Minimum and maximum TX power
  wifiPhy.Set ("TxPowerStart",DoubleValue (m_txp));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (m_txp));

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);

  //Handles Routing
  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;
  switch (p) {
    case 1:
      list.Add (olsr, 100);
      pName = "OLSR";
      break;
    case 2:
      list.Add (aodv, 100);
      pName = "AODV";
      break;
    case 3:
      list.Add (dsdv, 100);
      pName = "DSDV";
      break;
    default:
      NS_FATAL_ERROR ("No such protocol");
  }
  m_proto = p;
  CheckThroughput();
  cout << "~~~~~~~~~~~~~~~~~" << pName << "~~~~~~~~~~~~~~~~~~\n";

  internet.SetRoutingHelper (list);
  internet.Install (adhocNodes);

  NS_LOG_INFO ("assigning ip address");
  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);


  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  int si,so;
  for (int i=0; i<m_nSinks; i++) {
    si = rand () % 41;
    so = rand () % 41;
    Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (si), adhocNodes.Get (si));
    Ptr<Socket> source = Socket::CreateSocket (adhocNodes.Get(so), tid);
    InetSocketAddress remote = InetSocketAddress (adhocInterfaces.GetAddress (si, 0), port);
    source->Connect (remote);
    Simulator::Schedule (Seconds (50.0), &GenerateTraffic,
                         source, m_pSize, m_numP, m_pInt);

  }

  //Installs flow monitor on the nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  //Runs the simulations and shows output
  Simulator::Stop (Seconds (m_time));
  Simulator::Run ();
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    // $$$$$$$$$ How can we determine the node from the address?
    cout << "Flow:               " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    cout << "  Tx Packets:       " << i->second.txPackets << "\n";
    cout << "  Tx Bytes:         " << i->second.txBytes << "\n";
    cout << "  Rx Packets:       " << i->second.rxPackets << "\n";
    cout << "  Rx Bytes:         " << i->second.rxBytes << "\n";
    cout << "  Packet Loss:      " << i->second.txPackets -i->second.rxPackets << "\n";
    if (i->second.delaySum > 0) {
      cout << "  Throughput:       " << (i->second.rxBytes * 8) / (i->second.timeLastRxPacket - i->second.timeFirstRxPacket) << "bps \n";
    } else {
      cout << "  Throughput:       " << "0 bps \n";
    }
    cout << "  Mean Delay:       " << Seconds (i->second.delaySum) << "\n";
    cout << "  Jitter:           " << Seconds (i->second.jitterSum) << "\n";
  }
  flowmon.SerializeToXmlFile ((tr_name + ".flowmon").c_str(), false, false);
  Simulator::Destroy ();
}

