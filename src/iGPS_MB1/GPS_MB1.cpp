// $Header: /raid/cvs-server/REPOSITORY/software/MOOS/interface/general/iGPS/GPSInstrument.cpp,v 5.18 2007/01/31 03:53:10 anrp Exp $
// copyright (2001-2003) Massachusetts Institute of Technology (pnewman et al.)

// GPSInstrument.cpp: implementation of the CGPS_MB1 class.
// 
// Receives data from the GPS in the MB1 RTA, which are passed via UDP
// packets with NMEA strings starting at byte 31
//////////////////////////////////////////////////////////////////////
#include <cstring>
#include "GPS_MB1.h"

using namespace std;



//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CGPS_MB1::CGPS_MB1()
{
	m_sType = "VANILLA";
}

CGPS_MB1::~CGPS_MB1()
{
}


/////////////////////////////////////////////
///this is where it all happens..
bool CGPS_MB1::Iterate()
{
	if (GetData()) {
		PublishData();
	}

	return true;
}


bool CGPS_MB1::OnStartUp()
{
	CMOOSInstrument::OnStartUp();

	//set up Geodetic conversions
	double dfLatOrigin;
	double dfLongOrigin;


	m_MissionReader.GetConfigurationParam("TYPE", m_sType);

	string sVal;

	if (m_MissionReader.GetValue("LatOrigin", sVal)) {
		dfLatOrigin = atof(sVal.c_str());
	} else {
		MOOSTrace("LatOrigin not set - FAIL\n");

		return false;

	}

	if (m_MissionReader.GetValue("LongOrigin", sVal)) {
		dfLongOrigin = atof(sVal.c_str());
	} else {
		MOOSTrace("LongOrigin not set - FAIL\n");

		return false;
	}

	if (!m_Geodesy.Initialise(dfLatOrigin, dfLongOrigin)) {
		MOOSTrace("Geodesy Init failed - FAIL\n");

		return false;
	}


	//here we make the variables that we are managing
	double dfGPSPeriod = 0.01;

	//GPS update @ 2Hz
	AddMOOSVariable("X", "SIM_X", "GPS_X", dfGPSPeriod);

	AddMOOSVariable("Y", "SIM_Y", "GPS_Y", dfGPSPeriod);

	AddMOOSVariable("N", "", "GPS_N", dfGPSPeriod);

	AddMOOSVariable("E", "", "GPS_E", dfGPSPeriod);

	AddMOOSVariable("Raw", "", "GPS_RAW", dfGPSPeriod);

	AddMOOSVariable("Satellites", "", "GPS_SAT", dfGPSPeriod);



	if (IsSimulateMode()) {
		//not much to do...
		RegisterMOOSVariables();
	} else {
		//try to open

		if (!SetupPort()) {
			return false;
		}

		//try 10 times to initialise sensor
		if (!InitialiseSensorN(10, "GPS")) {
			return false;
		}
	}


	return true;
}



bool CGPS_MB1::OnNewMail(MOOSMSG_LIST &NewMail)
{
	return UpdateMOOSVariables(NewMail);
}

bool CGPS_MB1::PublishData()
{
	return PublishFreshMOOSVariables();
}



bool CGPS_MB1::OnConnectToServer()
{
	if (IsSimulateMode()) {
		//not much to do...
		RegisterMOOSVariables();
	} else {

	}

	return true;
}


///////////////////////////////////////////////////////////////////////////
// here we initialise the sensor, giving it start up values
bool CGPS_MB1::InitialiseSensor()
{
    //We don't have any need to initialize the RTA

	return true;

}



/**
*
*/

bool CGPS_MB1::GetData()
{
	if (!IsSimulateMode()) {
		//here we actually access serial ports etc

		string sWhat;

		double dfWhen;

		if (m_Port.IsStreaming()) {
			if (!m_Port.GetLatest(sWhat, dfWhen)) {
				return false;
			}
		} else {
			if (!m_Port.GetTelegram(sWhat, 0.5)) {
				return false;
			}
		}

		//MOOSTrace("Rx:  %s",sWhat.c_str());
		if (PublishRaw()) {
			SetMOOSVar("Raw", sWhat, MOOSTime());
		}

		ParseNMEAString(sWhat);

	} else {
		//in simulated mode there is nothing to do..all data
		//arrives via comms.
	}

	return true;

}


bool CGPS_MB1::ParseNMEAString(string &sNMEAString)
{
  //keep a copy for later..
  string sCopy = sNMEAString;
  string sWhat = MOOSChomp(sNMEAString, ",");
  bool bGood = true;
  
    //first of all is this a good NMEA string?
  if (!DoNMEACheckSum(sCopy)) {
    MOOSDebugWrite("GPS Failed NMEA check sum");
    return false;
  }
  //GGA and GLL headers format the NMEA string differently
  
  if (sWhat == "$GPGGA") {
    
    //OK so extract data...
    double dfTimeNow = MOOSTime();
    
    string sTmp;
    
    sTmp = MOOSChomp(sNMEAString, ",");
    
    /////////////////////////////////////
    //         GPS Time
    double dfTime	= atof(	sTmp.c_str());
    m_Comms.Notify("GPS_TIME", dfTime, dfTimeNow);

    
    /////////////////////////////////////
    //          latitude..
    sTmp = MOOSChomp(sNMEAString, ",");
    if (sTmp.size() == 0)
      bGood = false;
    
    double dfLat = atof(sTmp.c_str());
    
    sTmp = MOOSChomp(sNMEAString, ",");
    
    string sNS	= sTmp;
    
    if (sNS == "S") {
      dfLat *= -1.0;
    }
    
    //////////////////////////////////////
    //          longitude
    sTmp = MOOSChomp(sNMEAString, ",");
    
    if (sTmp.size() == 0)
      bGood = false;
    
    double dfLong = atof(sTmp.c_str());
    
    sTmp = MOOSChomp(sNMEAString, ",");
    
    string sEW = sTmp;
    
    if (sEW == "W") {
      dfLong *= -1.0;
    }
    
    
    ////////////////////////////////////////
    //          quality
    
    sTmp = MOOSChomp(sNMEAString, ",");
    double dfHDOP	= atof(sTmp.c_str());
    sTmp = MOOSChomp(sNMEAString, ",");
    double dfSatellites = atof(	sTmp.c_str());
    if (dfSatellites < 4)
      bGood = false;
    
    
    /////////////////////////////////////////
    //GEODETIC CONVERSION....
    
    double dfLatDecDeg = m_Geodesy.DMS2DecDeg(dfLat);
    double dfLongDecDeg = m_Geodesy.DMS2DecDeg(dfLong);
    double dfELocal;
    double dfNLocal;
    double dfXLocal;
    double dfYLocal;
    
    if (m_Geodesy.LatLong2LocalUTM(dfLatDecDeg, dfLongDecDeg, dfNLocal, dfELocal)) {
      //set our GPS local Northings and Eastings variables
      
      if (bGood) {
	SetMOOSVar("N", dfNLocal, dfTimeNow);
	SetMOOSVar("E", dfELocal, dfTimeNow);
      }
    }
    
    if (m_Geodesy.LatLong2LocalUTM(dfLatDecDeg, dfLongDecDeg, dfYLocal, dfXLocal)) {
      //set our GPS local Grid variables
      
      if (bGood) {
	SetMOOSVar("X", dfXLocal, dfTimeNow);
	SetMOOSVar("Y", dfYLocal, dfTimeNow);
      }
    }
    
    m_Comms.Notify("GPS_LATITUDE", dfLatDecDeg);
    m_Comms.Notify("GPS_LONGITUDE", dfLongDecDeg);
    
    //always say how many satellites we have...
    SetMOOSVar("Satellites", dfSatellites, dfTimeNow);
    
    char tmp[160];
    snprintf(tmp, 160, "time=%lf,n=%lf,e=%lf,x=%lf,y=%lf,lat=%lf,lon=%lf,sat=%i,hdop=%lf",
	     dfTimeNow, dfNLocal, dfELocal, dfXLocal,
	     dfYLocal, dfLatDecDeg, dfLongDecDeg, (int)dfSatellites, dfHDOP);
    
    m_Comms.Notify("GPS_SUMMARY", tmp);
    
    return true;
  } else if(sWhat == "$GPRMC") {
    NMEAMessage m(sCopy);
    
    if(strlen(m.Part(7).c_str())) {
      m_Comms.Notify("GPS_SPEED", atof(m.Part(7).c_str()) * 0.51444444);
    }
    
    if(strlen(m.Part(8).c_str())) {
      double heading = atof(m.Part(8).c_str());
      while(heading > 180) heading -= 360;
      while(heading < -180) heading += 360;
      double yaw = -heading * M_PI/180.0;
      
      m_Comms.Notify("GPS_TRACK_ANG", heading);
      m_Comms.Notify("GPS_YAW", yaw);
    }
    
    if(strlen(m.Part(10).c_str())) {
      double f = atof(m.Part(10).c_str());
      
      if(m.Part(11)[0] == 'W') f = 0 - f;
      
      m_Comms.Notify("GPS_MAGNETIC_DECLINATION", f);
    } 
    return true;
  } else if (sWhat == "$GPHDT") {
    NMEAMessage m(sCopy);

    if(strlen(m.Part(1).c_str())) {
        m_Comms.Notify("GPS_HEADING", atof(m.Part(1).c_str()));
    }
    return true;
  } else if (sWhat == "$GPVTG") {
    NMEAMessage m(sCopy);

    //Track made good (degrees true)
    if(strlen(m.Part(1).c_str())) {
        m_Comms.Notify("GPS_VTG_HEADING", atof(m.Part(1).c_str()));
    }
    //Speed, in knots
    if(strlen(m.Part(5).c_str())) {
        m_Comms.Notify("GPS_SPEED", atof(m.Part(5).c_str()) * 0.51444444);
    } else if (strlen(m.Part(7).c_str())) {
        //Speed over ground in kilometers/hour (kph)
        m_Comms.Notify("GPS_SPEED", atof(m.Part(7).c_str()) * 0.27777778);
    }
    
    return true;
  }
  //May also want to handle ZDA

  return false;
}