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

/* TOPOLOGY
*********      **********
BUILDING*      *BUILDING*
*********      **********

*********      **********
BUILDING*      *BUILDING*
*********      **********
where each building contains 10 nodes
each building is 1000 x 500 m
buildings are separated by 500 m
*/

#include <fstream>
#include <iostream>
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
  void Run (int nSinks, string CSVfileName);
  //static void SetMACParam (ns3::NetDeviceContainer & devices,
  //                                 int slotDistance);
  string CommandSetup (int argc, char **argv);

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);
  void CheckThroughput ();

  uint32_t port;
  uint32_t bytesTotal;
  uint32_t packetsReceived;

  string CSVfileName;
  int m_nSinks;
  string m_protocolName;
  double m_txp;
  bool m_traceMobility;
  uint32_t m_protocol;
};

RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    packetsReceived (0),
    CSVfileName ("manet.output.csv"),
    m_txp(7.5),
    m_traceMobility (false),
    m_protocol (1) // OLSR
{
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
    bytesTotal += packet->GetSize ();
    packetsReceived += 1;
    NS_LOG_UNCOND (PrintReceivedPacket (socket, packet, senderAddress));
  }
}

void RoutingExperiment::CheckThroughput () {
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;
  ofstream out (CSVfileName.c_str (), ios::app);
  out << (Simulator::Now ()).GetSeconds () << " , "
      << kbs << " , " << packetsReceived << " , "
      << m_nSinks << " , " << m_protocolName << " , "
      << m_txp << endl;

  out.close ();
  packetsReceived = 0;
  Simulator::Schedule (Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));
  return sink;
}

string RoutingExperiment::CommandSetup (int argc, char **argv) {
  CommandLine cmd;
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", CSVfileName);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", m_traceMobility);
  cmd.AddValue ("protocol", "1=OLSR;2=AODV;3=DSDV", m_protocol);
  cmd.AddValue ("power", "Tx power dBm", m_txp);
  cmd.Parse (argc, argv);
  return CSVfileName;
}

int main (int argc, char *argv[]) {
  RoutingExperiment experiment;
  string CSVfileName = experiment.CommandSetup (argc,argv);
  //blank out the last output file and write the column headers
  ofstream out (CSVfileName.c_str ());
  out << "SimulationSecond," << "ReceiveRate," <<
  "PacketsReceived," << "NumberOfSinks," <<
  "RoutingProtocol," << "TransmissionPower" << endl;
  out.close ();
  int nSinks = 10;

  experiment.Run (nSinks, CSVfileName);
}


void RoutingExperiment::Run (int nSinks, string CSVfileName) {
  Packet::EnablePrinting ();
  int nWifis = 40;
  int nBuilding = 10;
  double stories = 30;
  double naught = 0;
  double x1 = 100;
  double x2 = 125;
  double x3 = 225;
  double y1 = 50;
  double y2 = 75;
  double y3 = 125;
  double TotalTime = 100.0;
  string rate ("2048bps");
  string phyMode ("DsssRate11Mbps");
  string tr_name ("manet");
  int nodeSpeed = 20; //in m/s
  int nodePause = 0; //in s
  m_protocolName = "protocol";

  Config::SetDefault  ("ns3::OnOffApplication::PacketSize",StringValue ("64"));
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
  for (int one=0; one<nBuilding; one+=2)
    positionAlloc->Add (Vector (1000, one, 0.0));
  for (int two=0; two<nBuilding; two+=2)
    positionAlloc->Add (Vector (x2, two, 0.0));
  for (int three=0; three<nBuilding; three+=2)
    positionAlloc->Add (Vector (x2,three+1000,0.0));
  for (int four=0; four<nBuilding; four+=2)
    positionAlloc->Add (Vector (x1,four+1000,0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (adhocNodes);
/*
  MobilityHelper walk;
  //Bounds for building 1
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|1000|0|500"));
  walk.Install(b1);
  //Bounds for building 2
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("1500|2500|0|500"));
  walk.Install(b2);
  //Bounds for building 3
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("1500|2500|1000|1500"));
  walk.Install(b3);
  //Bounds for building 4
  walk.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|1000|1000|1500"));
  walk.Install(b4);
*/

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
/*
  //Implements a constant loss
  Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel> ();
  lossModel->SetDefaultLoss (200); // set default loss to 200 dB (no link)
  for (int g=0; g<(nBuilding-1); g++) {
    lossModel->SetLoss (b1.Get (g)->GetObject<MobilityModel> (), b1.Get (g+1)->GetObject<MobilityModel> (), 10);
    lossModel->SetLoss (b2.Get (g)->GetObject<MobilityModel> (), b2.Get (g+1)->GetObject<MobilityModel> (), 10);
    lossModel->SetLoss (b3.Get (g)->GetObject<MobilityModel> (), b3.Get (g+1)->GetObject<MobilityModel> (), 10);
    lossModel->SetLoss (b4.Get (g)->GetObject<MobilityModel> (), b4.Get (g+1)->GetObject<MobilityModel> (), 10);
  }
  lossModel->SetLoss (b1.Get (3)->GetObject<MobilityModel> (), b2.Get (3)->GetObject<MobilityModel> (), 30);
  lossModel->SetLoss (b2.Get (3)->GetObject<MobilityModel> (), b3.Get (3)->GetObject<MobilityModel> (), 30);
  lossModel->SetLoss (b3.Get (3)->GetObject<MobilityModel> (), b4.Get (3)->GetObject<MobilityModel> (), 30);
  lossModel->SetLoss (b4.Get (3)->GetObject<MobilityModel> (), b1.Get (3)->GetObject<MobilityModel> (), 30);
*/
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
  switch (m_protocol) {
    case 1:
      list.Add (olsr, 100);
      m_protocolName = "OLSR";
      break;
    case 2:
      list.Add (aodv, 100);
      m_protocolName = "AODV";
      break;
    case 3:
      list.Add (dsdv, 100);
      m_protocolName = "DSDV";
      break;
    default:
      NS_FATAL_ERROR ("No such protocol:" << m_protocol);
    }
  internet.SetRoutingHelper (list);
  internet.Install (adhocNodes);

  NS_LOG_INFO ("assigning ip address");
  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);

  OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
  onoff1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));

  for (int i = 0; i < nSinks; i++) {
    Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (i), adhocNodes.Get (i));

    AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (i), port));
    onoff1.SetAttribute ("Remote", remoteAddress);

    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
    ApplicationContainer temp = onoff1.Install (adhocNodes.Get (i + nSinks));
    temp.Start (Seconds (var->GetValue (50.0,60.0)));
    temp.Stop (Seconds (TotalTime));
  }

  stringstream ss;
  ss << nWifis;
  string nodes = ss.str ();

  stringstream ss2;
  ss2 << nodeSpeed;
  string sNodeSpeed = ss2.str ();

  stringstream ss3;
  ss3 << nodePause;
  string sNodePause = ss3.str ();

  stringstream ss4;
  ss4 << rate;
  string sRate = ss4.str ();

/*  //Graphical interpretation
  AnimationInterface anim ("manet.xml");
  anim.SetMobilityPollInterval(Time(0.5));
  anim.SetStartTime (Seconds(50));
  anim.SetStopTime (Seconds(TotalTime));
  anim.EnablePacketMetadata(true);
*/  //Enables flowmonitor for XML

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  CheckThroughput ();

  Simulator::Stop (Seconds (TotalTime));
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
    cout << "  TxOffered:        " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
    cout << "  Rx Packets:       " << i->second.rxPackets << "\n";
    cout << "  Rx Bytes:         " << i->second.rxBytes << "\n";
    if (i->second.rxBytes > 0) {
      cout << "  Throughput:       " << (i->second.rxBytes * 8) / i->second.delaySum << "bps \n";
    } else {
      cout << "  Throughput:       " << "0 bps \n";
    }
    cout << "  Mean Delay:       " << i->second.delaySum << "\n";
    cout << "  Jitter:           " << i->second.jitterSum << "\n";
  }
  flowmon.SerializeToXmlFile ((tr_name + ".flowmon").c_str(), false, false);
  Simulator::Destroy ();
}

