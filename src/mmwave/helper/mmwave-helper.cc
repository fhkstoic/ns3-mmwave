/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/string.h>
#include <ns3/log.h>
#include <ns3/abort.h>
#include <ns3/pointer.h>
#include <iostream>
#include <string>
#include <sstream>
#include "mmwave-helper.h"
#include <ns3/abort.h>
#include <ns3/buildings-propagation-loss-model.h>
#include <ns3/multi-model-spectrum-channel.h>
#include <ns3/uinteger.h>
#include <ns3/double.h>
#include <ns3/ipv4.h>
#include <ns3/mmwave-rrc-protocol-ideal.h>
#include <ns3/lte-rrc-protocol-real.h>
#include <ns3/epc-enb-application.h>
#include <ns3/epc-x2.h>


namespace ns3 {

/* ... */
NS_LOG_COMPONENT_DEFINE ("MmWaveHelper");

NS_OBJECT_ENSURE_REGISTERED (MmWaveHelper);

MmWaveHelper::MmWaveHelper(void)
	:m_imsiCounter (0),
	 m_cellIdCounter (0),
	 m_noTxAntenna (64),
	 m_noRxAntenna (16)
{
	NS_LOG_FUNCTION(this);
	m_channelFactory.SetTypeId (MultiModelSpectrumChannel::GetTypeId ());
	m_enbNetDeviceFactory.SetTypeId (MmWaveEnbNetDevice::GetTypeId ());
	m_ueNetDeviceFactory.SetTypeId (MmWaveUeNetDevice::GetTypeId ());

	m_enbAntennaModelFactory.SetTypeId (AntennaArrayModel::GetTypeId ());
	m_ueAntennaModelFactory.SetTypeId (AntennaArrayModel::GetTypeId ());

}

MmWaveHelper::~MmWaveHelper(void)
{
	NS_LOG_FUNCTION(this);
}

TypeId
MmWaveHelper::GetTypeId (void)
{
	static TypeId
	    tid =
	    TypeId ("ns3::MmWaveHelper")
	    .SetParent<Object> ()
	    .AddConstructor<MmWaveHelper> ()
		.AddAttribute ("PathlossModel",
					   "The type of path-loss model to be used. "
					   "The allowed values for this attributes are the type names "
					   "of any class inheriting from ns3::PropagationLossModel.",
					   StringValue ("ns3::MmWavePropagationLossModel"),
					   MakeStringAccessor (&MmWaveHelper::SetPathlossModelType),
					   MakeStringChecker ())
		.AddAttribute ("Scheduler",
				      "The type of scheduler to be used for eNBs. "
				      "The allowed values for this attributes are the type names "
				      "of any class inheriting from ns3::MmWaveMacScheduler.",
				      StringValue ("ns3::MmWaveRrMacScheduler"),
				      MakeStringAccessor (&MmWaveHelper::SetSchedulerType,
				                          &MmWaveHelper::GetSchedulerType),
				      MakeStringChecker ())
	  .AddAttribute ("HarqEnabled",
					"Enable Hybrid ARQ",
					BooleanValue (false),
					MakeBooleanAccessor (&MmWaveHelper::m_harqEnabled),
					MakeBooleanChecker ())
	;

	return tid;
}

void
MmWaveHelper::DoDispose (void)
{
	NS_LOG_FUNCTION (this);
	m_channel = 0;
	Object::DoDispose ();
}

void
MmWaveHelper::DoInitialize()
{
	NS_LOG_FUNCTION (this);

	m_channel = m_channelFactory.Create<SpectrumChannel> ();
	m_beamforming = CreateObject<MmWaveBeamforming> (m_noTxAntenna, m_noRxAntenna);
	//m_beamforming = CreateObject<MmWaveChannelMatrix> ();

	m_channel->AddSpectrumPropagationLossModel (m_beamforming);

	m_phyMacCommon = CreateObject <MmWavePhyMacCommon> () ;

	m_beamforming->SetCofigurationParameters (m_phyMacCommon);

	m_pathlossModel = m_pathlossModelFactory.Create ();
	Ptr<PropagationLossModel> splm = m_pathlossModel->GetObject<PropagationLossModel> ();
	if( splm )
	{
		NS_LOG_LOGIC (this << " using a SpectrumPropagationLossModel");
		m_channel->AddPropagationLossModel (splm);
	}
	else
	{
		NS_LOG_LOGIC (this << " using a PropagationLossModel");
		Ptr<PropagationLossModel> plm = m_pathlossModel->GetObject<PropagationLossModel> ();
		NS_ASSERT_MSG (plm != 0, " " << m_pathlossModel << " is neither PropagationLossModel nor SpectrumPropagationLossModel");
		m_channel->AddPropagationLossModel (plm);
	}
	m_phyStats = CreateObject<mmWavePhyRxTrace> ();

	Object::DoInitialize();
}

void
MmWaveHelper::SetAntenna (uint16_t Nrx, uint16_t Ntx)
{
	m_noTxAntenna = Ntx;
	m_noRxAntenna = Nrx;
}

void
MmWaveHelper::SetPathlossModelType (std::string type)
{
	NS_LOG_FUNCTION (this << type);
	m_pathlossModelFactory = ObjectFactory ();
	m_pathlossModelFactory.SetTypeId (type);
}

void
MmWaveHelper::SetSchedulerType (std::string type)
{
	NS_LOG_FUNCTION (this << type);
	m_schedulerFactory = ObjectFactory ();
	m_schedulerFactory.SetTypeId (type);
}
std::string
MmWaveHelper::GetSchedulerType () const
{
	return m_schedulerFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetHarqEnabled (bool harqEnabled)
{
	m_harqEnabled = harqEnabled;
}

bool
MmWaveHelper::GetHarqEnabled ()
{
	return m_harqEnabled;
}

NetDeviceContainer
MmWaveHelper::InstallUeDevice (NodeContainer c)
{
	NS_LOG_FUNCTION (this);
	Initialize ();  // Run DoInitialize (), if necessary
	NetDeviceContainer devices;
	for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
	  {
	    Ptr<Node> node = *i;
	    Ptr<NetDevice> device = InstallSingleUeDevice (node);
	    device->SetAddress (Mac48Address::Allocate ());
	    devices.Add (device);
	  }
	return devices;

}

NetDeviceContainer
MmWaveHelper::InstallEnbDevice (NodeContainer c)
{
	NS_LOG_FUNCTION (this);
	Initialize ();  // Run DoInitialize (), if necessary
	NetDeviceContainer devices;
	for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
	  {
	    Ptr<Node> node = *i;
	    Ptr<NetDevice> device = InstallSingleEnbDevice (node);
	    device->SetAddress (Mac48Address::Allocate ());
	    devices.Add (device);
	  }
	return devices;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleUeDevice (Ptr<Node> n)
{
	Ptr<MmWaveUeNetDevice> device = m_ueNetDeviceFactory.Create<MmWaveUeNetDevice> ();
	//m_imsiCounter++;

	Ptr<MmWaveSpectrumPhy> ulPhy = CreateObject<MmWaveSpectrumPhy> ();
	Ptr<MmWaveSpectrumPhy> dlPhy = CreateObject<MmWaveSpectrumPhy> ();

	Ptr<MmWaveUePhy> phy = CreateObject<MmWaveUePhy> (dlPhy, ulPhy);

	Ptr<MmWaveHarqPhy> harq = Create<MmWaveHarqPhy> (m_phyMacCommon->GetNumHarqProcess ());
	dlPhy->SetHarqPhyModule (harq);
//	ulPhy->SetHarqPhyModule (harq);
	phy->SetHarqPhyModule (harq);

	/* Do not do this here. Do it during registration with the BS
	 * phy->SetCofigurationParameters(m_phyMacCommon);*/

	Ptr<mmWaveChunkProcessor> pData = Create<mmWaveChunkProcessor> ();
	pData->AddCallback (MakeCallback (&MmWaveUePhy::GenerateDlCqiReport, phy));
	pData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlPhy));
	dlPhy->AddDataSinrChunkProcessor (pData);
	if(m_harqEnabled)
	{
		dlPhy->SetPhyDlHarqFeedbackCallback (MakeCallback (&MmWaveUePhy::ReceiveLteDlHarqFeedback, phy));
	}

	ulPhy->SetChannel(m_channel);
	dlPhy->SetChannel(m_channel);

	Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
	NS_ASSERT_MSG (mm, "MobilityModel needs to be set on node before calling MmWaveHelper::InstallUeDevice ()");
	ulPhy->SetMobility(mm);
	dlPhy->SetMobility(mm);

	Ptr<MmWaveUeMac> mac = CreateObject<MmWaveUeMac> ();

	/* Antenna model */
	Ptr<AntennaModel> antenna = (m_ueAntennaModelFactory.Create ())->GetObject<AntennaModel> ();
	NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");
	dlPhy->SetAntenna (antenna);
	ulPhy->SetAntenna (antenna);

	Ptr<LteUeRrc> rrc = CreateObject<LteUeRrc> ();
	bool useIdealRrc = true;
	if (useIdealRrc)
	{
		Ptr<mmWaveUeRrcProtocolIdeal> rrcProtocol = CreateObject<mmWaveUeRrcProtocolIdeal> ();
		rrcProtocol->SetUeRrc (rrc);
		rrc->AggregateObject (rrcProtocol);
		rrcProtocol->SetLteUeRrcSapProvider (rrc->GetLteUeRrcSapProvider ());
		rrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
	}
	else
	{
		Ptr<LteUeRrcProtocolReal> rrcProtocol = CreateObject<LteUeRrcProtocolReal> ();
		rrcProtocol->SetUeRrc (rrc);
		rrc->AggregateObject (rrcProtocol);
		rrcProtocol->SetLteUeRrcSapProvider (rrc->GetLteUeRrcSapProvider ());
		rrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
	}
	if (m_epcHelper != 0)
	{
		rrc->SetUseRlcSm (false);
	}
	else
	{
		rrc->SetUseRlcSm (true);
	}
	Ptr<EpcUeNas> nas = CreateObject<EpcUeNas> ();
	nas->SetAsSapProvider (rrc->GetAsSapProvider ());
	rrc->SetAsSapUser (nas->GetAsSapUser ());

	rrc->SetLteUeCmacSapProvider (mac->GetUeCmacSapProvider ());
	mac->SetUeCmacSapUser (rrc->GetLteUeCmacSapUser ());
	rrc->SetLteMacSapProvider (mac->GetUeMacSapProvider ());

	phy->SetUeCphySapUser (rrc->GetLteUeCphySapUser ());
	rrc->SetLteUeCphySapProvider (phy->GetUeCphySapProvider ());

	NS_ABORT_MSG_IF (m_imsiCounter >= 0xFFFFFFFF, "max num UEs exceeded");
	uint64_t imsi = ++m_imsiCounter;

	phy->SetCofigurationParameters (m_phyMacCommon);
	mac->SetCofigurationParameters (m_phyMacCommon);

	phy->SetPhySapUser (mac->GetPhySapUser());
	mac->SetPhySapProvider (phy->GetPhySapProvider());

	device->SetNode(n);
	device->SetAttribute ("Imsi", UintegerValue(imsi));
	device->SetAttribute ("MmWaveUePhy", PointerValue(phy));
	device->SetAttribute ("MmWaveUeMac", PointerValue(mac));
	device->SetAttribute ("EpcUeNas", PointerValue (nas));
	device->SetAttribute ("mmWaveUeRrc", PointerValue (rrc));

	phy->SetDevice (device);
	//phy->SetForwardUpCallback (MakeCallback (&MmWaveUeNetDevice::Receive, device));
	ulPhy->SetDevice(device);
	dlPhy->SetDevice(device);
	nas->SetDevice(device);


	n->AddDevice(device);
	dlPhy->SetPhyRxDataEndOkCallback (MakeCallback (&MmWaveUePhy::PhyDataPacketReceived, phy));
	dlPhy->SetPhyRxCtrlEndOkCallback (MakeCallback (&MmWaveUePhy::ReceiveControlMessageList, phy));
	nas->SetForwardUpCallback (MakeCallback (&MmWaveUeNetDevice::Receive, device));
	if (m_epcHelper != 0)
	{
		m_epcHelper->AddUe (device, device->GetImsi ());
	}

	device->Initialize();

	return device;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleEnbDevice (Ptr<Node> n)
{
	NS_ABORT_MSG_IF (m_cellIdCounter == 65535, "max num eNBs exceeded");
	uint16_t cellId = ++m_cellIdCounter;

	Ptr<MmWaveSpectrumPhy> ulPhy = CreateObject<MmWaveSpectrumPhy> ();
	Ptr<MmWaveSpectrumPhy> dlPhy = CreateObject<MmWaveSpectrumPhy> ();

	Ptr<MmWaveEnbPhy> phy = CreateObject<MmWaveEnbPhy> (dlPhy, ulPhy);

	Ptr<MmWaveHarqPhy> harq = Create<MmWaveHarqPhy> (m_phyMacCommon->GetNumHarqProcess ());
	dlPhy->SetHarqPhyModule (harq);
//	ulPhy->SetHarqPhyModule (harq);
	phy->SetHarqPhyModule (harq);

	Ptr<mmWaveChunkProcessor> pData = Create<mmWaveChunkProcessor> ();
	pData->AddCallback (MakeCallback (&MmWaveEnbPhy::GenerateDataCqiReport, phy));
	pData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlPhy));
	dlPhy->AddDataSinrChunkProcessor (pData);

	phy->SetCofigurationParameters(m_phyMacCommon);

	ulPhy->SetChannel (m_channel);
	dlPhy->SetChannel (m_channel);

	Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
	NS_ASSERT_MSG (mm, "MobilityModel needs to be set on node before calling MmWaveHelper::InstallEnbDevice ()");
	ulPhy->SetMobility (mm);
	dlPhy->SetMobility (mm);

	/* Antenna model */
	Ptr<AntennaModel> antenna = (m_enbAntennaModelFactory.Create ())->GetObject<AntennaModel> ();
	NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");
	dlPhy->SetAntenna (antenna);
	ulPhy->SetAntenna (antenna);

	Ptr<MmWaveEnbMac> mac = CreateObject<MmWaveEnbMac> ();
	mac->SetCofigurationParameters (m_phyMacCommon);
	Ptr<MmWaveMacScheduler> sched = m_schedulerFactory.Create<MmWaveMacScheduler> ();

	/*to use the dummy ffrAlgorithm, I changed the bandwidth to 25 in EnbNetDevice
	m_ffrAlgorithmFactory = ObjectFactory ();
	m_ffrAlgorithmFactory.SetTypeId ("ns3::LteFrNoOpAlgorithm");
	Ptr<LteFfrAlgorithm> ffrAlgorithm = m_ffrAlgorithmFactory.Create<LteFfrAlgorithm> ();
	*/
	Ptr<LteEnbRrc> rrc = CreateObject<LteEnbRrc> ();
	sched->ConfigureCommonParameters (m_phyMacCommon);
	mac->SetMmWaveMacSchedSapProvider(sched->GetMacSchedSapProvider());
	sched->SetMacSchedSapUser (mac->GetMmWaveMacSchedSapUser());

	phy->SetPhySapUser (mac->GetPhySapUser());
	mac->SetPhySapProvider (phy->GetPhySapProvider());

	bool useIdealRrc = true;
	if (useIdealRrc)
	{
		Ptr<MmWaveEnbRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveEnbRrcProtocolIdeal> ();
		rrcProtocol->SetLteEnbRrcSapProvider (rrc->GetLteEnbRrcSapProvider ());
		rrc->SetLteEnbRrcSapUser (rrcProtocol->GetLteEnbRrcSapUser ());
		rrc->AggregateObject (rrcProtocol);
		rrcProtocol->SetCellId (cellId);
	}
	else
	{
		Ptr<LteEnbRrcProtocolReal> rrcProtocol = CreateObject<LteEnbRrcProtocolReal> ();
		rrcProtocol->SetLteEnbRrcSapProvider (rrc->GetLteEnbRrcSapProvider ());
		rrc->SetLteEnbRrcSapUser (rrcProtocol->GetLteEnbRrcSapUser ());
		rrc->AggregateObject (rrcProtocol);
		rrcProtocol->SetCellId (cellId);
	}

	if (m_epcHelper != 0)
	{
		EnumValue epsBearerToRlcMapping;
		rrc->GetAttribute ("EpsBearerToRlcMapping", epsBearerToRlcMapping);
		// it does not make sense to use RLC/SM when also using the EPC
		if (epsBearerToRlcMapping.Get () == LteEnbRrc::RLC_SM_ALWAYS)
		{
			rrc->SetAttribute ("EpsBearerToRlcMapping", EnumValue (LteEnbRrc::RLC_UM_ALWAYS));
		}
	}

	rrc->SetAttribute ("mmWaveDevice", BooleanValue(true));
	rrc->SetLteEnbCmacSapProvider (mac->GetEnbCmacSapProvider ());
	mac->SetEnbCmacSapUser (rrc->GetLteEnbCmacSapUser ());

	rrc->SetLteMacSapProvider (mac->GetUeMacSapProvider ());
	phy->SetmmWaveEnbCphySapUser (rrc->GetLteEnbCphySapUser ());
	rrc->SetLteEnbCphySapProvider (phy->GetmmWaveEnbCphySapProvider ());

	//FFR SAP
	//rrc->SetLteFfrRrcSapProvider (ffrAlgorithm->GetLteFfrRrcSapProvider ());
	//ffrAlgorithm->SetLteFfrRrcSapUser (rrc->GetLteFfrRrcSapUser ());


	Ptr<MmWaveEnbNetDevice> device = m_enbNetDeviceFactory.Create<MmWaveEnbNetDevice> ();
	device->SetNode (n);
	device->SetAttribute ("CellId", UintegerValue (cellId));
	device->SetAttribute ("MmWaveEnbPhy", PointerValue (phy));
	device->SetAttribute ("MmWaveEnbMac", PointerValue (mac));
	device->SetAttribute ("mmWaveScheduler", PointerValue(sched));
	device->SetAttribute ("ConnectedLteRRC", PointerValue (rrc));


	phy->SetDevice (device);
	dlPhy->SetDevice (device);
	dlPhy->SetCellId (cellId);
	ulPhy->SetDevice (device);
	n->AddDevice (device);

	dlPhy->SetPhyRxDataEndOkCallback (MakeCallback (&MmWaveEnbPhy::PhyDataPacketReceived, phy));
	dlPhy->SetPhyRxCtrlEndOkCallback (MakeCallback (&MmWaveEnbPhy::PhyCtrlMessagesReceived, phy));
  dlPhy->SetPhyUlHarqFeedbackCallback (MakeCallback (&MmWaveEnbPhy::ReceiveUlHarqFeedback, phy));

	//mac->SetForwardUpCallback (MakeCallback (&MmWaveEnbNetDevice::Receive, device));
	rrc->SetForwardUpCallback (MakeCallback (&MmWaveEnbNetDevice::Receive, device));


	NS_LOG_LOGIC ("set the propagation model frequencies");
	double freq = m_phyMacCommon->GetCentreFrequency ();
	NS_LOG_LOGIC ("Channel Frequency: " << freq);
	bool freqOk = m_pathlossModel->SetAttributeFailSafe ("Frequency", DoubleValue (freq));
	if (!freqOk)
	{
		NS_LOG_WARN ("Propagation model does not have a Frequency attribute");
	}

	device->Initialize ();

	m_channel->AddRx (dlPhy);


	if (m_epcHelper != 0)
	{
		NS_LOG_INFO ("adding this eNB to the EPC");
		m_epcHelper->AddEnb (n, device, device->GetCellId ());
		Ptr<EpcEnbApplication> enbApp = n->GetApplication (0)->GetObject<EpcEnbApplication> ();
		NS_ASSERT_MSG (enbApp != 0, "cannot retrieve EpcEnbApplication");

		// S1 SAPs
		rrc->SetS1SapProvider (enbApp->GetS1SapProvider ());
		enbApp->SetS1SapUser (rrc->GetS1SapUser ());

		// X2 SAPs
		Ptr<EpcX2> x2 = n->GetObject<EpcX2> ();
		x2->SetEpcX2SapUser (rrc->GetEpcX2SapUser ());
		rrc->SetEpcX2SapProvider (x2->GetEpcX2SapProvider ());
	}

	return device;
}

void
MmWaveHelper::AttachToClosestEnb (NetDeviceContainer ueDevices, NetDeviceContainer enbDevices)
{
	NS_LOG_FUNCTION(this);

	for (NetDeviceContainer::Iterator i = ueDevices.Begin(); i != ueDevices.End(); i++)
	{
		AttachToClosestEnb(*i, enbDevices);
	}

	m_beamforming->Initial(ueDevices,enbDevices);
}

void
MmWaveHelper::AttachToClosestEnb (Ptr<NetDevice> ueDevice, NetDeviceContainer enbDevices)
{
	NS_LOG_FUNCTION (this);
	NS_ASSERT_MSG (enbDevices.GetN () > 0, "empty enb device container");
	Vector uepos = ueDevice->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
	double minDistance = std::numeric_limits<double>::infinity ();
	Ptr<NetDevice> closestEnbDevice;
	for (NetDeviceContainer::Iterator i = enbDevices.Begin (); i != enbDevices.End (); ++i)
	{
	    Vector enbpos = (*i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
	    double distance = CalculateDistance (uepos, enbpos);
	    if (distance < minDistance)
	    {
	        minDistance = distance;
	        closestEnbDevice = *i;
	    }
	}
	NS_ASSERT (closestEnbDevice != 0);

	uint16_t cellId = closestEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId ();
	Ptr<MmWavePhyMacCommon> configParams = closestEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetPhy()->GetConfigurationParameters();

	closestEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetPhy ()->AddUePhy (ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi (), ueDevice);
	ueDevice->GetObject<MmWaveUeNetDevice> ()->GetPhy ()->RegisterToEnb (cellId, configParams);
	closestEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetMac ()->AssociateUeMAC (ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi ());

	Ptr<EpcUeNas> ueNas = ueDevice->GetObject<MmWaveUeNetDevice> ()->GetNas ();
	ueNas->Connect (closestEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId (),
					closestEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetEarfcn ());

	if (m_epcHelper != 0)
	{
		// activate default EPS bearer
		m_epcHelper->ActivateEpsBearer (ueDevice, ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi (), EpcTft::Default (), EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
	}

	// tricks needed for the simplified LTE-only simulations
	//if (m_epcHelper == 0)
	//{
		ueDevice->GetObject<MmWaveUeNetDevice> ()->SetTargetEnb (closestEnbDevice->GetObject<MmWaveEnbNetDevice> ());
	//}

}

/* Call this from a script to configure the MAC PHY common parameters
 * using "SetAttribute" */
Ptr<MmWavePhyMacCommon>
MmWaveHelper::GetPhyMacConfigurable (void)
{
	return (m_phyMacCommon);
}

void
MmWaveHelper::SetPhyMacConfigurationParameters (std::string paramName, std::string value)
{
	std::stringstream ss (value);

	if (paramName.compare("CentreFreq") == 0)
	{
		double cf;
		ss >> cf;
		m_phyMacCommon->SetAttribute ("CentreFreq", DoubleValue(cf));
		m_beamforming->SetAttribute ("CentreFreq", DoubleValue(cf));
	}
	else if (paramName.compare("SymbolPerSlot") == 0)
	{
		uint32_t symNum;
		std::stringstream ss (value);
		ss >> symNum;
		m_phyMacCommon->SetAttribute ("SymbolPerSlot", UintegerValue(symNum));
	}
	else if (paramName.compare("SymbolLength") == 0)
	{
		double prd;
		ss >> prd;
		m_phyMacCommon->SetAttribute ("SymbolPeriod", DoubleValue(prd));
	}
	else if (paramName.compare("SlotsPerSubframe") == 0)
	{
		uint32_t slt;
		ss >> slt;
		m_phyMacCommon->SetAttribute ("SlotsPerSubframe", UintegerValue(slt));
	}
	else if (paramName.compare("SubframePerFrame") == 0)
	{
		uint32_t sf;
		ss >> sf;
		m_phyMacCommon->SetAttribute ("SubframePerFrame", UintegerValue(sf));
	}
	else if (paramName.compare("SubcarriersPerSubband") == 0)
	{
		uint32_t sc;
		ss >> sc;
		m_phyMacCommon->SetAttribute ("SubcarriersPerChunk", UintegerValue(sc));
	}
	else if (paramName.compare("SubbandPerRB") == 0)
	{
		uint32_t sb;
		ss >> sb;
		m_phyMacCommon->SetAttribute ("ChunkPerRB", UintegerValue(sb));
		m_beamforming->SetAttribute ("NumSubbandPerRB", UintegerValue(sb));
	}
	else if (paramName.compare("SubbandWidth") == 0)
	{
		double w;
		ss >> w;
		m_phyMacCommon->SetAttribute ("ChunkWidth", DoubleValue(w));
		m_beamforming->SetAttribute ("ChunkWidth", DoubleValue(w));
	}
	else if (paramName.compare("NumResourceBlock") == 0)
	{
		uint32_t rb;
		ss >> rb;
		m_phyMacCommon->SetAttribute ("ResourceBlockNum", UintegerValue(rb));
		m_beamforming->SetAttribute ("NumResourceBlocks", UintegerValue(rb));
	}
	else if (paramName.compare("NumReferenceSymbols") == 0)
	{
		uint32_t ref;
		ss >> ref;
		m_phyMacCommon->SetAttribute ("NumReferenceSymbols", UintegerValue(ref));

	}
	else if (paramName.compare("TDDControlDataPattern") == 0)
	{
		m_phyMacCommon->SetAttribute ("TDDPattern", StringValue (value));

	}
	else
	{
		NS_LOG_ERROR ("Unknown parameter name "<<paramName);
	}
}

void
MmWaveHelper::SetEpcHelper (Ptr<EpcHelper> epcHelper)
{
	m_epcHelper = epcHelper;
}

class DrbActivator : public SimpleRefCount<DrbActivator>
{
public:
  DrbActivator (Ptr<NetDevice> ueDevice, EpsBearer bearer);
  static void ActivateCallback (Ptr<DrbActivator> a, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti);
  void ActivateDrb (uint64_t imsi, uint16_t cellId, uint16_t rnti);
private:
  bool m_active;
  Ptr<NetDevice> m_ueDevice;
  EpsBearer m_bearer;
  uint64_t m_imsi;
};

DrbActivator::DrbActivator (Ptr<NetDevice> ueDevice, EpsBearer bearer)
  : m_active (false),
    m_ueDevice (ueDevice),
    m_bearer (bearer),
    m_imsi (m_ueDevice->GetObject< MmWaveUeNetDevice> ()->GetImsi ())
{
}

void
DrbActivator::ActivateCallback (Ptr<DrbActivator> a, std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (a << context << imsi << cellId << rnti);
  a->ActivateDrb (imsi, cellId, rnti);
}

void
DrbActivator::ActivateDrb (uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << imsi << cellId << rnti << m_active);
  if ((!m_active) && (imsi == m_imsi))
    {
      Ptr<LteUeRrc> ueRrc = m_ueDevice->GetObject<MmWaveUeNetDevice> ()->GetRrc ();
      NS_ASSERT (ueRrc->GetState () == LteUeRrc::CONNECTED_NORMALLY);
      uint16_t rnti = ueRrc->GetRnti ();
      Ptr<MmWaveEnbNetDevice> enbLteDevice = m_ueDevice->GetObject<MmWaveUeNetDevice> ()->GetTargetEnb ();
      Ptr<LteEnbRrc> enbRrc = enbLteDevice->GetObject<MmWaveEnbNetDevice> ()->GetRrc ();
      NS_ASSERT (ueRrc->GetCellId () == enbLteDevice->GetCellId ());
      Ptr<UeManager> ueManager = enbRrc->GetUeManager (rnti);
      NS_ASSERT (ueManager->GetState () == UeManager::CONNECTED_NORMALLY ||
                 ueManager->GetState () == UeManager::CONNECTION_RECONFIGURATION);
      EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters params;
      params.rnti = rnti;
      params.bearer = m_bearer;
      params.bearerId = 0;
      params.gtpTeid = 0; // don't care
      enbRrc->GetS1SapUser ()->DataRadioBearerSetupRequest (params);
      m_active = true;
    }
}

void
MmWaveHelper::ActivateDataRadioBearer (NetDeviceContainer ueDevices, EpsBearer bearer)
{
  NS_LOG_FUNCTION (this);
  for (NetDeviceContainer::Iterator i = ueDevices.Begin (); i != ueDevices.End (); ++i)
    {
      ActivateDataRadioBearer (*i, bearer);
    }
}
void
MmWaveHelper::ActivateDataRadioBearer (Ptr<NetDevice> ueDevice, EpsBearer bearer)
{
  NS_LOG_FUNCTION (this << ueDevice);
  //NS_ASSERT_MSG (m_epcHelper == 0, "this method must not be used when the EPC is being used");

  // Normally it is the EPC that takes care of activating DRBs
  // when the UE gets connected. When the EPC is not used, we achieve
  // the same behavior by hooking a dedicated DRB activation function
  // to the Enb RRC Connection Established trace source


  Ptr<MmWaveEnbNetDevice> enbmmWaveDevice = ueDevice->GetObject<MmWaveUeNetDevice> ()->GetTargetEnb ();

  std::ostringstream path;
  path << "/NodeList/" << enbmmWaveDevice->GetNode ()->GetId ()
       << "/DeviceList/" << enbmmWaveDevice->GetIfIndex ()
       << "/ConnectedLteRRC/ConnectionEstablished";
  Ptr<DrbActivator> arg = Create<DrbActivator> (ueDevice, bearer);
  Config::Connect (path.str (), MakeBoundCallback (&DrbActivator::ActivateCallback, arg));
}


void
MmWaveHelper::EnableTraces (void)
{
	EnableDlPhyTrace ();
	//EnableEnbPacketCountTrace ();
	//EnableUePacketCountTrace ();
	EnableTransportBlockTrace ();
}

void
MmWaveHelper::EnableDlPhyTrace (void)
{
	NS_LOG_FUNCTION_NOARGS ();
	Config::Connect ("/NodeList/*/DeviceList/*/MmWaveUePhy/ReportCurrentCellRsrpSinr",
			MakeBoundCallback (&mmWavePhyRxTrace::ReportCurrentCellRsrpSinrCallback, m_phyStats));
}

void
MmWaveHelper::EnableEnbPacketCountTrace ()
{
	NS_LOG_FUNCTION_NOARGS ();
	Config::Connect ("/NodeList/*/DeviceList/*/MmWaveEnbPhy/DlSpectrumPhy/ReportEnbTxRxPacketCount",
			MakeBoundCallback (&mmWavePhyRxTrace::ReportPacketCountEnbCallback, m_phyStats));

}

void
MmWaveHelper::EnableUePacketCountTrace ()
{
	NS_LOG_FUNCTION_NOARGS ();
	Config::Connect ("/NodeList/*/DeviceList/*/MmWaveUePhy/DlSpectrumPhy/ReportUeTxRxPacketCount",
			MakeBoundCallback (&mmWavePhyRxTrace::ReportPacketCountUeCallback, m_phyStats));

}

void
MmWaveHelper::EnableTransportBlockTrace ()
{
	NS_LOG_FUNCTION_NOARGS ();
	Config::Connect ("/NodeList/*/DeviceList/*/MmWaveUePhy/ReportDownlinkTbSize",
				MakeBoundCallback (&mmWavePhyRxTrace::ReportDownLinkTBSize, m_phyStats));
}

}

