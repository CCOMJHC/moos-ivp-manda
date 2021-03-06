/************************************************************/
/*    NAME: Damian Manda                                    */
/*    ORGN: UNH                                             */
/*    FILE: CourseKeepMRAS.cpp                              */
/*    DATE: 2015-01-15                                      */
/************************************************************/

#include "MOOS/libMOOS/MOOSLib.h"
#include "AngleUtils.h"
#include "CourseKeepMRAS.h"
#include <math.h>

#define KP_LIMIT 10
#define KI_ROT_THRESHOLD 0.15 //percent
#define MIN_SPEED 0.5
#define MIN_ADAPT_RUDDER 0.25  //percent

#define DEBUG false

using namespace std;

CourseKeepMRAS::CourseKeepMRAS() : m_dfKp{1}, m_dfKd{0.3}, m_dfTauM{0.5},
    m_dfKm{1.2}, m_bFilterROT{false} {
    m_bFirstRun = true;
    m_bControllerSwitch = false;
    m_bParametersSet = false;
    m_dfRudderOut = 0;
    //m_dfF = 1;
    m_dfMaxROTInc = 6;
    m_dfModelROT = 0;
    m_dfModelRudder = 0;
    m_dfKi = 0;
    m_dfZ = 1;
    m_dfWn = 1.2;

    //We should read this in as a parameter
    m_dfMu = 2.4;
    m_dfDeadband = 0;
}

void CourseKeepMRAS::SetParameters(double dfKStar, double dfTauStar, double dfZ,
    double dfWn, double dfBeta, double dfAlpha, double dfGamma, double dfXi,
    double dfRudderLimit, double dfCruisingSpeed, double dfShipLength,
    double dfMaxROT, bool bDecreaseAdapt, double dfRudderSpeed, double dfDeadband,
    double sample_T, bool filter_ROT)
{
    m_dfKmStar = dfKStar;
    m_dfTaumStar = dfTauStar;
    m_dfTauM = m_dfTaumStar * dfShipLength / dfCruisingSpeed;
    m_dfKm = m_dfKmStar * dfCruisingSpeed / dfShipLength;
    m_dfZ = dfZ;
    m_dfWn = dfWn;
    m_dfBeta = dfBeta;
    m_dfAlpha = dfAlpha;
    m_dfGamma = dfGamma;
    m_dfXi = dfXi;
    m_dfRudderLimit = dfRudderLimit;
    m_dfMaxROT = dfMaxROT;
    m_dfCruisingSpeed = dfCruisingSpeed;
    m_dfShipLength = dfShipLength;
    m_bDecreaseAdapt = bDecreaseAdapt;
    m_dfRudderSpeed = dfRudderSpeed;
    m_dfDeadband = dfDeadband;
    m_RotFilter = SignalFilter(0.25, sample_T);
    m_bFilterROT = filter_ROT;

    m_lIterations = 0;
    m_bParametersSet = true;
}

double CourseKeepMRAS::Run(double dfDesiredHeading, double dfMeasuredHeading,
    double dfMeasuredROT, double dfSpeed, double dfTime, bool bDoAdapt,
    bool bTurning)
{
    bool bAdaptLocal = bDoAdapt;
    // Don't adapt if we are going slow or straight (influence likely due to waves)
    if (dfSpeed < MIN_SPEED || (fabs(m_dfRudderOut - m_dfKi) < m_dfDeadband)
        || (!bTurning && fabs(m_dfModelRudder - m_dfKi) <
            MIN_ADAPT_RUDDER * m_dfRudderLimit)) {
        bAdaptLocal = false;
        #if DEBUG
        MOOSTrace("CourseKeep: No model adaptation\n");
        #endif
    }
    if (dfSpeed < MIN_SPEED)
        dfSpeed = MIN_SPEED;
    if (DEBUG)
        MOOSTrace("Using Course Keep Controller\n");

    dfMeasuredHeading = angle180(dfMeasuredHeading);
    dfDesiredHeading = angle180(dfDesiredHeading);

    if (m_bFirstRun && m_bParametersSet) {
        //Otherwise this will nearly always be zero and result in incorrect
        //initial values for Kp, etc
        InitModel(dfMeasuredHeading, dfMeasuredROT, m_dfCruisingSpeed);
        m_dfInitTime = dfTime;
        if (DEBUG)
            MOOSTrace("Model and Controller Initialized.\n");

        m_bFirstRun = false;
    } else if (m_bControllerSwitch) {
        m_bControllerSwitch = false;
    } else {
        //Normal operation
        double dfDeltaT = dfTime - m_dfPreviousTime;
        //This includes both the series and parallel models
        UpdateRudderModel(dfDeltaT);
        UpdateModel(dfMeasuredROT, m_dfModelRudder, dfSpeed, dfDeltaT,
          bAdaptLocal, bTurning);

        // double dfTimeReduceFactor = 1;
        // if (m_bDecreaseAdapt) {
        //     dfTimeReduceFactor = m_dfXi / (1 + dfTime - m_dfCourseChangeTime);
        // }

        // Determine the PID constants
        if (!bTurning) {
            m_dfKp = m_dfWn * m_dfWn * m_dfTauM / m_dfKm;
            if (m_dfKp > KP_LIMIT) {
                m_dfKp = KP_LIMIT;
            }
            m_dfKd = (2 * m_dfZ * sqrt(m_dfKp * m_dfKm *
                m_dfTauM) - 1) / (m_dfKm);
            if (m_dfKd < 0)
                m_dfKd = 0;
            else if (m_dfKd > (m_dfKp * m_dfShipLength / dfSpeed))
                m_dfKd = m_dfKp * m_dfShipLength / dfSpeed;

          m_dfKi = m_dfKim;
          m_dfKi = TwoSidedLimit(m_dfKi, 10 * dfSpeed / m_dfCruisingSpeed);
        }

    }
    if (DEBUG)
        MOOSTrace("PID Constants: Kp: %0.2f  Kd: %0.2f  Ki: %0.2f\n", m_dfKp,
            m_dfKd, m_dfKi);
    //PID equation
    double heading_error = angle180(dfDesiredHeading - dfMeasuredHeading);
    m_dfRudderOut = m_dfKp * heading_error - m_dfKd * dfMeasuredROT + m_dfKi;
    if (!bTurning && m_dfDeadband > 0 && (fabs(m_dfRudderOut - m_dfKi) < m_dfDeadband)) {
        //Ki is the rudder that needs to be carried to go straight, oscillations are about it
        m_dfRudderOut = m_dfKi;
    }

    m_dfPreviousTime = dfTime;
    m_dfPreviousHeading = dfDesiredHeading;
    m_dfMeasuredHeading = dfMeasuredHeading;
    m_dfMeasuredROT = dfMeasuredROT;

    //limit the rudder
    return TwoSidedLimit(m_dfRudderOut, m_dfRudderLimit);
}

void CourseKeepMRAS::InitModel(double dfHeading, double dfROT, double dfSpeed) {
    if (DEBUG)
        MOOSTrace("Mu: %0.2f\n", m_dfMu);

    //Model variables
    m_dfTauM = m_dfTaumStar * m_dfShipLength / dfSpeed;
    m_dfKm = m_dfKmStar * dfSpeed / m_dfShipLength;

    //m_dfKp = m_dfMu / 2;
    //m_dfKp = 1 / (4 * m_dfZ * m_dfZ * m_dfTauM);
    m_dfKp = m_dfWn * m_dfWn * m_dfTauM / m_dfKm;
    if (m_dfKp > KP_LIMIT) {
      m_dfKp = KP_LIMIT;
    }
    m_dfKd = (m_dfShipLength * 2 * m_dfZ * sqrt(m_dfKp * m_dfKmStar * m_dfTaumStar) - 1) /
        (dfSpeed * m_dfKmStar);
    if (m_dfKd < 0) {
        m_dfKd = 0;
    } else if (m_dfKd > (m_dfKp * m_dfShipLength / dfSpeed)) {
        m_dfKd = m_dfKp * m_dfShipLength / dfSpeed;
    }
    m_dfKim = 0;

    ResetModel(dfHeading, dfROT, dfSpeed);

    if (DEBUG)
        MOOSTrace("Adaptive Init: TauM*: %0.2f  Km*: %0.2f  Ki,m: %0.2f\n", m_dfTaumStar,
            m_dfKmStar, m_dfKim);
}

void CourseKeepMRAS::ResetModel(double dfHeading, double dfROT, double dfRudder) {
    m_dfModelHeading = dfHeading;
    m_dfModelROT = dfROT;
    m_dfModelPhiDotDot = m_dfKm / m_dfTauM * dfRudder - dfROT / m_dfTauM;
    if (DEBUG) {
        MOOSTrace("Model Init: Y..: %0.2f  Y.: %0.2f  Y: %0.2f\n", m_dfModelPhiDotDot,
            m_dfModelROT, m_dfModelHeading);
        MOOSTrace("Model Init: TauM*: %0.2f  Km*: %0.2f  Ki,m: %0.2f\n", m_dfTaumStar,
            m_dfKmStar, m_dfKim);
    }

    m_dfModelRudder = dfRudder;
    m_dfRudderOut = dfRudder;
}

void CourseKeepMRAS::SwitchController() {
    m_bControllerSwitch = true;
}

void CourseKeepMRAS::UpdateModel(double dfMeasuredROT, double dfRudder,
    double dfSpeed, double dfDeltaT, bool bDoAdapt, bool bTurning) {
    //Propagate model
    m_dfModelPhiDotDot = (m_dfKm * (dfRudder - m_dfKim) - m_dfModelROT) / m_dfTauM;
    m_dfModelROT += m_dfModelPhiDotDot * dfDeltaT;
    //this is the limit of ROT in this model
    // if (fabs(m_dfModelROT) > fabs(m_dfKm * dfRudder)) {
    //     m_dfModelROT = m_dfKm * dfRudder;
    // }
    m_dfModelHeading += m_dfModelROT * dfDeltaT;
    m_dfModelHeading = angle180(m_dfModelHeading);

    if (DEBUG) {
        MOOSTrace("Process Vars: Y.: %0.2f  dT: %0.2f\n",dfMeasuredROT, dfDeltaT);
        MOOSTrace("Model Params: Km: %0.2f  Taum: %0.2f\n", m_dfKm, m_dfTauM);
        MOOSTrace("Model Update: Y..: %0.2f  Y.: %0.2f  Y: %0.2f  Rudder: %0.2f\n",
            m_dfModelPhiDotDot, m_dfModelROT, m_dfModelHeading, dfRudder);
    }

    double rudder_for_adapt = dfRudder;
    if (m_bFilterROT) {
      m_dfFilteredROT = m_RotFilter.IngestValue(m_dfModelROT);
      m_rudder_hist.push_front(dfRudder);
      // Ten samples is the approximate filter time offset
      if (m_rudder_hist.size() > 10)
        m_rudder_hist.pop_back();
      rudder_for_adapt = m_rudder_hist.back();
    } else {
      m_dfFilteredROT = m_dfModelROT;
    }

    // Do adaptation, using filtered, since the measured is filtered.  Want the
    // same time delay.  For FIR filter this could be a index offset
    double dfe = m_dfFilteredROT - dfMeasuredROT;
    // Always adapt the Ki, want it modified even during deadband and waves
    // Avoid integral windup during turns
    if (!bTurning)
        m_dfKim += m_dfGamma * dfe * dfDeltaT;
    if (bDoAdapt) {
        double dfDeltaKmTm = (-m_dfBeta * dfe * (rudder_for_adapt - m_dfKim)) * dfDeltaT;
        double dfDeltaTmRecip = (m_dfAlpha * dfe * m_dfFilteredROT) * dfDeltaT;
        m_dfTaumStar = 1 / (1 / m_dfTaumStar + dfDeltaTmRecip);
        if (m_dfTaumStar < 0.1) {
            m_dfTaumStar = 0.1;
        } else if (m_dfTaumStar > 10) {
            //TODO: think over these limits;
            m_dfTaumStar = 10;
        }
        m_dfKmStar += dfDeltaKmTm * m_dfTaumStar;
        if (m_dfKmStar < 0.1) {
            m_dfKmStar = 0.1;
        } else if (m_dfKmStar > 10) {
            m_dfKmStar = 10;
        }

        if (DEBUG)
            MOOSTrace("Adaptive Update: TauM*: %0.2f  Km*: %0.2f  Ki,m: %0.2f\n", m_dfTaumStar,
                m_dfKmStar, m_dfKim);
    }

    //Potential to divide by zero here if dfSpeed == 0
    if (dfSpeed > MIN_SPEED) {
        m_dfTauM = m_dfTaumStar * m_dfShipLength / dfSpeed;
        m_dfKm = m_dfKmStar * dfSpeed / m_dfShipLength;
    }

}

void CourseKeepMRAS::UpdateRudderModel(double dfDeltaT) {
    // Update a model of the actual rudder location
    // This is needed for simple systems which do not have a sensor to provide this
    //feedback
    double dfRudderOut = TwoSidedLimit(m_dfRudderOut, m_dfRudderLimit);
    double dfRudderInc = dfDeltaT * m_dfRudderSpeed;
    double dfRudderDiff = dfRudderOut - m_dfModelRudder;
    if (fabs(dfRudderDiff) > dfRudderInc) {
        m_dfModelRudder += copysign(dfRudderInc, dfRudderDiff);
    } else {
        m_dfModelRudder = dfRudderOut;
    }
    if (DEBUG)
        MOOSTrace("Model Rudder: %0.2f  Rudder Out: %0.2f\n", m_dfModelRudder, dfRudderOut);
}

double CourseKeepMRAS::GetModelRudder() {
    return m_dfModelRudder;
}

double CourseKeepMRAS::GetTauStar() {
    return m_dfTaumStar;
}

double CourseKeepMRAS::GetTauM() {
    return m_dfTaumStar;
}

double CourseKeepMRAS::GetKStar() {
    return m_dfKmStar;
}

double CourseKeepMRAS::TwoSidedLimit(double dfNumToLimit, double dfLimit) {
    if (fabs(dfNumToLimit) > dfLimit) {
        return copysign(dfLimit, dfNumToLimit);
    }
    return dfNumToLimit;
}

string CourseKeepMRAS::GetStatusInfo() {
    stringstream info;
    info << m_dfKp << "|" << m_dfKd << "|" << m_dfKi << "|" << m_dfModelHeading
        << "|" << m_dfMeasuredHeading << "|" << m_dfPreviousHeading;
    return info.str();
}

string CourseKeepMRAS::GetDebugInfo() {
    stringstream info;
    info << m_dfKm << "|" << m_dfKmStar << "|" << m_dfTauM << "|"
         << m_dfTaumStar << "|" << m_dfModelROT << "|" << m_dfMeasuredROT;
    return info.str();
}

void CourseKeepMRAS::GetDebugVariables(double * vars) {
    vars[0] = m_dfKp;
    vars[1] = m_dfKd;
    vars[2] = m_dfKi;
    vars[3] = m_dfRudderOut;
    vars[4] = m_dfModelHeading;
    vars[5] = m_dfModelROT;
    vars[6] = m_dfTaumStar;  //m_dfSeriesHeading
    vars[7] = m_dfModelPhiDotDot; //m_dfSeriesROT
    vars[8] = m_dfTauM; //m_dfPsiRefP
    vars[9] = m_dfKm;   //m_dfPsiRefPP
    vars[10] = m_dfModelRudder;
    vars[11] = m_dfKmStar;
    vars[12] = m_dfFilteredROT;
}
