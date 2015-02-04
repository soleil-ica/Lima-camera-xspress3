#include <stdlib.h>
#include <iostream>
#include "H5Cpp.h"
#include <sstream>
#include <vector>
#include <time.h>

using namespace H5;
using namespace std;

const int RANK_ONE = 1;
const int RANK_TWO = 2;
const int nchan = 4;
const int npixels = 4096;
const int nframes = 20;
const int nscalers = 8;
const string names[] =
		{ "Time", "ResetTicks", "ResetCount", "AllEvent", "AllGood", "InWindow0", "InWindow1", "PileUp" };


int main() {
	hsize_t dim[1];     /* scaler Dataspace dimensions */
	hsize_t dims[2];   /* histogram Dataspace dimensions */
	hsize_t dimss[2];   /* histogram Memspace dimensions */
	hsize_t dimp[1];     /* parameter Dataspace dimensions */
	hsize_t offset[2], count[2];
	int hist_data[npixels];
	int scaler_data[1];
	float hist_fdata[npixels];
	float scaler_fdata[1];
	int l;

	try
	{
		/*
		 * Turn off the auto-printing when failure occurs so that we can
		 * handle the errors appropriately
	 */
		Exception::dontPrint();

		H5File file = H5File("/home/xspress3/esrf/gmdata/test.hdf", H5F_ACC_TRUNC);

		/*
		 * Create a group in the file
		 */
		Group entry = Group(file.createGroup("/Entry"));
		{
			{
				// ISO 8601 Time format
				time_t now;
				time(&now);
				char buf[sizeof("2011-10-08T07:07:09Z")];
				strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&now));
				string stime = string(buf);
				hsize_t strdim[] = {1}; /* Dataspace dimensions */
				DataSpace dataspace(RANK_ONE, strdim);
				StrType datatype(H5T_C_S1, stime.size());
				DataSet dataset = DataSet(entry.createDataSet("start_time", datatype, dataspace));
				dataset.write(stime, datatype);
			}
			Group instrument = Group(entry.createGroup("Instrument"));
			Group detector = Group(instrument.createGroup("Xspress3"));
			{
				// write the firmware version
				string version = "V1.20";
				hsize_t strdim[] = {1}; /* Dataspace dimensions */
				DataSpace dataspace(RANK_ONE, strdim);
				StrType datatype(H5T_C_S1, version.size());
				DataSet dataset = DataSet(detector.createDataSet("firmware-version", datatype, dataspace));
				dataset.write(version, datatype);
			}
			{
				// write the dtc correction energy
				string dtc_energy = "1172eV";
				hsize_t strdim[] = {1}; /* Dataspace dimensions */
				DataSpace dataspace(RANK_ONE, strdim);
				StrType datatype(H5T_C_S1, dtc_energy.size());
				DataSet dataset = DataSet(detector.createDataSet("dtc-correction-energy", datatype, dataspace));
				dataset.write(dtc_energy, datatype);
			}
			DataSpace dataspace1; // scalers
			DataSpace memspace1; // scalers
			DataSpace dataspace2; // histogram
			DataSpace memspace2; // histogram
			DataSet dataset1[nscalers]; // scalers
			DataSet dataset2[nscalers]; // scalers
			DataSet dataset3; // histogram
			DataSet dataset4; // histogram-dtc
			Group scaler1, scaler2;
			Group group;
			for (int j = 0; j < nframes; j++) {
				l = j * 4096;
				for (int k = 0; k < npixels; k++) {
					hist_data[k] = l;
					hist_fdata[k] = (float) l++;
				}

				for (int i = 0; i < nchan; i++) {
					if (j == 0) {
						stringstream ss;
						ss << "channel_";
						(i < 10) ? ss << "0" << i : ss << i;
						group = Group(detector.createGroup(ss.str()));
						scaler1 = Group(group.createGroup("scaler"));
						scaler2 = Group(group.createGroup("scaler-dtc"));
					}
// scaler and scaler-dtc
					for (int k = 0; k < nscalers; k++) {
						scaler_data[0] = j + k * 20;
						scaler_fdata[0] = (float) j + k * 20;

						if (j == 0) {
							if (k == 0) {
								dim[0] = nframes; // frames
								dataspace1 = DataSpace(RANK_ONE, dim); // create new dspace
								dimss[0] = 1; // frames in slab
								memspace1 = DataSpace(RANK_ONE, dimss);
							}
							dataset1[k] = DataSet(scaler1.createDataSet(names[k], PredType::NATIVE_INT, dataspace1));
							dataset2[k] = DataSet(scaler2.createDataSet(names[k], PredType::NATIVE_FLOAT, dataspace1));
						}
						hsize_t offset2[1], count2[1];
						offset2[0] = j;
						count2[0] = 1;
						dataspace1.selectHyperslab(H5S_SELECT_SET, count2, offset2);
						std::cout << scaler_data[0] << std::endl;
						dataset1[k].write(scaler_data, PredType::NATIVE_INT, memspace1, dataspace1);
						dataspace1.selectHyperslab(H5S_SELECT_SET, count2, offset2);
						dataset2[k].write(scaler_fdata, PredType::NATIVE_FLOAT, memspace1, dataspace1);
					}
// histogram and histogram-dtd
					if (j == 0) {
						dims[1] = npixels; // pixels
						dims[0] = nframes; // total frames
						dataspace2 = DataSpace(RANK_TWO, dims); // create new dspace
						dataset3 = DataSet(group.createDataSet("histogram", PredType::NATIVE_INT, dataspace2));
						dataset4 = DataSet(group.createDataSet("histogram-dtc", PredType::NATIVE_FLOAT, dataspace2));

						dimss[1] = npixels; // pixels in slab
						dimss[0] = 1; // frames in slab
						memspace2 = DataSpace(RANK_TWO, dimss);
					}
					offset[0] = j;
					offset[1] = 0;
					count[0] = 1;
					count[1] = npixels;
					dataspace2.selectHyperslab(H5S_SELECT_SET, count, offset);
					dataset3.write(hist_data, PredType::NATIVE_INT, memspace2, dataspace2);

					dataspace2.selectHyperslab(H5S_SELECT_SET, count, offset);
					dataset4.write(hist_fdata, PredType::NATIVE_FLOAT, memspace2, dataspace2);
// correction parameters
					if (j == 0) {
						string dtc_names[] = { "processDeadTimeAllEventGradient", "processDeadTimeAllEventOffset",
								"processDeadTimeInWindowOffset", "processDeadTimeInWindowGradient" };
						double dtc_params[] = { 7.9E+06, 7.9E+06, 7.8e+06, 7.8e+06 };

						dimp[0] = 1;
						Group dtc = Group(group.createGroup("dtc-correction-parameters"));
						DataSpace dataspace = DataSpace(RANK_ONE, dimp); // create new dspace
						for (int i = 0; i < 4; i++) {
							DataSet dataset = DataSet(
									dtc.createDataSet(dtc_names[i], PredType::NATIVE_DOUBLE, dataspace));
							dataset.write(&dtc_params[i], PredType::NATIVE_DOUBLE);
						}
					}
				}
			}
		}
		{
			Group data = Group(entry.createGroup("Data"));
			/*
			 * Create hard link to the Data group.
			 */
			for (int i = 0; i < nchan; i++) {
				stringstream ss, ss2;
				ss << "channel_";
				(i < 10) ? ss << "0" << i : ss << i;
				ss2 << "/Entry/Instrument/Xspress3/" << ss.str() << "/histogram-dtc";
				try {
					data.link(H5L_TYPE_HARD, ss2.str(), ss.str());
				} catch (Exception &e) {
					std::cout << e.getCDetailMsg() << std::endl;
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
			hsize_t strdim[] = {1}; /* Dataspace dimensions */
			DataSpace dataspace(RANK_ONE, strdim);
			StrType datatype(H5T_C_S1, etime.size());
			DataSet dataset = DataSet(entry.createDataSet("end_time", datatype, dataspace));
			dataset.write(etime, datatype);
		}
	}
	// catch failure caused by the H5File operations
	catch (FileIException& error) {
		error.printError();
		return -1;
	}

	// catch failure caused by the DataSet operations
	catch (DataSetIException& error) {
		error.printError();
		return -1;
	}

	// catch failure caused by the DataSpace operations
	catch (DataSpaceIException& error) {
		error.printError();
		return -1;
	}
	return 0;
}
