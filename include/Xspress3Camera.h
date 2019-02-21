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

#ifndef XSPRESS3CAMERA_H_
#define XSPRESS3CAMERA_H_

#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <vector>
#include <strings.h>
#include "lima/HwMaxImageSizeCallback.h"
#include "lima/HwBufferMgr.h"
#include "lima/HwInterface.h"
#include <ostream>
#include "lima/Debug.h"
#include "processlib/Data.h"
#include "xspress3.h"
#include "Xspress3Interface.h"

using namespace std;

namespace lima {
namespace Xspress3 {

const int xPixelSize = 1;
const int yPixelSize = 1;

class BufferCtrlObj;

/*******************************************************************
 * \class Camera
 * \brief object controlling the Xspress3 camera
 *******************************************************************/
class Camera {
DEB_CLASS_NAMESPC(DebModCamera, "Camera", "Xspress3");

public:

	struct Xsp3Roi {
	public:
		friend std::ostream& operator<<(std::ostream& os, const Xsp3Roi& roi);
		Xsp3Roi(int num_roi) : m_num_roi(num_roi){}
		void addRegion(int index, int lhs, int rhs, int bins) {
			m_lhs[index] = lhs;
			m_rhs[index] = rhs;
			m_bins[index] = bins;
		}
		int getLhs(int index) {return m_lhs[index];}
		int getRhs(int index) {return m_rhs[index];}
		int getBins(int index) {return m_bins[index];}
		int getNumRoi() {return m_num_roi;}
	private:
		int m_num_roi;
		int m_lhs[8];
		int m_rhs[8];
		int m_bins[8];
	};
	enum Status {Idle, Paused, Running};
	enum DataSrc {
			Normal = XSP3_CC_SEL_DATA(XSP3_CC_SEL_DATA_NORMAL),			///< Input data from this channels ADC
			Alternate = XSP3_CC_SEL_DATA(XSP3_CC_SEL_DATA_ALTERNATE),	///< Input data from Alternate Channel
			Multiplexer = XSP3_CC_SEL_DATA(XSP3_CC_SEL_DATA_MUX_DATA),	///< Input Data from All channel multiplexer
			PlaybackStream0 = XSP3_CC_SEL_DATA(XSP3_CC_SEL_DATA_EXT0),	///< Input Data From Playback Stream 0
			PlaybackStream1 = XSP3_CC_SEL_DATA(XSP3_CC_SEL_DATA_EXT1),	///< Input Data From Playback Stream 1
	};

	enum ClockSrc {
		IntClk = XSP3_CLK_SRC_INT,		///< channel processing clock comes from fpga processor (testing only)
		XtalClk = XSP3_CLK_SRC_XTAL,	///< adc and channel processing clock from crystal on the ADC board (normal single board or master operation).
		ExtClk = XSP3_CLK_SRC_EXT,  	///< adc and channel processing clock from lemo clock connector on ADC board (slave boards)
		Future = XSP3_CLK_SRC_FPGA,		///< not implemented, for future expansion
		Mini = XSP3M_CLK_SRC_CDCM61004, //< X3m cdmc clock
		X4AdcLMK = XSP3M_CLK_SRC_LMK61E2,      ///< X4 ADC board lmk at 100mhz
		X4MplLMK = XSP4_CLK_SRC_MIDPLN_LMK61E2 ///< X4 midplane clock at 100mhz
	};

	enum ClockFlags {
		Master = XSP3_CLK_FLAGS_MASTER,				///< this clock generate clocks for other boards in the system
		NoDither =  XSP3_CLK_FLAGS_NO_DITHER,		///< disables dither within the ADC
		Stage1Only = XSP3_CLK_FLAGS_STAGE1_ONLY,	///< performs stage of the lmk 03200 setup, does not enable zero delay mode
		NoCheck = XSP3_CLK_FLAGS_NO_CHECK,			///< dont check for lock detect from lmk 03200
		TpEnb = XSP3_CLK_FLAGS_TP_ENB,				///< enable test pattern from spartans
		DisOverTemp = XSP3_CLK_FLAGS_DIS_OVER_TEMP,	///< Disable Over temperature protection on ADC Board
		Shutdown0 = XSP3_CLK_FLAGS_SHUTDOWN0,		///< Shutdown ADC channel 0
		Shutdown123 =  XSP3_CLK_FLAGS_SHUTDOWN123,	///< Shutdown ADC channels 123
		Shutdown4 =  XSP3_CLK_FLAGS_SHUTDOWN4,		///< Shutdown ADC channel 4 (middle (unused?))
		Shutdown5678 = XSP3_CLK_FLAGS_SHUTDOWN5678	///< Shutdown ADC channel5678 last 4
	};

	enum ItfgTriggerMode
	{
		Burst,				///<  Run burst of back to back frames.
		SoftwarePause,		///< Pause before every frame and wait for rising edge on CountEnb bit.
		HardwarePause,		///< Pause before every frame and wait for rising edge on TTL_IN(1).
		SoftwareOnlyFirst,	///< Pause before first frame and wait for rising edge on CountEnb bit.
		HardwareOnlyFirst	///< Pause before first frame and wait for rising edge on TTL_IN(1).
	};

	enum ItfgGapMode {
		Gap25ns,	///< Minimal gap between frames. Care when using multiple boxes. Short cables and/or termination. 0 debounce time.
		Gap200ns,	///< 200ns gap between frames. Use short cables and short (approx 10 cycle debounce time) when using multiple boxes.
		Gap500ns,	///< 500ns gap between frames. Use approx 30 cycle debounce time when using multiple boxes.
		Gap1us		///< 1us gap between frames. Allows long cables and  approx 70 cycle debounce time when using multiple boxes.
	};

	Camera(int nbCards, int nbFrames, string baseIPaddress, int basePort, string baseMACaddress, int nbChans,
		bool createScopeModule, string scopeModuleName, int debug, int cardIndex, bool noUDP, string directoryName);
	~Camera();

	void init();
	void reset();
	void prepareAcq();
	void startAcq();
	void stopAcq();
	void getStatus(Status& status);
	int getNbHwAcquiredFrames();

	// -- detector info object
	void getImageType(ImageType& type);
	void setImageType(ImageType type);
//
	void getDetectorType(std::string& type);
	void getDetectorModel(std::string& model);
	void getDetectorImageSize(Size& size);
	void getPixelSize(double& sizex, double& sizey);

	// -- Buffer control object
	HwBufferCtrlObj* getBufferCtrlObj();

	//-- Synch control object
	void setTrigMode(TrigMode mode);
	void getTrigMode(TrigMode& mode);

	void setExpTime(double exp_time);
	void getExpTime(double& exp_time);

	void setLatTime(double lat_time);
	void getLatTime(double& lat_time);

	void getExposureTimeRange(double& min_expo, double& max_expo) const;
	void getLatTimeRange(double& min_lat, double& max_lat) const;

	void setNbFrames(int nb_frames);
	void getNbFrames(int& nb_frames);

	bool isAcqRunning() const;

	///////////////////////////
	// -- xspress3 specific functions
	///////////////////////////

	void setupClocks(ClockSrc clk_src, ClockFlags flags, int tp_type=0);
	void setRunMode(bool playback=false, bool scope=false, bool scalers=true, bool hist=true);
	void getRunMode(bool& playback, bool& scope, bool& scalers, bool& hist);
	void getNbScalers(int& nscalers);
	void getRevision(int& revision);
	void initBrams(int chan);
	void setWindow(int chan, int win, int low, int high);
	void getWindow(int chan, int win, u_int32_t& low, u_int32_t& high);
	void setScaling(int chan, double scaling);
	void setGoodThreshold(int chan, int good_thres);
	void getGoodThreshold(int chan, u_int32_t& good_thres);
	void saveSettings();
	void restoreSettings(bool force_mismatch = false);
	void setRinging(int chan, double scale_a, int delay_a, double scale_b, int delay_b);

	void setDeadtimeCalculationEnergy(double energy);
	void getDeadtimeCalculationEnergy(double &energy);
	void setDeadtimeCorrectionParameters(int chan, double processDeadTimeAllEventGradient,
			double processDeadTimeAllEventOffset, double processDeadTimeInWindowOffset,
			double processDeadTimeInWindowGradient, bool useGoodEvent=false, bool omitChannel=false);
	void getDeadtimeCorrectionParameters(int chan, double &processDeadTimeAllEventGradient,
			double &processDeadTimeAllEventOffset, double &processDeadTimeInWindowOffset,
			double &processDeadTimeInWindowGradient, bool &useGoodEvent, bool &omitChannel);
	void setFanSetpoint(int deg_c);
	void getFanTemperatures(Data& temp);
	void setFanController(int p_term, int i_term);
	void arm();
	void pause();
	void restart();
	void start();
	void stop();
	void setClearMode(bool flag=true);
	void checkProgress(int& frameNos);
	void setCard(int card);
	void getCard(int& card);
	void getNumChan(int& num_chan);
	void getNumCards(int& num_cards);
	void getChansPerCard(int& chans_per_card);
	void getBinsPerMca(int& bins_per_mca);
	void getMaxNumChan(int &max_chan);
	void initRoi(int chan);
	void setRoi(int chan, Xsp3Roi& roi, int& nbins);
	void getUseDtc(bool &flag);
	void setUseDtc(bool flag);
	void readScalers(Data& temp, int frame_nb, int channel=-1);
	void readHistogram(Data& temp, int frame_nb, int channel=-1);
	void readRawHistogram(Data& histData, int frame_nb, int channel);
	void correctScalerData(double* buff, u_int32_t* fptr, int channe, double& dtcFactorl);
	void setAdcTempLimit(int temp);
	void setPlayback(bool enable);
	void loadPlayback(string filename, int src0, int src1, int streams=0, int digital=0);
	void startScope();
	void setTiming(int time_src, int fixed_time, int alt_ttl_mode, int debounce, bool loop_io=false,
			bool f0_invert=false, bool veto_invert=false);
	void setTimingMode();
	void formatRun(int chan, int nbits_eng=12, int aux1_mode=0, int adc_bits=0, int min_samples=0, int aux2_mode=0, bool pileup_reject=false);
	void getDataSource(int chan, DataSrc& data_src);
	void setDataSource(int chan, DataSrc data_src=Normal);
	void setItfgTiming(int nframes, int triggerMode, int gapMode);
	// internal only not for sip

private:
	class AcqThread;
	class ReadThread;

	// xspress3 specific
	int m_nb_cards;
	int m_max_frames;
	string m_baseIPaddress;
	int m_basePort;
	string m_baseMACaddress;
	int m_nb_chans;
	bool m_create_module;
	string m_modname;
	int m_card_index;
	int m_debug;
	int m_npixels;
	int m_nscalers;
	int m_handle;
	bool m_no_udp;
	string m_config_directory_name;
	Status m_status;
	bool m_use_dtc;
	bool m_clear_flag;
	int m_card;

	// Lima
	AcqThread *m_acq_thread;   // Thread to handle data acquisition
	ReadThread *m_read_thread; // Thread to handle reading the data into the Lima buffers and writing the output file
	TrigMode m_trigger_mode;
	double m_exp_time;
	ImageType m_image_type;
	int m_nb_frames; // nos of frames to acquire
	bool m_thread_running;
	bool m_wait_flag;
	bool m_read_wait_flag;
	bool m_quit;
	bool m_abort;
	int m_acq_frame_nb; // nos of frames acquired
	int m_read_frame_nb; // nos of frames readout
	mutable Cond m_cond;

	// Buffer control object
	SoftBufferCtrlObj m_bufferCtrlObj;

	void readFrame(void* ptr, int frame_nb);
};

inline std::ostream& operator<<(std::ostream& os, const Camera::Xsp3Roi& roi)
{
	for (int i=0; i<roi.m_num_roi; i++) {
		os << "<region= " << i << " lhs= " << roi.m_lhs[i] << " rhs= " << roi.m_rhs[i] << " bins= " << roi.m_bins[i] << ">";
	}
	return os;
}

inline Camera::ClockFlags operator|(Camera::ClockFlags a, Camera::ClockFlags b) {
  return Camera::ClockFlags(int(a) | int(b));
}

inline Camera::ClockFlags& operator|=(Camera::ClockFlags& a, Camera::ClockFlags b) {
  a = a | b;
  return a;
}

} // namespace Xspress3
} // namespace lima

#endif /* XSPRESS3CAMERA_H_ */
