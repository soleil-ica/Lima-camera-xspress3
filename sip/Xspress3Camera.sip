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

namespace Xspress3
{

  /*******************************************************************
   * \class Camera
   * \brief object controlling the xspress3 detector. 
   *******************************************************************/
  class Camera
  {
%TypeHeaderCode
#include <Xspress3Camera.h>
#include <string>
%End

  public:

	struct Xsp3Roi {
	public:
		Xsp3Roi(int num_roi);
		void addRegion(int index, int lhs, int rhs, int bins);
		int getLhs(int index);
		int getRhs(int index);
		int getBins(int index);
		int getNumRoi();
	};

	enum Status {Idle, Paused, Running};
	enum DataSrc {Normal, Alternate, Multiplexer, PlaybackStream0, PlaybackStream1};

	Camera(int nbCards, int nbFrames, std::string baseIPaddress, int basePort, std::string baseMACaddress, int nbChans,
			bool createScopeModule, std::string scopeModuleName, int debug, int cardIndex, bool noUDP, std::string directoryName);
	~Camera();

	void init();
	void reset();
	void prepareAcq();
	void startAcq();
	void stopAcq();
	void getStatus(Status& status /Out/);
	int getNbHwAcquiredFrames();

	// -- detector info object
	void getImageType(ImageType& type /Out/);
	void setImageType(ImageType type);

	void getDetectorType(std::string& type /Out/);
	void getDetectorModel(std::string& model /Out/);
	void getDetectorImageSize(Size& size /Out/);
	void getPixelSize(double& sizex, double& sizey /Out/);

	// -- Buffer control object
	HwBufferCtrlObj* getBufferCtrlObj();

	// -- Buffer control object
	//SavingCtrlObj* getSavingCtrlObj();

	//-- Synch control object
	void setTrigMode(TrigMode mode);
	void getTrigMode(TrigMode& mode /Out/);

	void setExpTime(double exp_time);
	void getExpTime(double& exp_time /Out/);

	void setLatTime(double lat_time);
	void getLatTime(double& lat_time /Out/);
        
	// not implemented yet
	//void getExposureTimeRange(double& min_expo /Out/, double& max_expo /Out/) const;
	//void getLatTimeRange(double& min_lat /Out/, double& max_lat /Out/) const;

	void setNbFrames(int nb_frames);
	void getNbFrames(int& nb_frames /Out/);

	bool isAcqRunning() const;

	///////////////////////////
	// -- xspress3 specific functions
	///////////////////////////

	void setupClocks(int clk_src, int flags, int tp_type=0);
	void setRunMode(bool playback=false, bool scope=false, bool scalers=true, bool hist=true);
	void getRunMode(bool& playback /Out/, bool& scope /Out/, bool& scalers /Out/, bool& hist /Out/);
	void getNbScalers(int& nscalers /Out/);
	void getRevision(int& revision /Out/);
	void initBrams(int chan);
	void setWindow(int chan, int win, int low, int high);
	void getWindow(int chan, int win, unsigned int& low /Out/, unsigned int& high /Out/);
	void setScaling(int chan, double scaling);
	void setGoodThreshold(int chan, int good_thres);
	void getGoodThreshold(int chan, unsigned int& good_thres /Out/);
	void saveSettings();
	void restoreSettings(bool force_mismatch = false);
	void setDeadtimeCalculationEnergy(double energy);
	void getDeadtimeCalculationEnergy(double &energy /Out/);
	void setDeadtimeCorrectionParameters(int chan, double processDeadTimeAllEventGradient,
			double processDeadTimeAllEventOffset, double processDeadTimeInWindowOffset, double processDeadTimeInWindowGradient,
			bool useGoodEvent=false, bool omitChannel=false);
	void getDeadtimeCorrectionParameters(int chan, double &processDeadTimeAllEventGradient /Out/,
			double &processDeadTimeAllEventOffset /Out/, double &processDeadTimeInWindowOffset /Out/,
			double &processDeadTimeInWindowGradient /Out/, bool &useGoodEvent /Out/, bool &omitChannel /Out/);
	void setFanSetpoint(int deg_c);
	void getFanTemperatures(Data& temp /Out/);
	void setFanController(int p_term, int i_term);
	void arm();
	void pause();
	void restart();
	void start();
	void stop();
	void clear();
	void setCard(int card);
	void getCard(int& card /Out/);
	void getNumChan(int& num_chan /Out/);
	void getNumCards(int& num_cards /Out/);
	void getChansPerCard(int& chans_per_card /Out/);
	void getBinsPerMca(int& bins_per_mca /Out/);
	void getMaxNumChan(int &max_chan /Out/);
	void initRoi(int chan);
	void setRoi(int chan, Xsp3Roi& roi, int& nbins /Out/);
	void getUseDtc(bool &flag /Out/);
	void setUseDtc(bool flag);
	void readScalers(Data& temp /Out/, int frame_nb, int channel);
	void readHistogram(Data& temp /Out/, int frame_nb, int channel);
	void setAdcTempLimit(int temp);
	void setPlayback(bool enable);
	void loadPlayback(std::string filename, int src0, int src1, int streams=0, int digital=0);
	void startScope();
	void setTiming(int time_src, int fixed_time, int alt_ttl_mode, int debounce, bool loop_io=false,
			bool f0_invert=false, bool veto_invert=false);
	void formatRun(int chan=-1, int nbits_eng=12, int aux1_mode=0, int adc_bits=0, int min_samples=0, int aux2_mode=0, bool pileup_reject=false);
	void getDataSource(int chan, int& data_src /Out/);
	void setDataSource(int chan, int data_src=Normal);
  };
};

