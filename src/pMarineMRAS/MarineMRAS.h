/************************************************************/
/*    NAME: Damian Manda                                    */
/*    ORGN: UNH                                             */
/*    FILE: MarineMRAS.h                                    */
/*    DATE: 2015-12-06                                      */
/************************************************************/

#ifndef MarineMRAS_HEADER
#define MarineMRAS_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "CourseChangeMRAS.h"
#include "CourseKeepMRAS.h"
#include "SpeedControl.h"
#include "SignalFilter.h"

enum class ControllerType {
  CourseChange,
  CourseKeep,
  CourseKeepNoAdapt
};

class MarineMRAS : public AppCastingMOOSApp
{
 public:
   MarineMRAS();
   ~MarineMRAS() {};


 protected: // Standard MOOSApp functions to overload
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();
   void UpdateROT(double curr_time);
   void PostAllStop();
   void AddHeadingHistory(double heading, double heading_time);
   ControllerType DetermineController();
   bool IsTurning();
   double GetSettleTime();

 protected: // Standard AppCastingMOOSApp function to overload
   bool buildReport();

 protected:
   void registerVariables();

 private: // Configuration variables
    double m_k_star;
    double m_tau_star;
    double m_z;
    double m_wn;
    double m_beta;
    double m_alpha;
    double m_gamma;
    double m_xi;
    double m_rudder_limit;
    double m_rudder_deadband;
    double m_max_ROT;
    double m_cruising_speed;
    double m_length;
    bool   m_decrease_adapt;
    double m_desired_thrust;
    bool   m_allstop_posted;
    double m_speed_factor;
    double m_max_thrust;
    double m_rudder_speed;
    bool   m_discard_large_ROT;
    bool   m_output;
    bool   m_record_mode;
    bool   m_course_keep_only;
    bool   m_course_change_only;
    bool   m_adapt_turns;
    bool   m_low_pass;
    double m_low_pass_freq;
    std::string m_speed_var;
    std::string m_cog_var;

 private: // State variables
    double m_desired_heading;
    double m_current_heading;
    double m_current_cog;
    double m_desired_speed;
    double m_current_speed;
    double m_current_speed_time;
    double m_last_heading_time;
    double m_end_last_turn;
    bool   m_first_heading;
    double m_previous_heading;
    double m_current_ROT;
    std::list<double> m_DiffHistory;
    int m_ROT_filter_len;
    bool   m_has_control;
    std::list<double> m_desired_heading_history;
    std::list<double> m_desired_hist_time;
    double m_last_iterate_time;
    double m_iterate_len;

    CourseChangeMRAS m_CourseControl;
    CourseKeepMRAS m_CourseKeepControl;
    SpeedControl m_speed_control;
    ControllerType m_last_controller;
    SignalFilter m_rot_filter;
    double m_raw_ROT;

};

#endif
