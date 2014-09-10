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
// Xspress3SavingCtrlObj.cpp
// Created on: Jan 15, 2014
// Author: g.r.mant

#include "Xspress3Interface.h"
#include "Xspress3Camera.h"
#include "H5Cpp.h"

using namespace lima;
using namespace lima::Xspress3;
using namespace std;
using namespace H5;

// TODO: get these from the library as these should match m_nscalers == XSP3_SW_NUM_SCALERS!
const string names[] =
		{ "Time", "ResetTicks", "ResetCount", "AllEvent", "AllGood", "InWindow0", "InWindow1", "PileUp" };
static const char DIR_SEPARATOR = '/';
const int RANK_ONE = 1;
const int RANK_TWO = 2;

SavingCtrlObj::SavingCtrlObj(Camera& camera) :
		HwSavingCtrlObj(HwSavingCtrlObj::COMMON_HEADER | HwSavingCtrlObj::MANUAL_WRITE), m_cam(camera) {
}

SavingCtrlObj::~SavingCtrlObj() {
}

void SavingCtrlObj::getPossibleSaveFormat(std::list<std::string> &format_list) const {
	DEB_MEMBER_FUNCT();
	format_list.push_back(HwSavingCtrlObj::HDF5_FORMAT_STR);
}

void SavingCtrlObj::writeFrame(int frame_nr, int nb_frames) {
	DEB_MEMBER_FUNCT();
	HwFrameInfo frame_info;

	HwBufferCtrlObj* m_bufferCtrlObj = m_cam.getBufferCtrlObj();
	m_bufferCtrlObj->getFrameInfo(frame_nr, frame_info);
	float hist_fdata[m_npixels];
	double scaler_dtc_data[m_nscalers];

	DEB_TRACE() << "writing frame number " << DEB_VAR2(frame_nr, nb_frames);
	for (int i = 0; i < m_nchan; i++) {
		int l = frame_nr * 4096;
		for (int k = 0; k < 4096; k++) {
			hist_fdata[k] = (float) l++;
		}
		u_int32_t *fptr = (u_int32_t*)frame_info.frame_ptr;
		fptr += i * (m_npixels + m_nscalers) + m_npixels;
		m_cam.correctScalerData(scaler_dtc_data, fptr, i);
		for (int k = 0; k < m_nscalers; k++) {
			hsize_t slab_dim[] = {1}; // frames in slab
			DataSpace slabspace = DataSpace(RANK_ONE, slab_dim);
			hsize_t offset[] = {frame_nr};
			hsize_t count[] = {1};
			m_scaler_dataspace.selectHyperslab(H5S_SELECT_SET, count, offset);
			m_scaler_dataset[i][k].write(fptr+k, PredType::NATIVE_UINT32, slabspace, m_scaler_dataspace);
			if (m_useDTC) {
				m_scaler_dataspace.selectHyperslab(H5S_SELECT_SET, count, offset);
				m_scaler_dtc_dataset[i][k].write(scaler_dtc_data, PredType::NATIVE_DOUBLE, slabspace, m_scaler_dataspace);
			}
		}
		{
			u_int32_t *fptr = (u_int32_t*) frame_info.frame_ptr;
			fptr += i * (m_npixels + m_nscalers);
			// write the histogram and optionally the dead time corrected data
			hsize_t slab_dim[2];
			slab_dim[1] = m_npixels; // pixels in slab
			slab_dim[0] = 1; // frames in slab
			DataSpace slabspace = DataSpace(RANK_TWO, slab_dim);

			hsize_t offset[] = { frame_nr, 0 };
			hsize_t count[] = { 1, m_npixels };
			m_hist_dataspace.selectHyperslab(H5S_SELECT_SET, count, offset);
			m_hist_dataset[i].write(fptr, PredType::NATIVE_UINT32, slabspace, m_hist_dataspace);
			if (m_useDTC) {
				m_hist_dataspace.selectHyperslab(H5S_SELECT_SET, count, offset);
				m_hist_dtc_dataset[i].write(hist_fdata, PredType::NATIVE_FLOAT, slabspace, m_hist_dataspace);
			}
		}
	}
	if (frame_nr == m_nframes-1) {
		_close();
	}
}

void SavingCtrlObj::setCommonHeader(const HeaderMap& headerMap) {
	DEB_MEMBER_FUNCT();

	if (!headerMap.empty()) {
		Group header = Group(m_entry.createGroup("Header"));
		for (map<string, string>::const_iterator it = headerMap.begin(); it != headerMap.end(); it++) {

			string key = it->first;
			string value = it->second;
			hsize_t strdim[] = { 1 }; /* Dataspace dimensions */
			DataSpace dataspace(RANK_ONE, strdim);
			StrType datatype(H5T_C_S1, value.size());
			DataSet dataset = DataSet(header.createDataSet(key, datatype, dataspace));
			dataset.write(value, datatype);
		}
	}
}
void SavingCtrlObj::_close() {
	DEB_MEMBER_FUNCT();
	{
		Group data = Group(m_entry.createGroup("Data"));
		// Create hard link to the Data group.
		for (int i = 0; i < m_nchan; i++) {
			stringstream ss, ss2;
			ss << "channel_";
			(i < 10) ? ss << "0" << i : ss << i;
			if (m_useDTC) {
				ss2 << "/Entry/Instrument/Xspress3/" << ss.str() << "/histogram-dtc";
			} else {
				ss2 << "/Entry/Instrument/Xspress3/" << ss.str() << "/histogram";
			}
			data.link(H5L_TYPE_HARD, ss2.str(), ss.str());
		}
	}
	{
		// ISO 8601 Time format
		time_t now;
		time(&now);
		char buf[sizeof("2011-10-08T07:07:09Z")];
		strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&now));
		string etime = string(buf);
		hsize_t strdim[] = {1}; /* Dataspace dimensions */
		DataSpace dataspace(RANK_ONE, strdim);
		StrType datatype(H5T_C_S1, etime.size());
		DataSet dataset = DataSet(m_entry.createDataSet("end_time", datatype, dataspace));
		dataset.write(etime, datatype);
	}
	m_file.close();
	stop();
}

void SavingCtrlObj::_prepare() {
	DEB_MEMBER_FUNCT();

	std::string filename;
	if (m_suffix != ".hdf")
		THROW_HW_ERROR(lima::Error) << "Suffix must be .hdf";

	try
	{
		// Turn off the auto-printing when failure occurs so that we can
		// handle the errors appropriately
		H5::Exception::dontPrint();

		// Get the fully qualified filename
		char number[16];
		snprintf(number, sizeof(number), m_index_format.c_str(), m_next_number);
		filename = m_directory + DIR_SEPARATOR + m_prefix + number + m_suffix;

		H5File file = H5File(filename, H5F_ACC_TRUNC);

		// Create the root group in the file
		m_entry = Group(file.createGroup("/Entry"));
		m_cam.getUseDtc(m_useDTC);
		Size size;
		m_cam.getDetectorImageSize(size);
		m_nchan = size.getHeight();
		m_cam.getNbScalers(m_nscalers);
		m_npixels = size.getWidth() - m_nscalers;
		m_cam.getNbFrames(m_nframes);
		{
			// ISO 8601 Time format
			time_t now;
			time(&now);
			char buf[sizeof("2011-10-08T07:07:09Z")];
			strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&now));
			string stime = string(buf);
			hsize_t strdim[] = { 1 }; /* Dataspace dimensions */
			DataSpace dataspace(RANK_ONE, strdim);
			StrType datatype(H5T_C_S1, stime.size());
			DataSet dataset = DataSet(m_entry.createDataSet("start_time", datatype, dataspace));
			dataset.write(stime, datatype);
		}
		Group instrument = Group(m_entry.createGroup("Instrument"));
		Group detector = Group(instrument.createGroup("Xspress3"));
		{
			// write the firmware version
			int revision;
			m_cam.getRevision(revision);
			stringstream ss;
			ss << revision;
			string version = ss.str();;
			hsize_t strdim[] = { 1 }; /* Dataspace dimensions */
			DataSpace dataspace(RANK_ONE, strdim);
			StrType datatype(H5T_C_S1, version.size());
			DataSet dataset = DataSet(detector.createDataSet("firmware-version", datatype, dataspace));
			dataset.write(version, datatype);
		}
		{
			// write the dtc correction energy
			if (m_useDTC) {
				double dtc_energy;
				m_cam.getDeadtimeCalculationEnergy(dtc_energy);
				stringstream ss;
				ss << dtc_energy;
				string energy = ss.str();
				hsize_t strdim[] = { 1 }; /* Dataspace dimensions */
				DataSpace dataspace(RANK_ONE, strdim);
				StrType datatype(H5T_C_S1, energy.size());
				DataSet dataset = DataSet(detector.createDataSet("dtc-correction-energy", datatype, dataspace));
				dataset.write(energy, datatype);
			}
		}
		// create the scaler data structure in the file
		for (int i = 0; i < m_nchan; i++) {
			stringstream ss;
			ss << "channel_";
			(i < 10) ? ss << "0" << i : ss << i;
			Group group = Group(detector.createGroup(ss.str()));
			Group scaler1 = Group(group.createGroup("scaler"));
			Group scaler2;
			if (m_useDTC) {
				scaler2 = Group(group.createGroup("scaler-dtc"));
			}
			hsize_t data_dim[] = {m_nframes}; // nframes
			m_scaler_dataspace = DataSpace(RANK_ONE, data_dim); // create new dspace

			for (int k = 0; k < m_nscalers; k++) {
				m_scaler_dataset[i][k] = DataSet(scaler1.createDataSet(names[k], PredType::NATIVE_INT, m_scaler_dataspace));
				if (m_useDTC) {
					m_scaler_dtc_dataset[i][k] = DataSet(scaler2.createDataSet(names[k], PredType::NATIVE_FLOAT, m_scaler_dataspace));
				}
			}

			// create the histogram data structure in the file
			hsize_t data_dims[2];
			data_dims[1] = m_npixels; // pixels
			data_dims[0] = m_nframes; // total frames
			m_hist_dataspace = DataSpace(RANK_TWO, data_dims); // create new dspace
			m_hist_dataset[i] = DataSet(group.createDataSet("histogram", PredType::NATIVE_INT, m_hist_dataspace));
			if (m_useDTC) {
				m_hist_dtc_dataset[i] = DataSet(group.createDataSet("histogram-dtc", PredType::NATIVE_FLOAT, m_hist_dataspace));
			}

			// write the dead time correction parameters
			if (m_useDTC) {
				double processDeadTimeAllEventGradient;
				double processDeadTimeAllEventOffset;
				double processDeadTimeInWindowOffset;
				double processDeadTimeInWindowGradient;
				bool useGoodEvent;
				bool omitChannel;
				m_cam.getDeadtimeCorrectionParameters(i, processDeadTimeAllEventGradient, processDeadTimeAllEventOffset,
						processDeadTimeInWindowOffset, processDeadTimeInWindowGradient, useGoodEvent, omitChannel);

				hsize_t param_dim[] = {1};
				Group dtc = Group(group.createGroup("dtc-correction-parameters"));
				DataSpace dataspace = DataSpace(RANK_ONE, param_dim); // create new dspace
				DataSet dataset;
				dataset = DataSet(
						dtc.createDataSet("processDeadTimeAllEventGradient", PredType::NATIVE_DOUBLE, dataspace));
				dataset.write(&processDeadTimeAllEventGradient, PredType::NATIVE_DOUBLE);
				dataset = DataSet(
						dtc.createDataSet("processDeadTimeAllEventOffset", PredType::NATIVE_DOUBLE, dataspace));
				dataset.write(&processDeadTimeAllEventOffset, PredType::NATIVE_DOUBLE);
				dataset = DataSet(
						dtc.createDataSet("processDeadTimeInWindowOffset", PredType::NATIVE_DOUBLE, dataspace));
				dataset.write(&processDeadTimeInWindowOffset, PredType::NATIVE_DOUBLE);
				dataset = DataSet(
						dtc.createDataSet("processDeadTimeInWindowGradient", PredType::NATIVE_DOUBLE, dataspace));
				dataset.write(&processDeadTimeInWindowGradient, PredType::NATIVE_DOUBLE);
			}
		}
	} catch (FileIException &error) {
		THROW_CTL_ERROR(Error) << "File " << filename << " not opened successfully";
	}
	// catch failure caused by the DataSet operations
	catch (DataSetIException& error) {
		THROW_CTL_ERROR(Error) << "DataSet " << filename << " not created successfully";
		error.printError();
	}
	// catch failure caused by the DataSpace operations
	catch (DataSpaceIException& error) {
		THROW_CTL_ERROR(Error) << "DataSpace " << filename << " not created successfully";
	}
	// catch failure caused by any other HDF5 error
	catch (H5::Exception &e) {
		THROW_CTL_ERROR(Error) << e.getCDetailMsg();
	}
	// catch anything not hdf5 related
	catch (Exception &e) {
		THROW_CTL_ERROR(Error) << e.getErrMsg();
	}
}
