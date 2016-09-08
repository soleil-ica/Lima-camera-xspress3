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
static const string names[] =
  { "Time", "ResetTicks", "ResetCount", "AllEvent", "AllGood", "InWindow0", "InWindow1", "PileUp", "TotalTicks" };
static const char DIR_SEPARATOR = '/';
static const int RANK_ONE = 1;
static const int RANK_TWO = 2;

/* Static function helper*/
DataType get_h5_type(unsigned char)		{return PredType(PredType::NATIVE_UINT8);}
DataType get_h5_type(char)			{return PredType(PredType::NATIVE_INT8);}
DataType get_h5_type(unsigned short)		{return PredType(PredType::NATIVE_UINT16);}
DataType get_h5_type(short)			{return PredType(PredType::NATIVE_INT16);}
DataType get_h5_type(unsigned int)		{return PredType(PredType::NATIVE_UINT32);}
DataType get_h5_type(int)			{return PredType(PredType::NATIVE_INT32);}
DataType get_h5_type(unsigned long long)	{return PredType(PredType::NATIVE_UINT64);}
DataType get_h5_type(long long)			{return PredType(PredType::NATIVE_INT64);}
DataType get_h5_type(float)			{return PredType(PredType::NATIVE_FLOAT);}
DataType get_h5_type(double)			{return PredType(PredType::NATIVE_DOUBLE);}
DataType get_h5_type(std::string& s)            {return StrType(H5T_C_S1, s.size());}
DataType get_h5_type(bool)			{return PredType(PredType::NATIVE_UINT8);}

template<class T>
void write_h5_dataset(Group group, const char* entry_name, T& val) {
	DataSpace dataspace(H5S_SCALAR);
	DataType datatype = get_h5_type(val);
	DataSet dataset(group.createDataSet(entry_name, datatype, dataspace));
	dataset.write(&val, datatype);
}

template<>
void write_h5_dataset(Group group, const char* entry_name, std::string& val) {
	DataSpace dataspace(H5S_SCALAR);
	DataType datatype = get_h5_type(val);
	DataSet dataset(group.createDataSet(entry_name, datatype, dataspace));
	dataset.write(val.c_str(), datatype);
}

template <class L,class T>
void write_h5_attribute(L location,const char* entry_name,T& val)
{
       DataSpace dataspace(H5S_SCALAR);
       DataType datatype = get_h5_type(val);
       Attribute attr(location.createAttribute(entry_name,datatype, dataspace));
       attr.write(datatype, &val);
}

template <class L>
void write_h5_attribute(L location,const char* entry_name,std::string& val)
{
       DataSpace dataspace(H5S_SCALAR);
       DataType datatype = get_h5_type(val);
       Attribute attr(location.createAttribute(entry_name,datatype, dataspace));
       attr.write(datatype, val.c_str());
    
}

SavingCtrlObj::SavingCtrlObj(Camera& camera) :
		HwSavingCtrlObj(HwSavingCtrlObj::COMMON_HEADER | HwSavingCtrlObj::MANUAL_WRITE), m_cam(camera) {
}

SavingCtrlObj::~SavingCtrlObj() {
  DEB_DESTRUCTOR();
}

void SavingCtrlObj::_prepare() {
	DEB_MEMBER_FUNCT();

	std::string filename;
	if (m_suffix != ".hdf")
		THROW_HW_ERROR(lima::Error) << "Suffix must be .hdf";

	try {
		// Turn off the auto-printing when failure occurs so that we can
		// handle the errors appropriately
		H5::Exception::dontPrint();

		// Get the fully qualified filename
		char number[16];
		snprintf(number, sizeof(number), m_index_format.c_str(), m_next_number);
		filename = m_directory + DIR_SEPARATOR + m_prefix + number + m_suffix;
		DEB_TRACE() << "Opening filename " << filename << " with overwritePolicy " << m_overwritePolicy;

		if (m_overwritePolicy == "Overwrite") {
			// overwrite existing file
			m_file = new H5File(filename, H5F_ACC_TRUNC);
		} else if (m_overwritePolicy == "Abort") {
			// fail if file already exists
			m_file = new H5File(filename.c_str(), H5F_ACC_EXCL);
		} else {
			THROW_CTL_ERROR(Error) << "Append and multiset  not supported !";
		}
		//		m_file = new H5File(filename, H5F_ACC_TRUNC);

		m_entry = new Group(m_file->createGroup("/entry"));
		string nxentry = "NXentry";
		write_h5_attribute(*m_entry, "NX_class", nxentry);
		string title = "Lima Xspress3 detector";
		write_h5_dataset(*m_entry, "title", title);

		m_cam.getUseDtc(m_useDTC);
		DEB_TRACE() << DEB_VAR1(m_useDTC);
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
			write_h5_dataset(*m_entry, "start_time", stime);
		}
		Group instrument = Group(m_entry->createGroup("Instrument"));
		string nxinstrument = "NXinstrument";
		write_h5_attribute(instrument, "NX_class", nxinstrument);
		Group detector = Group(instrument.createGroup("Xspress3"));
		string nxdetector = "NXdetector";
		write_h5_attribute(detector, "NX_class", nxdetector);
		{
			// write the firmware version
			int revision;
			m_cam.getRevision(revision);
			//			DEB_TRACE() << "write thefirmware version " << DEB_VAR1(revision);
			stringstream ss;
			ss << revision;
			string version = ss.str();
			write_h5_dataset(detector, "firmware-version", version);
		}
		{
			// write the dtc correction energy
			if (m_useDTC) {
				double dtc_energy;
				m_cam.getDeadtimeCalculationEnergy(dtc_energy);
				//				DEB_TRACE() << "write the dtc correction energy " << DEB_VAR1(dtc_energy);
				stringstream ss;
				ss << dtc_energy;
				string energy = ss.str();
				write_h5_dataset(detector, "dtc-correction-energy" "dtc-correction-energy", energy);
			}
		}
		// how many channels to save?
		std::vector<bool> saveChannels = m_cam.getSaveChannels();
		m_saving_nchan = m_nchan; //0;
		for (std::vector<bool>::iterator it = saveChannels.begin(); it != saveChannels.end(); ++it) {
		    if (*it) {
		        m_saving_nchan++;
		    }
		}
		DEB_TRACE() << "Saving " << m_saving_nchan << " channels out of " << m_nchan;
		// create the scaler data structure in the file
		m_scaler_dataset = new DataSet[m_saving_nchan * m_nscalers];
		m_scaler_dtc_dataset = new DataSet[m_saving_nchan * m_nscalers];
		m_hist_dataset = new DataSet[m_saving_nchan];
		m_hist_dtc_dataset = new DataSet[m_saving_nchan];
		for (int i = 0; i < m_nchan; i++) {
		     if (saveChannels[i]) {
			  DEB_TRACE() << "Create the scaler data structure in the file for channel " << i;
				stringstream ss;
				ss << "channel_";
				(i < 10) ? ss << "0" << i : ss << i;
				Group group = Group(detector.createGroup(ss.str()));
				Group scaler = Group(group.createGroup("scaler"));
				Group scaler_dtc;
				if (m_useDTC) {
					scaler_dtc = Group(group.createGroup("scaler-dtc"));
				}

				hsize_t data_dim[] = { (unsigned) m_nframes }; // nframes
				//			DEB_TRACE() << DEB_VAR1(m_nframes);
				m_scaler_dataspace = new DataSpace(RANK_ONE, data_dim); // create new dspace
				//			DEB_TRACE() << DEB_VAR1(m_nscalers);
				for (int k = 0; k < m_nscalers; k++) {
					m_scaler_dataset[i * m_saving_nchan + k] = DataSet(
							scaler.createDataSet(names[k], PredType::NATIVE_INT, *m_scaler_dataspace));
				}
				if (m_useDTC) {
					for (int k = 0; k < m_nscalers; k++) {
						m_scaler_dtc_dataset[i * m_saving_nchan + k] = DataSet(
								scaler_dtc.createDataSet(names[k], PredType::NATIVE_FLOAT, *m_scaler_dataspace));
					}
				}
				DEB_TRACE() << "create the histogram data structure in the file for channel " << i;
				// create the histogram data structure in the file
				hsize_t data_dims[2];
				data_dims[1] = m_npixels; // pixels
				data_dims[0] = m_nframes; // total frames
				m_hist_dataspace = new DataSpace(RANK_TWO, data_dims); // create new dspace
				m_hist_dataset[i] = DataSet(group.createDataSet("histogram", PredType::NATIVE_INT, *m_hist_dataspace));
				if (m_useDTC) {
					m_hist_dtc_dataset[i] = DataSet(
							group.createDataSet("histogram-dtc", PredType::NATIVE_FLOAT, *m_hist_dataspace));
				}
				// write the dead time correction parameters
				if (m_useDTC) {
					//			  DEB_TRACE() << "write the dead time correction parameters for channel " << i;
					double processDeadTimeAllEventGradient;
					double processDeadTimeAllEventOffset;
					double processDeadTimeInWindowOffset;
					double processDeadTimeInWindowGradient;
					bool useGoodEvent;
					bool omitChannel;
					m_cam.getDeadtimeCorrectionParameters(i, processDeadTimeAllEventGradient, processDeadTimeAllEventOffset,
							processDeadTimeInWindowOffset, processDeadTimeInWindowGradient, useGoodEvent, omitChannel);

					hsize_t param_dim[] = { 1 };
					Group dtc = Group(group.createGroup("dtc-correction-parameters"));
					DataSpace dataspace = DataSpace(RANK_ONE, param_dim); // create new dspace
					DataSet dataset;
					dataset = DataSet(dtc.createDataSet("processDeadTimeAllEventGradient", PredType::NATIVE_DOUBLE, dataspace));
					dataset.write(&processDeadTimeAllEventGradient, PredType::NATIVE_DOUBLE);
					dataset = DataSet(dtc.createDataSet("processDeadTimeAllEventOffset", PredType::NATIVE_DOUBLE, dataspace));
					dataset.write(&processDeadTimeAllEventOffset, PredType::NATIVE_DOUBLE);
					dataset = DataSet(dtc.createDataSet("processDeadTimeInWindowOffset", PredType::NATIVE_DOUBLE, dataspace));
					dataset.write(&processDeadTimeInWindowOffset, PredType::NATIVE_DOUBLE);
					dataset = DataSet(dtc.createDataSet("processDeadTimeInWindowGradient", PredType::NATIVE_DOUBLE, dataspace));
					dataset.write(&processDeadTimeInWindowGradient, PredType::NATIVE_DOUBLE);
				}
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

void SavingCtrlObj::close() {
	_close();
}


void SavingCtrlObj::_close() {
	DEB_MEMBER_FUNCT();

	{
		std::vector<bool> saveChannels = m_cam.getSaveChannels();

		Group data = Group(m_entry->createGroup("Data"));
		// Create hard link to the Data group.
		for (int i = 0; i < m_nchan; i++) {
		  if (saveChannels[i]) {
				stringstream ss, ss2;
				ss << "channel_";
				(i < 10) ? ss << "0" << i : ss << i;
				if (m_useDTC) {
					ss2 << "/entry/Instrument/Xspress3/" << ss.str() << "/histogram-dtc";
				} else {
					ss2 << "/entry/Instrument/Xspress3/" << ss.str() << "/histogram";
				}
				data.link(H5L_TYPE_HARD, ss2.str(), ss.str());
		      	}
		}
	}
	{
		// ISO 8601 Time format
		time_t now;
		time(&now);
		char buf[sizeof("2011-10-08T07:07:09Z")];
		strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&now));
		string etime = string(buf);
		write_h5_dataset(*m_entry, "end_time", etime);
	}

	m_file->close();
	stop();
	delete m_hist_dataspace;
	delete m_scaler_dataspace;
	delete[] m_hist_dtc_dataset;
	delete[] m_hist_dataset;
	delete[] m_scaler_dtc_dataset;
	delete[] m_scaler_dataset;
	delete m_entry;
	delete m_file;
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
	float hist_dtc_data[m_npixels];
	double scaler_dtc_data[m_nscalers];
	double dtcFactor;

	std::vector<bool> saveChannels = m_cam.getSaveChannels();
	DEB_TRACE() << "writing frame number " << DEB_VAR2(frame_nr, nb_frames);
	for (int i = 0; i < m_nchan; i++) {
	     if (saveChannels[i]) {
			u_int32_t *scaler_data = (u_int32_t*) frame_info.frame_ptr;
			scaler_data += i * (m_npixels + m_nscalers) + m_npixels;
			if (m_useDTC) {
				m_cam.correctScalerData(scaler_dtc_data, scaler_data, i, dtcFactor);
			}
			for (int k = 0; k < m_nscalers; k++) {
				hsize_t slab_dim[] = { 1 }; // frames in slab
				DataSpace slabspace = DataSpace(RANK_ONE, slab_dim);
				hsize_t offset[] = { (unsigned)frame_nr };
				hsize_t count[] = { 1 };
				m_scaler_dataspace->selectHyperslab(H5S_SELECT_SET, count, offset);
				m_scaler_dataset[i * m_saving_nchan + k].write(scaler_data + k, PredType::NATIVE_UINT32, slabspace, *m_scaler_dataspace);
				if (m_useDTC) {
					m_scaler_dataspace->selectHyperslab(H5S_SELECT_SET, count, offset);
					m_scaler_dtc_dataset[i * m_saving_nchan + k].write(scaler_dtc_data + k, PredType::NATIVE_DOUBLE, slabspace,
							*m_scaler_dataspace);
				}
			}

			{
				DEB_TRACE() << "writing histogram data ";
				u_int32_t *hist_data = (u_int32_t*) frame_info.frame_ptr;
				hist_data += i * (m_npixels + m_nscalers);
				// write the histogram and optionally the dead time corrected data
				hsize_t slab_dim[2];
				slab_dim[1] = m_npixels; // pixels in slab
				slab_dim[0] = 1; // frames in slab
				DataSpace slabspace = DataSpace(RANK_TWO, slab_dim);

				hsize_t offset[] = { (unsigned) frame_nr, 0 };
				hsize_t count[] = { 1, (unsigned) m_npixels };
				m_hist_dataspace->selectHyperslab(H5S_SELECT_SET, count, offset);
				m_hist_dataset[i].write(hist_data, PredType::NATIVE_UINT32, slabspace, *m_hist_dataspace);
				if (m_useDTC) {
					for (int j = 0; j < m_npixels; j++) {
						hist_dtc_data[j] = hist_data[j] * dtcFactor;
					}
					m_hist_dataspace->selectHyperslab(H5S_SELECT_SET, count, offset);
					m_hist_dtc_dataset[i].write(hist_dtc_data, PredType::NATIVE_FLOAT, slabspace, *m_hist_dataspace);
				}
			}
	       }
	}
	if (frame_nr == m_nframes - 1) {
		_close();
	}
}

void SavingCtrlObj::setCommonHeader(const HeaderMap& headerMap) {
	DEB_MEMBER_FUNCT();

	if (!headerMap.empty()) {
		Group header = Group(m_entry->createGroup("Header"));
		for (map<string, string>::const_iterator it = headerMap.begin(); it != headerMap.end(); it++) {
			string key = it->first;
			string value = it->second;
			write_h5_dataset(header, key.c_str(), value);
		}
	}
}
