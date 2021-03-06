/************************************************************/
/*    NAME: Damian Manda                                              */
/*    ORGN: UNH, Durham NH                                            */
/*    FILE: GP9.cpp                                        */
/*    DATE: Apr 15th 2015                                                */
/************************************************************/

#include <iterator>
#include <string>
#include "MBUtils.h"
#include "ACTable.h"
#include "AngleUtils.h"
#include "GP9.h"

using namespace std;

const char VERSION[10] = "0.0.3";   // gp9_driver version

// Don't try to be too clever. Arrival of this message triggers
// us to publish everything we have.
const uint8_t TRIGGER_PACKET = DREG_EULER_PHI_THETA;

//---------------------------------------------------------
// Constructor

GP9::GP9()
  : defaultBaudRate(115200),
  sensor(NULL)
{
  baudRate = defaultBaudRate;
}

//---------------------------------------------------------
// Procedure: OnNewMail

bool GP9::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();

#if 0 // Keep these around just for template
    string comm  = msg.GetCommunity();
    double dval  = msg.GetDouble();
    string sval  = msg.GetString(); 
    string msrc  = msg.GetSource();
    double mtime = msg.GetTime();
    bool   mdbl  = msg.IsDouble();
    bool   mstr  = msg.IsString();
#endif

     //if(key == "FOO") 
     //  cout << "great!";

     if(key != "APPCAST_REQ") // handle by AppCastingMOOSApp
       reportRunWarning("Unhandled Mail: " + key);
   }
	
   return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer

bool GP9::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool GP9::Iterate()
{
  AppCastingMOOSApp::Iterate();
  // Do your thing here!

  // triggered by arrival of last message packet
  //MOOSTrace("GP9 Loop Run");
  if (ser.isOpen())
  {
    //MOOSTrace("GP9 Serial Open Loop");
    if (sensor.receive(&registers) == TRIGGER_PACKET)
    {
      publishMsgs(registers);
    }
  }
  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool GP9::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  // STRING_LIST::iterator p;
  // for(p=sParams.begin(); p!=sParams.end(); p++) {
  //   string orig  = *p;
  //   string line  = *p;
  //   string param = toupper(biteStringX(line, '='));
  //   string value = line;

  //   bool handled = false;
  //   if(param == "FOO") {
  //     handled = true;
  //   }
  //   else if(param == "BAR") {
  //     handled = true;
  //   }

  //   if(!handled)
  //     reportUnhandledConfigWarning(orig);

  // }

  if (!GeodesySetup()) {
	return(false);
  }

  // Set Up Serial Port
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  if (!m_MissionReader.GetConfigurationParam("SerialPort", serialPort))
    {
      MOOSTrace("Warning: parameter 'SerialPort' not specified.\n");
      MOOSTrace("Terminating\n");
      exit(-1);
    }

  if (!m_MissionReader.GetConfigurationParam("BaudRate", baudRate))
  {
    MOOSTrace("Warning: parameter 'BaudRate' not specified.  Using "
      "default baud rate (%i)\n", defaultBaudRate);
  }
  

  ser.setPort(serialPort);
  ser.setBaudrate(baudRate);
  serial::Timeout to = serial::Timeout(50, 50, 0, 50, 0);
  ser.setTimeout(to);

  // Initialize covariance. The GP9 sensor does not provide covariance values so,
  //   by default, this driver provides a covariance array of all zeros indicating
  //   "covariance unknown" as advised in sensor_msgs/Imu.h.
  // This param allows the user to specify alternate covariance values if needed.

  std::string covariance;
  char cov[200];
  char *ptr1;

  if (!m_MissionReader.GetConfigurationParam("Covariance", covariance))
  {
    MOOSTrace("Using Default Covariance");
    covariance = "0 0 0 0 0 0 0 0 0";
  }
  snprintf(cov, sizeof(cov), "%s", covariance.c_str());

  char* p = strtok_r(cov, " ", &ptr1);           // point to first value
  for (int iter = 0; iter < 9; iter++)
  {
    if (p) covar[iter] = atof(p);                // covar[] is global var
    else  covar[iter] = 0.0;
    p = strtok_r(NULL, " ", &ptr1);              // point to next value (nil if none)
  }

  // Real Time Loop
  first_failure = true;
  try
  {
    ser.open();
  }
  catch(const serial::IOException& e)
  {
      MOOSTrace("gp9_driver ver %s unable to connect to port.", VERSION);
      MOOSTrace(e.what());
  }
  if (ser.isOpen())
  {
    MOOSTrace("gp9_driver ver %s connected to serial port.", VERSION);
    first_failure = true;
    try
    {
      //Configure the comms rates/options
      sensor = gp9::Comms(&ser);
      configureSensor(&sensor);
    }
    catch(const std::exception& e)
    {
      if (ser.isOpen()) ser.close();
      MOOSTrace(e.what());
      MOOSTrace("GP9: Attempting reconnection after error.");
    }
  }
  else
  {
    if (first_failure)
    {
    MOOSTrace("Could not connect to serial device "
        + serialPort + ". Trying again on next iteration");
    }
    first_failure = false;
  }


  registerVariables();

  return(true);
}

bool GP9::GeodesySetup()
{
  m_dLatOrigin = 0.0;
  m_dLonOrigin = 0.0;
  bool geoOK = m_MissionReader.GetValue("LatOrigin", m_dLatOrigin);
  if (!geoOK) {
    reportConfigWarning("Latitude origin missing in MOOS file. Could not configure geodesy.");
    return false; }
  else {
    geoOK = m_MissionReader.GetValue("LongOrigin", m_dLonOrigin);
    if (!geoOK) {
      reportConfigWarning("Longitude origin missing in MOOS file. Could not configure geodesy.");
      return false; } }
  geoOK = m_geodesy.Initialise(m_dLatOrigin, m_dLonOrigin);
  if (!geoOK) {
    reportConfigWarning("Could not initialize geodesy with given origin.");
    return false; }
  return true;
}

//---------------------------------------------------------
// Procedure: registerVariables

void GP9::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  // Register("FOOBAR", 0);
}


//------------------------------------------------------------
// Procedure: buildReport()

bool GP9::buildReport() 
{
  m_msgs << "============================================ \n";
  m_msgs << "GP9 Driver:                                  \n";
  m_msgs << "============================================ \n";

  ACTable actab(7);
  actab << "Lat | Lon | Yaw | E | N | Vel E | Vel N";
  actab.addHeaderLines();
  actab << registers.latitude.get_scaled(0) << registers.longitude.get_scaled(0) 
    << registers.euler.get_scaled(2) << registers.pos_e.get_scaled(0)
    << registers.pos_n.get_scaled(0) << registers.velocity_e.get_scaled(0)
    << registers.velocity_n.get_scaled(0);
  m_msgs << actab.getFormattedString();

  return(true);
}

//=============================================================
// GP9 Specific Functions
//=============================================================


/**
 * Function generalizes the process of writing an XYZ vector into consecutive
 * fields in GP9 registers.
 */
// template<typename RegT>
// void GP9::configureVector3(gp9::Comms* sensor, const gp9::Accessor<RegT>& reg,
//     std::string param, std::string human_name)
// {
//   if (reg.length != 3)
//   {
//     throw std::logic_error("configureVector3 may only be used with 3-field registers!");
//   }

//   if (ros::param::has(param))
//   {
//     double x, y, z;
//     ros::param::get(param + "/x", x);
//     ros::param::get(param + "/y", y);
//     ros::param::get(param + "/z", z);
//     ROS_INFO_STREAM("Configuring " << human_name << " to ("
//                     << x << ", " << y << ", " << z << ")");
//     reg.set_scaled(0, x);
//     reg.set_scaled(1, y);
//     reg.set_scaled(2, z);
//     if (sensor->sendWaitAck(reg))
//     {
//       throw std::runtime_error("Unable to configure vector.");
//     }
//   }
// }

/**
 * Function generalizes the process of commanding the GP9 via one of its command
 * registers.
 */
template<typename RegT>
void GP9::sendCommand(gp9::Comms* sensor, const gp9::Accessor<RegT>& reg, std::string human_name)
{
  MOOSTrace("Sending command: " + human_name);
  if (!sensor->sendWaitAck(reg))
  {
    throw std::runtime_error("Command to device failed.");
  }
}


/**
 * Send configuration messages to the GP9, critically, to turn on the value outputs
 * which we require, and inject necessary configuration parameters.
 */
void GP9::configureSensor(gp9::Comms* sensor)
{
  gp9::Registers r;

  uint32_t comm_reg = (BAUD_115200 << COM_BAUD_START);
  r.communication.set(0, comm_reg);
  if (!sensor->sendWaitAck(r.comrate2))
  {
    throw std::runtime_error("Unable to set CREG_COM_SETTINGS.");
  }

  uint32_t raw_rate = (0 << RATE2_ALL_RAW_START);
  r.comrate2.set(0, raw_rate);
  if (!sensor->sendWaitAck(r.comrate2))
  {
    throw std::runtime_error("Unable to set CREG_COM_RATES2.");
  }
  
  uint32_t proc_indiv_rate = 0; //(10 << RATE3_PROC_ACCEL_START);
  r.comrate3.set(0, proc_indiv_rate);
  if (!sensor->sendWaitAck(r.comrate3))
  {
    throw std::runtime_error("Unable to set CREG_COM_RATES2.");
  }

  uint32_t proc_rate = (1 << RATE4_ALL_PROC_START);
  r.comrate4.set(0, proc_rate);
  if (!sensor->sendWaitAck(r.comrate4))
  {
    throw std::runtime_error("Unable to set CREG_COM_RATES4.");
  }

  uint32_t misc_rate = (10 << RATE5_EULER_START) | (10 << RATE5_POSITION_START)
           | (10 << RATE5_VELOCITY_START) | (0 << RATE5_QUAT_START);
  r.comrate5.set(0, misc_rate);
  if (!sensor->sendWaitAck(r.comrate5))
  {
    throw std::runtime_error("Unable to set CREG_COM_RATES5.");
  }

  uint32_t health_rate = (0 << RATE6_POSE_START) | (5 << RATE6_HEALTH_START);  // note:  5 gives 2 hz rate
  r.comrate6.set(0, health_rate);
  if (!sensor->sendWaitAck(r.comrate6))
  {
    throw std::runtime_error("Unable to set CREG_COM_RATES6.");
  }

  r.home_north.set(0, (float)m_dLatOrigin);
  if (!sensor->sendWaitAck(r.home_north))
  {
    throw std::runtime_error("Unable to set CREG_HOME_NORTH.");
  }

  r.home_east.set(0, (float)m_dLonOrigin);
  if (!sensor->sendWaitAck(r.home_east))
  {
    throw std::runtime_error("Unable to set CREG_HOME_EAST.");
  }

  // Options available using parameters)
 // Options available using parameters)
  uint32_t filter_config_reg = 0;  // initialize all options off

  // Optionally disable mag updates in the sensor's EKF.
  bool mag_updates = true;
  if (mag_updates)
  {
    filter_config_reg |= MAG_UPDATES_ENABLED;
  }
  else
  {
    MOOSTrace("Excluding magnetometer updates from EKF.");
  }

  // Optionally disable accelerometer updates in the sensor's EKF.
  bool acc_updates = true;
  if (acc_updates)
  {
    filter_config_reg |= ACC_UPDATES_ENABLED;
  }
  else
  {
    MOOSTrace("Excluding accelerometer updates from EKF.");
  }

  // Optionally disable gps updates in the sensor's EKF.
  bool gps_updates = true;
  if (gps_updates)
  {
    filter_config_reg |= GPS_UPDATES_ENABLED;
  }
  else
  {
    MOOSTrace("Excluding GPS updates from EKF.");
  }

  r.filter_config.set(0, filter_config_reg);
  if (!sensor->sendWaitAck(r.filter_config))
  {
    throw std::runtime_error("Unable to set CREG_FILTER_SETTINGS.");
  }

  // Optionally disable performing a zero gyros command on driver startup.
  bool zero_gyros = true;
  if (zero_gyros) sendCommand(sensor, r.cmd_zero_gyros, "zero gyroscopes");
}


/**
 * Uses the register accessors to grab data from the IMU, and populate
 * the ROS messages which are output.
 */
void GP9::publishMsgs(gp9::Registers& r)
{

  // Euler attitudes.  transform to ROS axes
  m_Comms.Notify("GP9_Roll", r.euler.get_scaled(0), MOOSTime());
  m_Comms.Notify("GP9_Pitch", r.euler.get_scaled(1), MOOSTime());
  m_Comms.Notify("GP9_Yaw", r.euler.get_scaled(2), MOOSTime());
  double dfHeading = (r.euler.get_scaled(2)*180/3.14159);
  m_Comms.Notify("GP9_Yaw_Heading", angle360(dfHeading), MOOSTime());

  m_Comms.Notify("GP9_VelE", r.velocity_e.get_scaled(1), MOOSTime());
  m_Comms.Notify("GP9_VelN", r.velocity_n.get_scaled(1), MOOSTime());
  m_Comms.Notify("GP9_PosE", r.pos_e.get_scaled(1), MOOSTime());
  m_Comms.Notify("GP9_PosN", r.pos_n.get_scaled(1), MOOSTime());

  // Temperature
  m_Comms.Notify("GP9_Temp1", r.temperature1.get_scaled(0), MOOSTime());

  // Position
  double lat = r.latitude.get_scaled(0);
  double lon = r.longitude.get_scaled(0);
  m_Comms.Notify("GP9_LAT", lat, MOOSTime());
  m_Comms.Notify("GP9_LONG", lon, MOOSTime());
  //convert to x,y
  if (lat != 0 && lon != 0) {
    double curX = 0.0;
    double curY = 0.0;
    bool bGeoSuccess = m_geodesy.LatLong2LocalUTM(lat, 
      lon, curY, curX);
    if (bGeoSuccess) {
      m_Comms.Notify("GP9_X", curX, MOOSTime());
      m_Comms.Notify("GP9_Y", curY, MOOSTime());
    }
  } else {
    //Pretend we are stationary at origin if no GPS signal
    m_Comms.Notify("GP9_X", 0.0, MOOSTime());
    m_Comms.Notify("GP9_Y", 0.0, MOOSTime());
  }

  m_Comms.Notify("GP9_GPS_SPEED", r.gps_speed.get_scaled(0), MOOSTime());
  m_Comms.Notify("GP9_GPS_HEADING", angle360(r.gps_course.get_scaled(0)), MOOSTime());
}
