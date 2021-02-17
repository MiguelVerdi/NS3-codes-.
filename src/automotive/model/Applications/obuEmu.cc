/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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

 * Created by:
 *  Marco Malinverno, Politecnico di Torino (marco.malinverno1@gmail.com)
 *  Francesco Raviglione, Politecnico di Torino (francescorav.es483@gmail.com)
 *  Carlos Mateo Risma Carletti, Politecnico di Torino (carlosrisma@gmail.com)
*/

#include "obuEmu.h"

#include "ns3/CAM.h"
#include "ns3/DENM.h"
#include "ns3/vdpTraci.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"

namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("obuEmu");

  NS_OBJECT_ENSURE_REGISTERED(obuEmu);

  TypeId
  obuEmu::GetTypeId (void)
  {
    static TypeId tid =
        TypeId ("ns3::obuEmu")
        .SetParent<Application> ()
        .SetGroupName ("Applications")
        .AddConstructor<obuEmu> ()
        .AddAttribute ("Client",
            "TraCI client for SUMO",
            PointerValue (0),
            MakePointerAccessor (&obuEmu::m_client),
            MakePointerChecker<TraciClient> ());
        return tid;
  }

  obuEmu::obuEmu ()
  {
    NS_LOG_FUNCTION(this);
    m_client = nullptr;
  }

  obuEmu::~obuEmu ()
  {
    NS_LOG_FUNCTION(this);
  }

  void
  obuEmu::DoDispose (void)
  {
    NS_LOG_FUNCTION(this);
    Application::DoDispose ();
  }

  void
  obuEmu::StartApplication (void)
  {
    NS_LOG_FUNCTION(this);
    m_id = m_client->GetVehicleId (this->GetNode ());

    /* Create the socket for TX and RX (must be a PacketSocket in order to use GeoNet instead of IP) */
    TypeId tid = TypeId::LookupByName ("ns3::PacketSocketFactory");

    /* Socket used to send CAMs and receive DENMs */
    m_socket = Socket::CreateSocket (GetNode (), tid);

    /* Bind the socket to local address */
    PacketSocketAddress local;
    local.SetSingleDevice (GetNode ()->GetDevice (0)->GetIfIndex ());
    local.SetPhysicalAddress (GetNode ()->GetDevice (0)->GetAddress ());
    local.SetProtocol (0x8947);
    if (m_socket->Bind (local) == -1)
      {
        NS_FATAL_ERROR ("Failed to bind client socket");
      }

    // Set the socket to broadcast
    PacketSocketAddress remote;
    remote.SetSingleDevice (GetNode ()->GetDevice (0)->GetIfIndex ());
    remote.SetPhysicalAddress (GetNode ()->GetDevice (0)->GetBroadcast ());
    remote.SetProtocol (0x8947);

    m_socket->Connect(remote);

    //Create new btp and geoNet objects and set them in DENBasicService and CABasicService
    m_btp = CreateObject <btp>();
    m_geoNet = CreateObject <GeoNet>();
    m_btp->setGeoNet(m_geoNet);
    m_denService.setBTP(m_btp);
    m_caService.setBTP(m_btp);

    /* Set sockets, callback and station properties in DENBasicService */
    m_denService.setSocketRx (m_socket);
    m_denService.setSocketTx (m_socket);
    m_denService.setStationProperties (std::stol(m_id.substr (3)), StationType_passengerCar);
    m_denService.addDENRxCallback (std::bind(&obuEmu::receiveDENM,this,std::placeholders::_1,std::placeholders::_2));
    m_denService.setRealTime (true);

    /* Set sockets, callback, station properties and TraCI VDP in CABasicService */
    m_caService.setSocketRx (m_socket);
    m_caService.setSocketTx (m_socket);
    m_caService.setStationProperties (std::stol(m_id.substr (3)), StationType_passengerCar);
    m_caService.addCARxCallback (std::bind(&obuEmu::receiveCAM,this,std::placeholders::_1,std::placeholders::_2));
    m_caService.setRealTime (true);

    /* Set TraCI vdp for GeoNet object */
    VDPTraCI traci_vdp(m_client,m_id);
    m_caService.setVDP(traci_vdp);

    /* Schedule CAM dissemination */
    std::srand(Simulator::Now().GetNanoSeconds ());
    double desync = ((double)std::rand()/RAND_MAX);
    m_caService.startCamDissemination(desync);

    /* Schedule DENM dissemination */
    obuEmu::TriggerDenm ();
  }

  void
  obuEmu::StopApplication ()
  {
    NS_LOG_FUNCTION(this);

    uint64_t cam_sent;
    cam_sent = m_caService.terminateDissemination ();
    std::cout<<"Number of CAMs sent for vehicle " <<m_id<< ": "<<cam_sent<<std::endl;;

    m_denService.cleanup();
    Simulator::Cancel (m_sendDenmEvent);
  }

  void
  obuEmu::StopApplicationNow ()
  {
    NS_LOG_FUNCTION(this);
    StopApplication ();
  }

  void
  obuEmu::TriggerDenm()
  {
    denData data;
    ActionID_t actionid;
    DENBasicService_error_t trigger_retval;

    /* Set DENM mandatpry fields */
    data.setDenmMandatoryFields (compute_timestampIts(),Latitude_unavailable,Longitude_unavailable);

    /* Compute GeoArea for denms */
    GeoArea_t geoArea;
    // Longitude and Latitude in [0.1 microdegree]
    libsumo::TraCIPosition pos = m_client->TraCIAPI::vehicle.getPosition (m_id);
    pos = m_client->TraCIAPI::simulation.convertXYtoLonLat (pos.x,pos.y);
    geoArea.posLong = pos.x*DOT_ONE_MICRO;
    geoArea.posLat = pos.y*DOT_ONE_MICRO;
    // Radius [m] of the circle around the vehicle, where the DENM will be received
    geoArea.distA = 50;
    // DistB [m] and angle [deg] equal to zero because we are defining a circular area as specified in ETSI EN 302 636-4-1 [9.8.5.2]
    geoArea.distB = 0;
    geoArea.angle = 0;
    m_denService.setGeoArea (geoArea);

    trigger_retval=m_denService.appDENM_trigger(data,actionid);

    if(trigger_retval!=DENM_NO_ERROR)
    {
      NS_LOG_ERROR("Cannot trigger DENM. Error code: " << trigger_retval);
    }

    data.denDataFree ();
    m_sendDenmEvent = Simulator::Schedule (Seconds (1), &obuEmu::UpdateDenm, this, actionid);
  }

  void
  obuEmu::UpdateDenm (ActionID actionid)
  {
    denData data;
    DENBasicService_error_t update_retval;

    /* Set DENM mandatpry fields */
    data.setDenmMandatoryFields (compute_timestampIts(),Latitude_unavailable,Longitude_unavailable);

    /* Compute GeoArea for denms */
    GeoArea_t geoArea;
    // Longitude and Latitude in [0.1 microdegree]
    libsumo::TraCIPosition pos = m_client->TraCIAPI::vehicle.getPosition (m_id);
    pos = m_client->TraCIAPI::simulation.convertXYtoLonLat (pos.x,pos.y);
    geoArea.posLong = pos.x*DOT_ONE_MICRO;
    geoArea.posLat = pos.y*DOT_ONE_MICRO;
    // Radius [m] of the circle around the vehicle, where the DENM will be received
    geoArea.distA = 50;
    // DistB [m] and angle [deg] equal to zero because we are defining a circular area as specified in ETSI EN 302 636-4-1 [9.8.5.2]
    geoArea.distB = 0;
    geoArea.angle = 0;
    m_denService.setGeoArea (geoArea);

    update_retval = m_denService.appDENM_update (data,actionid);
    if(update_retval!=DENM_NO_ERROR)
    {
      NS_LOG_ERROR("Cannot update DENM. Error code: " << update_retval);
    }

    data.denDataFree ();

    m_sendDenmEvent = Simulator::Schedule (Seconds (1), &obuEmu::UpdateDenm, this, actionid);

  }

  void
  obuEmu::receiveCAM (CAM_t *cam, Address from)
  {
    /* Implement CAM strategy here */
   std::cout << "Vehicle with ID "<< m_id << " received a new CAM." << std::endl;

   // Free the received CAM data structure
   ASN_STRUCT_FREE(asn_DEF_CAM,cam);
  }

  /* This is just a sample dummy receiveDENM function. The user can customize it to parse the content of a DENM when it is received. */
  void
  obuEmu::receiveDENM (denData denm, Address from)
  {
    std::cout << "Vehicle with ID "<< m_id << " received a new DENM." << std::endl;

    (void) denm; // Contains the data received from the DENM
    (void) from; // Contains the address from which the DENM has been received
  }

  long
  obuEmu::compute_timestampIts ()
  {
    /* To get millisec since  2004-01-01T00:00:00:000Z */
    auto time = std::chrono::system_clock::now(); // get the current time
    auto since_epoch = time.time_since_epoch(); // get the duration since epoch
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch); // convert it in millisecond since epoch

    long elapsed_since_2004 = millis.count() - TIME_SHIFT; // in TIME_SHIFT we saved the millisec from epoch to 2004-01-01
    return elapsed_since_2004;
  }
}





