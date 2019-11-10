#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("wifi-seven");

int main(int argc, char* argv[]){
    
    uint32_t nWifi = 6;
    uint32_t nPackets = 1;
    uint32_t packetSize = 1024;
    bool verbose = false;
    CommandLine cmd;

    cmd.AddValue ("Wifi", "Number of Wifi STA devices", nWifi);
    cmd.AddValue ("nPackets", "Number of packets to be sent from each station device", nPackets);
    cmd.AddValue ("packetSize", "Size of Each packet",packetSize);
    cmd.AddValue ("verbose","Enable Applcation Logging",verbose);
    cmd.Parse (argc,argv);

    

    //Enable Log for applications
    if(verbose){
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }
    
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi > 6?nWifi:6);
    
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    //Contains all the Nodes
    NodeContainer wifiNodes;
    wifiNodes.Add(wifiApNode.Get(0));
    for(uint32_t i=0;i<nWifi;i++){
        wifiNodes.Add(wifiStaNodes.Get(i));
    }
    

    
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    //For mobile nodes
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
    Ssid ssid = Ssid("base-station");
    mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
    
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    //For access point
    mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
    
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy,mac,wifiApNode);

    //Making the mobility model

    MobilityHelper mobility;

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (0.0),
                                    "MinY", DoubleValue (0.0),
                                    "DeltaX", DoubleValue (5.0),
                                    "DeltaY", DoubleValue (10.0),
                                    "GridWidth", UintegerValue (3),
                                    "LayoutType", StringValue ("RowFirst"));

    //Random walk for sta nodes
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (wifiStaNodes);

    //Stationary mobility model for  ap node
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiNodes);
    
    
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces = address.Assign (apDevice);
    wifiInterfaces.Add(address.Assign(staDevices));

    //Installing UDP echo server at ap node
    UdpEchoServerHelper echoServer (1234);
    ApplicationContainer serverApps = echoServer.Install (wifiApNode.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (20.0));

    UdpEchoClientHelper echoClient (wifiInterfaces.GetAddress (0), 1234);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (nPackets));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.25)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    //Install UDP client in each sta nodes
    for(uint32_t i=0;i<nWifi;i++){
        ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (i));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    //Throughput creation
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    //Tracing stuff
    

    Simulator::Stop (Seconds (200.0));

    //Netanim stuff

    AnimationInterface anim ("wifi-seven.xml");
    
    Simulator::Run ();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    float avgThroughput = 0;
    float totalflows = 0;
    int lostPackets = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "---- Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ") ---- \n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << " Lost packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
        std::cout << " Delay: " << i->second.delaySum << "\n";
        avgThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024 ;
        totalflows++;

        lostPackets += i->second.lostPackets;
    }

    std::cout << "------- Summary -----" << "\n";
    std::cout << "Distinct packet flows: "<<totalflows<<"\n";
    std::cout <<"Average Throughput: "<<avgThroughput/totalflows<<"\n";
    std::cout << "Total Packets Lost: " << lostPackets<<"\n";
    Simulator::Destroy ();
    return 0;
}