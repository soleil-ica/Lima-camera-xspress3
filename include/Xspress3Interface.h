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
// Xspress3Interface.h
// Created on: Sep 20, 2013
// Author: g.r.mant

#ifndef XSPRESS3INTERFACE_H_
#define XSPRESS3INTERFACE_H_

#include "HwInterface.h"
#include "H5Cpp.h"

namespace lima {
namespace Xspress3 {

class Interface;
class Camera;

/*******************************************************************
 * \class SavingCtrlObj
 * \brief Control object providing Xspress3 saving interface
 *******************************************************************/

class SavingCtrlObj: public HwSavingCtrlObj {
DEB_CLASS_NAMESPC(DebModCamera, "SavingCtrlObj","Xspress3");

public:
	SavingCtrlObj(Camera& cam);
	virtual ~SavingCtrlObj();

	virtual void getPossibleSaveFormat(std::list<std::string> &format_list) const;
	virtual void writeFrame(int frame_nr = -1, int nb_frames = 1);
	virtual void setCommonHeader(const HeaderMap&);

private:
	void _prepare();
	void _close();

	Camera& m_cam;
	int m_nchan;
	int m_nscalers;
	int m_npixels;
	int m_nframes;
	H5::H5File m_file;
	H5::Group m_entry;
// TODO: change this hardcoded asap!
	H5::DataSet m_hist_dataset[9];
	H5::DataSet m_hist_dtc_dataset[9];
	H5::DataSpace m_hist_dataspace;
	H5::DataSet m_scaler_dataset[9][8];
	H5::DataSet m_scaler_dtc_dataset[9][8];
	H5::DataSpace m_scaler_dataspace;

	bool m_useDTC;
};

/*******************************************************************
 * \class DetInfoCtrlObj
 * \brief Control object providing Xspress3 detector info interface
 *******************************************************************/

class DetInfoCtrlObj: public HwDetInfoCtrlObj {
DEB_CLASS_NAMESPC(DebModCamera, "DetInfoCtrlObj","Xspress3");

public:
	DetInfoCtrlObj(Camera& cam);
	virtual ~DetInfoCtrlObj();

	virtual void getMaxImageSize(Size& max_image_size);
	virtual void getDetectorImageSize(Size& det_image_size);

	virtual void getDefImageType(ImageType& def_image_type);
	virtual void getCurrImageType(ImageType& curr_image_type);
	virtual void setCurrImageType(ImageType curr_image_type);

	virtual void getPixelSize(double& x_size, double &y_size);
	virtual void getDetectorType(std::string& det_type);
	virtual void getDetectorModel(std::string& det_model);

	virtual void registerMaxImageSizeCallback(HwMaxImageSizeCallback& cb);
	virtual void unregisterMaxImageSizeCallback(HwMaxImageSizeCallback& cb);

private:
	Camera& m_cam;
};

/*******************************************************************
 * \class SyncCtrlObj
 * \brief Control object providing Xspress3 synchronization interface
 *******************************************************************/

class SyncCtrlObj: public HwSyncCtrlObj {
DEB_CLASS_NAMESPC(DebModCamera,"SyncCtrlObj","Xspress3");

public:
	SyncCtrlObj(Camera& cam); //, BufferCtrlObj& buffer);
	virtual ~SyncCtrlObj();

	virtual bool checkTrigMode(TrigMode trig_mode);
	virtual void setTrigMode(TrigMode trig_mode);
	virtual void getTrigMode(TrigMode& trig_mode);

	virtual void setExpTime(double exp_time);
	virtual void getExpTime(double& exp_time);

	virtual void setLatTime(double lat_time);
	virtual void getLatTime(double& lat_time);

	virtual void setNbHwFrames(int nb_frames);
	virtual void getNbHwFrames(int& nb_frames);

	virtual void getValidRanges(ValidRangesType& valid_ranges);

private:
	Camera& m_cam;
};

/*******************************************************************
 * \class Interface
 * \brief Xspress3 hardware interface
 *******************************************************************/

class Interface: public HwInterface {
DEB_CLASS_NAMESPC(DebModCamera, "Interface", "Xspress3");

public:
	Interface(Camera& cam);
	virtual ~Interface();
	virtual void getCapList(CapList&) const;
	virtual void reset(ResetLevel reset_level);
	virtual void prepareAcq();
	virtual void startAcq();
	virtual void stopAcq();
	virtual void getStatus(StatusType& status);
	virtual int getNbHwAcquiredFrames();

private:
	Camera& m_cam;
	CapList m_cap_list;
	DetInfoCtrlObj m_det_info;
	HwBufferCtrlObj*  m_bufferCtrlObj;
	SyncCtrlObj m_sync;
	SavingCtrlObj* m_saving;
};

} // namespace Xspress3
} // namespace lima

#endif /* XSPRESS3INTERFACE_H_ */
