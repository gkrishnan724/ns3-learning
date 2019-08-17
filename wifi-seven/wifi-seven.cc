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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("wifi-seven");

int main(int argc, char* argv[]){
    
    uint32_t nWifi = 6;
    uint32_t nPackets = 1;
    uint32_t packetSize = 1024;

    CommandLine cmd;

    cmd.AddValue ("Wifi", "Number of Wifi STA devices", nWifi);
    cmd.AddValue ("nPackets", "Number of packets to be sent from each station device", nPackets);
    cmd.AddValue ("packetSize", "Size of Each packet",packetSize);

    cmd.Parse (argc,argv);

    

    //Enable Log for applications
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

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
    WifiMacHelper mac;
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
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    //Install UDP client in each sta nodes
    for(uint32_t i=0;i<nWifi;i++){
        ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (i));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    //Tracing stuff
    

    Simulator::Stop (Seconds (20.0));

    //Netanim stuff

    AnimationInterface anim ("wifi-seven.xml");
    
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}