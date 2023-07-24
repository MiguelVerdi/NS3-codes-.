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
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FirstScriptExample");
const int SERVER_PORT = 9;
const int NODES = 4;
const int CONECCIONS = 4;
const int TOPOLOGY[CONECCIONS][2] = {
		{0,1},
		{1,2},
		{2,3},
		{3,0}

};

const int SERVER = 0;
const int CLIENT = 1;

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(NODES);       //Creates a network topology with NODES devices.

    /*
     * SETS THE PARAMETERS FOR THE COMMUNICATION BETWEEN TWO NODES.
    */

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[CONECCIONS];  //Setting network devices


    for(int i = 0; i < CONECCIONS;i++){
    	devices[i] = pointToPoint.Install(nodes.Get(TOPOLOGY[i][0]), nodes.Get(TOPOLOGY[i][1]));
       }


    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0"); //Network 1 IP address  nodes(0)-nodes(1)

    //Ipv4InterfaceContainer interfaces = address.Assign(devices[0]);
    Ipv4InterfaceContainer interfaces[CONECCIONS];

    for(int i = 0; i < CONECCIONS; i++){
       interfaces[i] = address.Assign(devices[i]);
    }

    UdpEchoServerHelper echoServer(SERVER_PORT);  //Port of Network 1


    //Node server
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(SERVER));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));



    //Node client where to send the request.
    UdpEchoClientHelper echoClient(interfaces[2].GetAddress(0), SERVER_PORT);



    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    //Create the node

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(CLIENT));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));


     OlsrHelper olsr;

     Ipv4StaticRoutingHelper staticRouting;

     Ipv4ListRoutingHelper list;
     list.Add(staticRouting, 0);
     list.Add(olsr, 10);

     InternetStackHelper internet;
     stack.SetRoutingHelper(list); // has effect on the next Install ()



    //Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    //GlobalRoutingHelper::CalculateRoutes();

    //Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim("anim3.xml");
    anim.SetConstantPosition(nodes.Get(0),0.0,48.0);
    anim.SetConstantPosition(nodes.Get(1),30.0,60.0);
    anim.SetConstantPosition(nodes.Get(2),30.0,30.0);
    anim.SetConstantPosition(nodes.Get(3),50.0,30.0);

    anim.EnablePacketMetadata(true); // Optional
    anim.EnableIpv4RouteTracking ("LTE-animation.xml", Seconds(0), Seconds(5), Seconds(0.25)); //Optional


    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
