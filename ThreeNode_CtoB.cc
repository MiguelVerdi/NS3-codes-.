/*
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
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

// Default Network Topology
//                   server
//       10.1.1.0          10.1.2.0
// n0 -------------- n1 -------------- n2
//    point-to-point    point-to-point
//        port 9            port 9


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    /*
     * LOG INFO IS USED TO GET MESSAGES IN THE CONSOLE ABOUT THE PROCESS.     *
     */
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);


    NodeContainer nodes;
    nodes.Create(3);       //Creates a network topology with 3 devices.

    /*
     * SETS THE PARAMETERS FOR THE COMMUNICATION BETWEEN TWO NODES.
     */
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesN1;  //Setting network 1 -> nodes(0)-nodes(1)
    NetDeviceContainer devicesN2;  //Setting network 2 -> nodes(1)-nodes(2)

    devicesN1 = pointToPoint.Install(nodes.Get(0),nodes.Get(1));  //Sets the communication parameters PtoP.
    devicesN2 = pointToPoint.Install(nodes.Get(1),nodes.Get(2));  //Sets the communication parameters PtoP.

    /*
     * SETS INTERNET PROTOCOLS IN THE NETWORK.
     */
    InternetStackHelper stack;
    stack.Install(nodes);



    /*
     * SETS ADRESSES IN BOTH NETWORKS
     */
    Ipv4AddressHelper addressN1;
    addressN1.SetBase("10.1.1.0", "255.255.255.0"); //Network 1 IP address  nodes(0)-nodes(1)
    Ipv4InterfaceContainer interfacesN1 = addressN1.Assign(devicesN1);


    Ipv4AddressHelper addressN2;
    addressN2.SetBase("43.23.5.0", "255.255.255.0"); //Network 2 IP address nodes(1)-nodes(2)
    Ipv4InterfaceContainer interfacesN2 = addressN2.Assign(devicesN2);


    UdpEchoServerHelper echoServerN1(9);  //Port of Network 1
    UdpEchoServerHelper echoServerN2(10); //Port of Network 2



    /*
     * Simulation of the application in one of the nodes (the server).
     */
    ApplicationContainer serverApps = echoServerN1.Install(nodes.Get(1)); //Node broadcasting to the network.
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));


    /*
     * Setting parameters of the clients reciving the packages.
     */

     UdpEchoClientHelper echoClientN1(interfacesN1.GetAddress(1), 9); //Network 1 comunicate with port 9.
     echoClientN1.SetAttribute("MaxPackets", UintegerValue(1));
     echoClientN1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
     echoClientN1.SetAttribute("PacketSize", UintegerValue(1024));


     UdpEchoClientHelper echoClientN2(interfacesN2.GetAddress(0), 9); //Network 2 comunicates with port 10.
     echoClientN2.SetAttribute("MaxPackets", UintegerValue(1));
     echoClientN2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
     echoClientN2.SetAttribute("PacketSize", UintegerValue(1024));


     ApplicationContainer clientAppsN1 = echoClientN1.Install(nodes.Get(0));
     clientAppsN1.Start(Seconds(2.0));
     clientAppsN1.Stop(Seconds(6.0));


     ApplicationContainer clientAppsN2 = echoClientN2.Install(nodes.Get(2));
     clientAppsN2.Start(Seconds(6.0));
     clientAppsN2.Stop(Seconds(9.0));


     AnimationInterface anim("ThreeNode_CtoB.xml");
     anim.SetConstantPosition(nodes.Get(0),1.0,2.0);
     anim.SetConstantPosition(nodes.Get(1),2.0,3.0);
     anim.SetConstantPosition(nodes.Get(2),3.0,4.0);




    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
