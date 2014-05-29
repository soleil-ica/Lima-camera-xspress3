/*
 * Xspress3SyncCtrlObj.cpp
 *
 *  Created on: Feb 04, 2013
 *      Author: gm
 */

#include <sstream>
#include "Xspress3Interface.h"
#include "Xspress3Camera.h"

using namespace lima;
using namespace lima::Xspress3;

SyncCtrlObj::SyncCtrlObj(Camera& cam) : m_cam(cam) {
	DEB_CONSTRUCTOR();
}

SyncCtrlObj::~SyncCtrlObj() {
	DEB_DESTRUCTOR();
}

bool SyncCtrlObj::checkTrigMode(TrigMode trig_mode) {
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(trig_mode);

	bool valid;
	switch (trig_mode) {
	case IntTrig:
	case IntTrigMult:
	case ExtGate:
		valid = true;
		break;
	default:
		valid = false;
		break;
	}
	DEB_RETURN() << DEB_VAR1(valid);
	return valid;
}

void SyncCtrlObj::setTrigMode(TrigMode trig_mode) {
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(trig_mode);

	if (!checkTrigMode(trig_mode)) {
		THROW_HW_ERROR(InvalidValue) << "Invalid "
					     << DEB_VAR1(trig_mode);
	}
	m_cam.setTrigMode(trig_mode);
}

void SyncCtrlObj::getTrigMode(TrigMode& trig_mode) {
	DEB_MEMBER_FUNCT();
	m_cam.getTrigMode(trig_mode);
}

void SyncCtrlObj::setExpTime(double exp_time) {
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(exp_time);
	m_cam.setExpTime(exp_time);
}

void SyncCtrlObj::getExpTime(double& exp_time) {
	DEB_MEMBER_FUNCT();
	m_cam.getExpTime(exp_time);
}

void SyncCtrlObj::setLatTime(double lat_time) {
	DEB_MEMBER_FUNCT();
	m_cam.setLatTime(lat_time);
}

void SyncCtrlObj::getLatTime(double& lat_time) {
	DEB_MEMBER_FUNCT();
	m_cam.getLatTime(lat_time);
}

void SyncCtrlObj::setNbHwFrames(int nb_frames) {
	DEB_MEMBER_FUNCT();
	m_cam.setNbFrames(nb_frames);
}

void SyncCtrlObj::getNbHwFrames(int& nb_frames) {
	DEB_MEMBER_FUNCT();
	m_cam.getNbFrames(nb_frames);
}

void SyncCtrlObj::getValidRanges(ValidRangesType& valid_ranges) {
	DEB_MEMBER_FUNCT();
    double min_time;
    double max_time;
    m_cam.getExposureTimeRange(min_time, max_time);
    valid_ranges.min_exp_time = min_time;
    valid_ranges.max_exp_time = max_time;

    m_cam.getLatTimeRange(min_time, max_time);
    valid_ranges.min_lat_time = min_time;
    valid_ranges.max_lat_time = max_time;
}

