//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2011
// European Synchrotron Radiation Facility
// BP 220, Grenoble 38043
// FRANCE
//
// This is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//###########################################################################
//
// Xspress3Interface.cpp
// Created on: Sep 20, 2013
// Author: g.r.mant

#include "Xspress3Interface.h"
#include "Xspress3Camera.h"

using namespace lima;
using namespace lima::Xspress3;

Interface::Interface(Camera& cam) :
		m_cam(cam), m_det_info(cam), m_sync(cam)
{
	DEB_CONSTRUCTOR();
	HwDetInfoCtrlObj *det_info = &m_det_info;
	m_cap_list.push_back(det_info);

	m_bufferCtrlObj = m_cam.getBufferCtrlObj();
	HwBufferCtrlObj *buffer = m_cam.getBufferCtrlObj();
	m_cap_list.push_back(buffer);

	HwSyncCtrlObj *sync = &m_sync;
	m_cap_list.push_back(sync);

	m_saving = m_cam.getSavingCtrlObj();
	HwSavingCtrlObj *saving = m_saving;
	m_cap_list.push_back(saving);

	m_sync.setNbFrames(1);
	m_sync.setExpTime(1.0);
	m_sync.setLatTime(0.0);
	m_sync.setTrigMode(IntTrig);
	Size image_size;
	m_det_info.getMaxImageSize(image_size);
	ImageType image_type;
	m_det_info.getDefImageType(image_type);
	FrameDim frame_dim(image_size, image_type);
	m_bufferCtrlObj->setFrameDim(frame_dim);
	m_bufferCtrlObj->setNbConcatFrames(1);
	m_bufferCtrlObj->setNbBuffers(2);
}

Interface::~Interface() {
	DEB_DESTRUCTOR();
	delete &m_det_info;
	delete &m_sync;
//	delete &m_saving;
}

void Interface::getCapList(CapList &cap_list) const {
	DEB_MEMBER_FUNCT();
	cap_list = m_cap_list;
}

void Interface::reset(ResetLevel reset_level) {
	DEB_MEMBER_FUNCT();
	DEB_PARAM() << DEB_VAR1(reset_level);

	stopAcq();
	m_cam.reset();
}

void Interface::prepareAcq() {
	DEB_MEMBER_FUNCT();
	m_cam.prepareAcq();
}

void Interface::startAcq() {
	DEB_MEMBER_FUNCT();
	m_cam.startAcq();
}

void Interface::stopAcq() {
	DEB_MEMBER_FUNCT();
	m_cam.stopAcq();
}

void Interface::getStatus(StatusType& status) {
	DEB_MEMBER_FUNCT();
	Camera::Status xspress3_status;
	m_cam.getStatus(xspress3_status);
	switch (xspress3_status) {
	case Camera::Idle:
		status.acq = AcqReady;
		status.det = DetIdle;
		break;
	case Camera::Paused:
		status.det = DetWaitForTrigger;
		status.acq = AcqRunning;
		break;
	case Camera::Running:
		status.det = DetExposure;
		status.acq = AcqRunning;
		break;
	}
}

int Interface::getNbHwAcquiredFrames() {
	DEB_MEMBER_FUNCT();
	return m_cam.getNbHwAcquiredFrames();
}
