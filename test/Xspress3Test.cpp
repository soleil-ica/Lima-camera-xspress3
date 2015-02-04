//###########################################################################
// This file is part of LImA, a Library for Image Acquisition
//
// Copyright (C) : 2009-2013
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
#include "lima/HwInterface.h"
#include "lima/CtControl.h"
#include "lima/CtAccumulation.h"
#include "lima/CtAcquisition.h"
#include "lima/CtSaving.h"
#include "lima/CtShutter.h"
#include "lima/Constants.h"

#include "Xspress3Camera.h"
#include "Xspress3Interface.h"
#include "lima/Debug.h"
#include <iostream>

using namespace std;
using namespace lima;
using namespace lima::Xspress3;

DEB_GLOBAL(DebModTest);

int main(int argc, char *argv[])
{
	DEB_GLOBAL_FUNCT();

	DebParams::setModuleFlags(DebParams::AllFlags);
	DebParams::setTypeFlags(DebParams::AllFlags);
	DebParams::setFormatFlags(DebParams::AllFlags);

	Camera *m_camera;
	Interface *m_interface;
	CtControl* m_control;

	int nbCards = 1;
	int maxFrames = 16384;
	string baseIPaddress= "192.168.0.1";
	int basePort = -1;
	string baseMACaddress = "02.00.00.00.00.00";
	int nbChans = 6;
	bool createModule = 1;
	string modname = "gm";
	int debug = 2;
	int cardIndex = 0;
	bool noUDP = 0;
       	string directoryName = "/home/xspress3/esrf/xspress3.8chan/settings";

	try {

		m_camera = new Camera(nbCards, maxFrames, baseIPaddress, basePort, baseMACaddress, nbChans,
				createModule, modname, debug, cardIndex, noUDP, directoryName);
		m_interface = new Interface(*m_camera);
		m_control = new CtControl(m_interface);

		// Setup user timing controls
		int nframes = 3;
		double m_exp_time = 5.0;

		// setup fileformat and data saving info
		CtSaving* saving = m_control->saving();
		saving->setDirectory("/home/xspress3/esrf/gmdata");
		saving->setFormat(CtSaving::HDF5);
	 	saving->setPrefix("xsp3_");
		saving->setSuffix(".hdf");
		saving->setSavingMode(CtSaving::Manual);
		saving->setManagedMode(CtSaving::Hardware);
		saving->setFramesPerFile(nframes);
		saving->setOverwritePolicy(CtSaving::Overwrite);

		// do acquisition
		m_camera->setCard(0);
		m_camera->setDataSource(-1, Camera::PlaybackStream0);
		m_camera->loadPlayback("/home/xspress3/esrf/Lima/camera/xspress3/test/Zr_mca15_pass0.d16", 0, 0);
		m_camera->setRunMode(true);
		m_camera->setUseDtc(false);
		//		m_camera->setDeadtimeCalculationEnergy(10000);
		//		m_camera->setDeadtimeCorrectionParameters(-1,2.5304E-9,2.2534E-7, 2.5304E-9,2.2534E-7, true, false);

		m_control->acquisition()->setAcqNbFrames(nframes);
		m_control->acquisition()->setAcqExpoTime(m_exp_time);
		m_camera->setTiming(0, 0, 0, 100);
		m_control->prepareAcq();
		m_control->startAcq();

	        struct timespec delay, remain;
		delay.tv_sec = (int)floor(m_exp_time/10.0);
		delay.tv_nsec = (int)(1E9*(m_exp_time/10.0-floor(m_exp_time/10.0)));
		DEB_TRACE() << "acq thread will sleep for " << m_exp_time/10.0 << " second";
		while (m_camera->isAcqRunning()) {
			delay.tv_sec = (int)floor(m_exp_time/10.0);
			delay.tv_nsec = (int)(1E9*(m_exp_time/10.0-floor(m_exp_time/10.0)));
			DEB_TRACE() << "sleep for " << m_exp_time/10.0 << " second";
			nanosleep(&delay, &remain);
		}
		DEB_TRACE() << "Finished collection 1";

		nframes = 2;
		m_exp_time = 1.0;

		long num;
		saving->getNextNumber(num);
		saving->setNextNumber(++num);
		m_control->acquisition()->setAcqNbFrames(nframes);
		m_control->acquisition()->setAcqExpoTime(m_exp_time);
		m_camera->setTiming(0, 0, 0, 100);
		m_control->prepareAcq();
		m_control->startAcq();

		delay.tv_sec = (int)floor(m_exp_time/10.0);
		delay.tv_nsec = (int)(1E9*(m_exp_time/10.0-floor(m_exp_time/10.0)));
		DEB_TRACE() << "acq thread will sleep for " << m_exp_time/10.0 << " second";
		while (m_camera->isAcqRunning()) {
			delay.tv_sec = (int)floor(m_exp_time/10.0);
			delay.tv_nsec = (int)(1E9*(m_exp_time/10.0-floor(m_exp_time/10.0)));
			DEB_TRACE()<< "sleep for " << m_exp_time/10.0 << " second";
			nanosleep(&delay, &remain);
		}

		DEB_TRACE() << "Finished collection 2";

	} catch (Exception& ex) {
		DEB_ERROR() << "LIMA Exception: " << ex;
	} catch (...) {
		DEB_ERROR() << "Unkown exception!";
	}
	return 0;
}
