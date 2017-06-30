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

#include "Xspress3Camera.h"
#include "lima/Exceptions.h"
#include "lima/Debug.h"
#include "Xspress3Interface.h"

using namespace lima;
using namespace lima::Xspress3;
using namespace std;

//---------------------------
//- utility thread
//---------------------------
class Camera::AcqThread: public Thread {
DEB_CLASS_NAMESPC(DebModCamera, "Camera", "AcqThread");
public:
    AcqThread(Camera &aCam);
    virtual ~AcqThread();

protected:
    virtual void threadFunction();

private:
    Camera& m_cam;
};

class Camera::ReadThread: public Thread {
DEB_CLASS_NAMESPC(DebModCamera, "Camera", "ReadThread");
public:
    ReadThread(Camera &aCam);
    virtual ~ReadThread();

protected:
    virtual void threadFunction();

private:
    Camera& m_cam;
};

//---------------------------
// @brief  Ctor
//---------------------------

Camera::Camera(int nbCards, int maxFrames, string baseIPaddress, int basePort, string baseMACaddress, int nbChans,
        bool createScopeModule, string scopeModuleName, int debug, int cardIndex, bool noUDP, string directoryName) : m_nb_cards(nbCards), m_max_frames(maxFrames),
        m_baseIPaddress(baseIPaddress), m_basePort(basePort), m_baseMACaddress(baseMACaddress), m_nb_chans(nbChans),
        m_create_module(createScopeModule), m_modname(scopeModuleName), m_card_index(cardIndex), m_debug(debug), m_npixels(4096), m_nscalers(XSP3_SW_NUM_SCALERS),
        m_no_udp(noUDP), m_config_directory_name(directoryName), m_trigger_mode(IntTrig), m_image_type(Bpp32), m_nb_frames(1), m_acq_frame_nb(-1),
        m_bufferCtrlObj() {

    DEB_CONSTRUCTOR();
    m_card = -1;
    m_use_dtc = false;
    m_acq_thread = new AcqThread(*this);
    m_acq_thread->start();
    m_read_thread = new ReadThread(*this);
    m_read_thread->start();
    m_clear_flag = true;
    m_exp_time = 0.0;
    init();
}

Camera::~Camera() {
    DEB_DESTRUCTOR();
    delete m_acq_thread;
    delete m_read_thread;
    if (xsp3_close(m_handle) < 0){
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

void Camera::init() {
    DEB_MEMBER_FUNCT();
    if (m_no_udp) {
        m_baseMACaddress = "00:00:00:00:00:00";
    }
    DEB_TRACE() << "Connecting to the Xspress3...";
    if ((m_handle = xsp3_config(m_nb_cards, m_max_frames, (char*)m_baseIPaddress.c_str(), m_basePort, (char*)m_baseMACaddress.c_str(), m_nb_chans,
            m_create_module, (char*)m_modname.c_str(), m_debug, m_card_index)) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    DEB_TRACE() << "Initialise the ROI's";
    initRoi(-1);

    DEB_TRACE() << "Set up clock register to use ADC clock...";

    int gen = xsp3_get_generation(m_handle, m_card);
    for (int i=0; i<m_nb_cards; i++) {
        setCard(i);
        setupClocks(gen == 2 ? Camera::Mini : Camera::XtalClk, Camera::Master | Camera::NoDither, 0);
    }
    setCard(-1);
    if (m_config_directory_name != "") {
        restoreSettings();
    }

    DEB_TRACE() <<  "Set up default run flags...";
    setRunMode();
    m_status = Idle;
    DEB_TRACE() << "Use dtc " << m_use_dtc;
    DEB_TRACE() << "Initialisation complete";
}

void Camera::reset() {
    DEB_MEMBER_FUNCT();
    init();
}

void Camera::prepareAcq() {
    DEB_MEMBER_FUNCT();
    if (m_clear_flag) {
        DEB_TRACE() << "Clear memory " << DEB_VAR2(m_nb_chans, m_nb_frames);
        if (xsp3_histogram_clear(m_handle, 0, m_nb_chans, 0, m_nb_frames) < 0) {
            THROW_HW_ERROR(Error) << xsp3_get_error_message();
        }
    }
}

void Camera::startAcq() {
    DEB_MEMBER_FUNCT();
    m_acq_frame_nb = 0; // Number of frames of data acquired;
    m_read_frame_nb = 0; // Number of frames read into Lima buffers
    StdBufferCbMgr& buffer_mgr = m_bufferCtrlObj.getBuffer();
    buffer_mgr.setStartTimestamp(Timestamp::now());
    if (xsp3_histogram_start(m_handle, m_card) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    AutoMutex aLock(m_cond.mutex());
    m_wait_flag = false;
    m_quit = false;
    m_abort = false;
    m_cond.broadcast();
    // Wait that Acq thread start if it's an external trigger
    //  while (m_trigger_mode == ExtGate && !m_thread_running)
    //  m_cond.wait();
}

void Camera::stopAcq() {
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cond.mutex());
    m_wait_flag = true;
    m_abort = true;
}

void Camera::getStatus(Status& status) {
    DEB_MEMBER_FUNCT();
    AutoMutex lock(m_cond.mutex());
    status = m_status;
}

int Camera::getNbHwAcquiredFrames() {
    DEB_MEMBER_FUNCT();
    return m_acq_frame_nb;
}

void Camera::AcqThread::threadFunction() {
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cam.m_cond.mutex());

    while (!m_cam.m_quit) {
        while (m_cam.m_wait_flag && !m_cam.m_quit) {
            m_cam.m_thread_running = false;
            m_cam.m_cond.wait();
        }
        DEB_TRACE()  << "Acq thread Running";
        m_cam.m_status = Running;
        m_cam.m_thread_running = true;
        if (m_cam.m_quit) {
            DEB_TRACE()  << "acq thread quit called";
            m_cam.m_status = Idle;
            m_cam.m_thread_running = false;
            return;
        }
        aLock.unlock();

        bool continueFlag = true;
        while (continueFlag && (!m_cam.m_nb_frames || m_cam.m_acq_frame_nb < m_cam.m_nb_frames)) {
            DEB_TRACE() << DEB_VAR1(m_cam.m_trigger_mode);
            if (m_cam.m_trigger_mode == IntTrig) {
          
                struct timespec delay, remain;
                delay.tv_sec = (int)floor(m_cam.m_exp_time);
                delay.tv_nsec = (int)(1E9*(m_cam.m_exp_time-floor(m_cam.m_exp_time)));
                DEB_TRACE() << "acq thread will sleep for " << m_cam.m_exp_time << " second";
                while (nanosleep(&delay, &remain) == -1 && errno == EINTR) {
                    // stop called ?
                    //AutoMutex aLock(m_cam.m_cond.mutex());
                    //continueFlag = !m_cam.m_wait_flag;
                  //if (m_cam.m_wait_flag) {
                  //    DEB_TRACE() << "acq thread histogram stopped  by user";
                  //    m_cam.stop();
                  //break;
                  //}
                    delay = remain;
                }
                if (m_cam.m_abort) {
                     DEB_TRACE() << "acq thread histogram stopped  by user";
                     m_cam.stop();
                     break;
                } else if (m_cam.m_acq_frame_nb < m_cam.m_nb_frames-1) {
                    m_cam.pause();
                    m_cam.restart();
                    DEB_TRACE() << "acq thread histogram paused and restarted " << m_cam.m_wait_flag;
                } else {
                    DEB_TRACE() << "acq thread histogram stop";
                    m_cam.stop();
                }
                aLock.lock();
                ++m_cam.m_acq_frame_nb;
              
                delay.tv_sec = 0;
                delay.tv_nsec = (int)(1E9*0.5);
                int completed_frames;
                do {
                    m_cam.checkProgress(completed_frames);
                    DEB_TRACE() << DEB_VAR2(completed_frames, m_cam.m_acq_frame_nb);
                        nanosleep(&delay, &remain);
                } while (completed_frames < m_cam.m_acq_frame_nb);
                m_cam.m_read_wait_flag = false;
                DEB_TRACE() << "acq thread signal read thread: " << m_cam.m_acq_frame_nb << " frames collected";
                m_cam.m_cond.broadcast();
                aLock.unlock();

            } else {
                struct timespec delay, remain;
                delay.tv_sec = 0;
                delay.tv_nsec = (int)(1E9*0.5);
                std::cout << "Started checking in while loop" << std::endl;

                int completed_frames;
                do {
                     m_cam.checkProgress(completed_frames);
                     std::cout << completed_frames << " " << m_cam.m_acq_frame_nb << std::endl;
                     std::cout << "acq thread will sleep for " << delay.tv_nsec << " nanosecond" << std::endl;
                     nanosleep(&delay, &remain);
                     if (m_cam.m_abort) {
                       DEB_TRACE() << "acq thread histogram stopped  by user";
                       m_cam.stop();
                       break;
                     }
                } while (completed_frames <= m_cam.m_acq_frame_nb);
                aLock.lock();
                m_cam.m_acq_frame_nb = (completed_frames > m_cam.m_nb_frames) ? m_cam.m_nb_frames : completed_frames;
                m_cam.m_read_wait_flag = false;
                DEB_TRACE() << "acq thread signal read thread: " << m_cam.m_acq_frame_nb << " frames collected";
                m_cam.m_cond.broadcast();
                aLock.unlock();

            }

            if (m_cam.m_abort) 
                break;


        }

        //      if (!m_cam.m_wait_flag) { // user requested stop
        if (!m_cam.m_abort) { // user requested stop
            // wait for read thread to finish here
            DEB_TRACE() << "acq thread Wait for read thead to finish";
            aLock.lock();
            m_cam.m_cond.wait();
        }

        m_cam.m_status = Idle;
        m_cam.m_thread_running = false;
        m_cam.m_wait_flag = true;
        DEB_TRACE() << "acq thread finished acquire " << m_cam.m_acq_frame_nb << " frames, required " << m_cam.m_nb_frames << " frames";
    }
}

Camera::AcqThread::AcqThread(Camera& cam) : m_cam(cam) {
    AutoMutex aLock(m_cam.m_cond.mutex());
    m_cam.m_wait_flag = true;
    m_cam.m_quit = false;
    aLock.unlock();
    pthread_attr_setscope(&m_thread_attr, PTHREAD_SCOPE_PROCESS);
}

Camera::AcqThread::~AcqThread() {
    AutoMutex aLock(m_cam.m_cond.mutex());
    m_cam.m_quit = true;
    m_cam.m_cond.broadcast();
    aLock.unlock();
}

void Camera::ReadThread::threadFunction() {
    DEB_MEMBER_FUNCT();
    AutoMutex aLock(m_cam.m_cond.mutex());
    StdBufferCbMgr& buffer_mgr = m_cam.m_bufferCtrlObj.getBuffer();

    while (!m_cam.m_quit) {
        while (m_cam.m_read_wait_flag && !m_cam.m_quit) {
          DEB_TRACE() << "Read thread waiting";
            m_cam.m_cond.wait();
        }
        DEB_TRACE() << "Read Thread Running";
        if (m_cam.m_quit)
            return;

        if (m_cam.m_abort) {
            DEB_TRACE() << "User Aborted Acq, Read thread waiting";
            m_cam.m_read_wait_flag = true;
            continue;
        }

        aLock.unlock();

        bool continueFlag = true;
        while (continueFlag && (!m_cam.m_nb_frames || m_cam.m_read_frame_nb < m_cam.m_acq_frame_nb)) {
            void* bptr = buffer_mgr.getFrameBufferPtr(m_cam.m_read_frame_nb);
            DEB_TRACE() << "buffer pointer " << bptr;
            DEB_TRACE() << "read histogram & scaler data frame number " << m_cam.m_read_frame_nb;
            m_cam.readFrame(bptr, m_cam.m_read_frame_nb);
            HwFrameInfoType frame_info;
            frame_info.acq_frame_nb = m_cam.m_read_frame_nb;
            continueFlag = buffer_mgr.newFrameReady(frame_info);
            DEB_TRACE() << "readThread::threadFunction() newframe ready ";
            ++m_cam.m_read_frame_nb;
        }
        aLock.lock();
        // if all frames read wakeup the acq thread
        if (m_cam.m_nb_frames == m_cam.m_read_frame_nb) {
            DEB_TRACE() << "broadcast to wake acq thread";
            m_cam.m_cond.broadcast();
            aLock.unlock();
        }
        // This thread will now wait for next frame or acq
        m_cam.m_read_wait_flag = true;
    }
}

Camera::ReadThread::ReadThread(Camera& cam) : m_cam(cam) {
    AutoMutex aLock(m_cam.m_cond.mutex());
    m_cam.m_read_wait_flag = true;
    m_cam.m_quit = false;
    aLock.unlock();
    pthread_attr_setscope(&m_thread_attr, PTHREAD_SCOPE_PROCESS);
}

Camera::ReadThread::~ReadThread() {
    AutoMutex aLock(m_cam.m_cond.mutex());
    m_cam.m_quit = true;
    m_cam.m_cond.broadcast();
    aLock.unlock();
}

void Camera::getImageType(ImageType& type) {
    DEB_MEMBER_FUNCT();
    type = m_image_type;
}

void Camera::setImageType(ImageType type) {
    DEB_MEMBER_FUNCT();
    m_image_type = type;
}

void Camera::getDetectorType(std::string& type) {
    DEB_MEMBER_FUNCT();
    type = "xspress3";
}

void Camera::getDetectorModel(std::string& model) {
    DEB_MEMBER_FUNCT();
    stringstream ss;
    int revision = xsp3_get_revision(m_handle);
    int major = (revision >> 12) & 0xfff;
    int minor = revision & 0xfff;
    ss << "Revision-" << major << "." << minor;
    model = ss.str();
}

void Camera::getDetectorImageSize(Size& size) {
    DEB_MEMBER_FUNCT();
    size = Size(m_npixels + m_nscalers, m_nb_chans);
}

void Camera::getPixelSize(double& sizex, double& sizey) {
    DEB_MEMBER_FUNCT();
    sizex = xPixelSize;
    sizey = yPixelSize;
}

HwBufferCtrlObj* Camera::getBufferCtrlObj() {
    return &m_bufferCtrlObj;
}

void Camera::setTimingMode() {
    DEB_MEMBER_FUNCT();
    
    if (!m_exp_time) return;
    if (!m_nb_frames) return;

    DEB_TRACE() << "Camera::setTimingMode() " << DEB_VAR1(m_trigger_mode) << "exp " << DEB_VAR1(m_exp_time) << "nbf " << m_nb_frames;

    int alt_ttl_mode = 0;
    int debounce = 80;

    if (m_trigger_mode == IntTrig) {
        // Src 1 = Internal
        // setTiming(int time_src, int first_frame, int alt_ttl_mode, int debounce, bool loop_io, bool f0_invert, bool veto_invert);
        setCard(0);
        setTiming(1, 0, alt_ttl_mode, debounce, false, false, false);
        for (int i=1;i<m_nb_cards; i++) {
            setCard(i);
            setTiming(4, 0, alt_ttl_mode, 0, false, false, false);
        }
        setCard(-1);

        // setItfgTiming(int nframes, int triggerMode, int gapMode);
        // triggerMode 0 = Burst, gapMode 3 = 1us
        setItfgTiming(m_nb_frames, 0, 3);

    } else if (m_trigger_mode == ExtGate) {
        // Src 4 = Ext
        setTiming(4, 0, alt_ttl_mode, debounce, false, false, false);
    }
}

void Camera::setTrigMode(TrigMode mode) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setTrigMode() " << DEB_VAR1(mode);
    DEB_PARAM() << DEB_VAR1(mode);
    switch (mode) {
    case IntTrig:
    case IntTrigMult:
    case ExtGate:
    case ExtTrigMult:
    case ExtTrigSingle:
        m_trigger_mode = mode;
        break;
    case ExtStartStop:
    case ExtTrigReadout:
    default:
        THROW_HW_ERROR(Error) << "Cannot change the Trigger Mode of the camera, this mode is not managed !";
        break;
    }

    setTimingMode();
}

void Camera::getTrigMode(TrigMode& mode) {
    DEB_MEMBER_FUNCT();
    mode = m_trigger_mode;
    DEB_RETURN() << DEB_VAR1(mode);
}

void Camera::getExpTime(double& exp_time) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::getExpTime() ";
    //  AutoMutex aLock(m_cond.mutex());
    exp_time = m_exp_time;
    DEB_RETURN() << DEB_VAR1(exp_time);
}

void Camera::setExpTime(double exp_time) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setExpTime() " << DEB_VAR1(exp_time);

    m_exp_time = exp_time;
    setTimingMode();
}

void Camera::setLatTime(double lat_time) {
    DEB_MEMBER_FUNCT();
    DEB_PARAM() << DEB_VAR1(lat_time);

    if (lat_time != 0.)
        THROW_HW_ERROR(Error) << "Latency not managed";
}

void Camera::getLatTime(double& lat_time) {
    DEB_MEMBER_FUNCT();
    lat_time = 0;
}

void Camera::getExposureTimeRange(double& min_expo, double& max_expo) const {
    DEB_MEMBER_FUNCT();
    min_expo = 0.;
    max_expo = (double)UINT_MAX * 20e-9; //32bits x 20 ns
    DEB_RETURN() << DEB_VAR2(min_expo, max_expo);
}

void Camera::getLatTimeRange(double& min_lat, double& max_lat) const {
    DEB_MEMBER_FUNCT();
    // --- no info on min latency
    min_lat = 0.;
    // --- do not know how to get the max_lat, fix it as the max exposure time
    max_lat = (double) UINT_MAX * 20e-9;
    DEB_RETURN() << DEB_VAR2(min_lat, max_lat);
}

void Camera::setNbFrames(int nb_frames) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setNbFrames() " << DEB_VAR1(nb_frames);
    if (m_nb_frames < 0) {
        THROW_HW_ERROR(Error) << "Number of frames to acquire has not been set";
    }
    m_nb_frames = nb_frames;
    setTimingMode();
}

void Camera::getNbFrames(int& nb_frames) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::getNbFrames() ";
    DEB_RETURN() << DEB_VAR1(m_nb_frames);
    nb_frames = m_nb_frames;
}

bool Camera::isAcqRunning() const {
    AutoMutex aLock(m_cond.mutex());
    return m_thread_running;
}

/////////////////////////////////
// xspress3 specific stuff now //
/////////////////////////////////

/**
 * Set up ADC data processing clocks source.
 *
 * @param[in] clk_src clock source {@see #ClockSrc}
 * @param[in] flags setup flags {@see #ClockFlags}
 * @param[in] tp_type test pattern type in the IO spartan FPGAs
 */
void Camera::setupClocks(ClockSrc clk_src, ClockFlags flags, int tp_type)
{
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setupClocks() " << DEB_VAR3(clk_src,flags,tp_type);
    if (xsp3_clocks_setup(m_handle, m_card, clk_src, flags, tp_type) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the run mode flags
 *
 * @param[in] scalers
 * @param[in] hist
 * @param[in] playback
 * @param[in] scope
 */
void Camera::setRunMode(bool playback, bool scope, bool scalers, bool hist) {
    DEB_MEMBER_FUNCT();
    int flags = 0;
    if (playback)
        flags |= XSP3_RUN_FLAGS_PLAYBACK;
    if (scope)
        flags |= XSP3_RUN_FLAGS_SCOPE;
    if (scalers)
        flags |= XSP3_RUN_FLAGS_SCALERS;
    if (hist)
        flags |= XSP3_RUN_FLAGS_HIST;

    DEB_TRACE() << "Camera::setRunMode() " << DEB_VAR4(playback,scope,scalers,hist);
    if (xsp3_set_run_flags(m_handle, flags) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Get the run mode flags
 *
 * @param[out] flags the value of run flags
 */
void Camera::getRunMode(bool& playback, bool& scope, bool& scalers, bool& hist) {
//void Camera::getRunMode(int& flags) {
    DEB_MEMBER_FUNCT();
    int flags;
    if ((flags = xsp3_get_run_flags(m_handle)) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    playback = flags & XSP3_RUN_FLAGS_PLAYBACK;
    scope = flags & XSP3_RUN_FLAGS_SCOPE;
    scalers = flags & XSP3_RUN_FLAGS_SCALERS;
    hist = flags & XSP3_RUN_FLAGS_HIST;
    DEB_TRACE() << "Camera::getRunMode() " << DEB_VAR4(playback,scope,scalers,hist);
}

/**
 * Get the number of scalers configured in the system.
 *
 * @param[out] nscalers the number of scalers in the xspress3 system.
 */
void Camera::getNbScalers(int& nb_scalers) {
    DEB_MEMBER_FUNCT();
    nb_scalers = m_nscalers;
}

/**
 * Get firmware revision.
 *
 * @param[out] revision the firmware revision of the xspress3 system.
 */
void Camera::getRevision(int& revision) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::getRevision() ";
    if ((revision = xsp3_get_revision(m_handle)) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Initialise the contents of the BRAM registers.
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected.
 */
void Camera::initBrams(int chan) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::initBrams() " << DEB_VAR1(chan);
    if (xsp3_bram_init(m_handle, chan, -1, -1.0) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the scaler windows
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected.
 * @param[in] win the window scaler (0 or 1)
 * @param[in] low the low window threshold (0 ... 65535)
 * @param[in] high the high window threshold (0 ... 65535)
 */
void Camera::setWindow(int chan, int win, int low, int high) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setWindow() " << DEB_VAR4(chan,win,low,high);
    if (xsp3_set_window(m_handle, chan, win, low, high) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Get the scaler window settings.
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected.
 * @param[in] win the window (0 or 1)
 * @param[out] low returned low window thresholds
 * @param[out] high returned high window threshold
 */
void Camera::getWindow(int chan, int win, u_int32_t& low, u_int32_t& high) {
    DEB_MEMBER_FUNCT();
    if (xsp3_get_window(m_handle, chan, win, &low, &high) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    DEB_TRACE() << "Camera::getWindow() " << DEB_VAR4(chan,win,low,high);
}

/**
 * Set energy scaling
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected.
 * @param[in] scaling
 */
void Camera::setScaling(int chan, double scaling) {
    DEB_MEMBER_FUNCT();
    if (scaling >= 0.0 && (scaling < 0.5 || scaling > 2.0)) {
        THROW_HW_ERROR(Error) << "# Warning: large magnitude scaling "<< scaling << " on channel " << chan << ", should be 0.5 to 2.0";
    }
    DEB_TRACE() << "Camera::setScaling() " << DEB_VAR2(chan, scaling);
    if (xsp3_bram_init(m_handle, chan, XSP3_REGION_RAM_QUOTIENT, scaling) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the threshold for the good event scaler
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected.
 * @param[in] good_thres the threshold for the good event scaler
 */
void Camera::setGoodThreshold(int chan, int good_thres) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setGoodThreshold() " << DEB_VAR2(chan, good_thres);
    if (xsp3_set_good_thres(m_handle, chan, good_thres) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Get the good event threshold
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 * @param[out] good_thres good event threshold
 */
void Camera::getGoodThreshold(int chan, u_int32_t& good_thres) {
    DEB_MEMBER_FUNCT();
    if (xsp3_get_good_thres(m_handle, chan, &good_thres) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    DEB_TRACE() << "Camera::getGoodThreshold() " << DEB_VAR2(chan, good_thres);
}

/**
 * Save all xspress3 settings into a file.
 *
 * @param[in] dir_name directory to save settings file.
 */
void Camera::saveSettings() {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::saveSettings() " << DEB_VAR1(m_config_directory_name);
    if (xsp3_save_settings(m_handle, (char*) m_config_directory_name.c_str()) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Restore all xspress3 settings from file.
 *
 * @param[in] dir_name directory with saved settings file.
 * @param[in] force_mismatch force restore if major revision of saved file does not match the firmware revision.
 */
void Camera::restoreSettings(bool force_mismatch) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::restoreSettings() " << DEB_VAR2(m_config_directory_name,force_mismatch);
    if (xsp3_restore_settings(m_handle, (char*) m_config_directory_name.c_str(), force_mismatch) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the trigger B ringing removal filter.
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected.
 * @param[in] scale_a Scaling for 1st correction point (-0.5 to +0.5).
 * @param[in] delay_a Delay for 1st correction point (3..34).
 * @param[in] scale_b Scaling for 2nd correction point (-0.5 to +0.5).
 * @param[in] delay_b Delay for 2nd correction point (3..34).
 */
void Camera::setRinging(int chan, double scale_a, int delay_a, double scale_b, int delay_b) {
    DEB_MEMBER_FUNCT();
    if (xsp3_set_trigger_b_ringing(m_handle, chan, scale_a, delay_a, scale_b, delay_b) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the energy for dead time energy correction
 *
 * @param[in] energy the energy in keV
 */
void Camera::setDeadtimeCalculationEnergy(double energy) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setDeadtimeCalculationEnergy() " << DEB_VAR1(energy);

    if (xsp3_setDeadtimeCalculationEnergy(m_handle, energy) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Get the energy for dead time energy correction
 *
 * @param[out] energy the energy in keV
 */
void Camera::getDeadtimeCalculationEnergy(double &energy) {
    DEB_MEMBER_FUNCT();
    if ((energy = xsp3_getDeadtimeCalculationEnergy(m_handle)) < 0.0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the parameters for dead time energy correction
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected
 * @param[in] processDeadTimeAllEventGradient
 * @param[in] processDeadTimeAllEventOffset
 * @param[in] processDeadTimeInWindowOffset
 * @param[in] processDeadTimeInWindowGradient
 * @param[in] useGoodEvent
 * @param[in] omitChannel
 */
void Camera::setDeadtimeCorrectionParameters(int chan, double processDeadTimeAllEventGradient,
        double processDeadTimeAllEventOffset, double processDeadTimeInWindowOffset, double processDeadTimeInWindowGradient,
        bool useGoodEvent, bool omitChannel) {
    DEB_MEMBER_FUNCT();
    int flags = 0;
    if (omitChannel)
        flags |= XSP3_DTC_OMIT_CHANNEL;
    if (useGoodEvent)
        flags |= XSP3_DTC_USE_GOOD_EVENT;
    DEB_TRACE() << "Camera::setDeadtimeCorrectionParameters() " << DEB_VAR7(chan,processDeadTimeAllEventGradient,
            processDeadTimeAllEventOffset,processDeadTimeInWindowOffset,processDeadTimeInWindowGradient,useGoodEvent,omitChannel);
    if (xsp3_setDeadtimeCorrectionParameters(m_handle, chan, flags, processDeadTimeAllEventGradient,
        processDeadTimeAllEventOffset, processDeadTimeInWindowOffset, processDeadTimeInWindowGradient) < 0){
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the parameters for dead time energy correction
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected
 * @param[out] processDeadTimeAllEventGradient
 * @param[out] processDeadTimeAllEventOffset
 * @param[out] processDeadTimeInWindowOffset
 * @param[out] processDeadTimeInWindowGradient
 * @param[out] useGoodEvent
 * @param[out] omitChannel
 */
void Camera::getDeadtimeCorrectionParameters(int chan, double &processDeadTimeAllEventGradient,
        double &processDeadTimeAllEventOffset, double &processDeadTimeInWindowOffset, double &processDeadTimeInWindowGradient,
        bool &useGoodEvent, bool &omitChannel) {
    DEB_MEMBER_FUNCT();
    int flags = 0;
    if (xsp3_getDeadtimeCorrectionParameters(m_handle, chan, &flags, &processDeadTimeAllEventGradient, &processDeadTimeAllEventOffset, &processDeadTimeInWindowOffset, &processDeadTimeInWindowGradient) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    omitChannel = flags & XSP3_DTC_OMIT_CHANNEL;
    useGoodEvent = flags & XSP3_DTC_USE_GOOD_EVENT;
}

/**
 * Set the set-point for the temperature controller
 *
 * @param[in] deg_c the target temperature (Deg C)
 */
void Camera::setFanSetpoint(int deg_c) {
    DEB_MEMBER_FUNCT();
    u_int32_t reg= deg_c*2;
    if (xsp3_write_fan_cont(m_handle, m_card, XSP3_FAN_OFFSET_SET_POINT, 1, &reg) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Read the current temperature from the controller
 *
 * @param[out] temp an array of 5 temperature values (4 sensors + maximum value) per card in the system.
 */
void Camera::getFanTemperatures(Data& sensorData) {
    DEB_MEMBER_FUNCT();
    int n = (m_card == -1) ? m_nb_cards : m_card;
    u_int32_t reg[n*5];
    for (int card = 0; card < n; card++) {
        if (xsp3_read_fan_cont(m_handle, card, XSP3_FAN_OFFSET_TEMP, 5, &reg[card*5]) < 0) {
            THROW_HW_ERROR(Error) << xsp3_get_error_message();
        }
    }
    sensorData.type = Data::DOUBLE;
    sensorData.frameNumber = 0;
    sensorData.dimensions.push_back(5);
    sensorData.dimensions.push_back(n);
    Buffer *buff = new Buffer();
    double *sdata = new double[n*5];
    for (int i = 0; i < n * 5; i++) {
        sdata[i] = 0.5 * reg[i];
    }
    buff->data = sdata;
    sensorData.setBuffer(buff);
    buff->unref();
    DEB_TRACE() << "returning Data.size() " << sensorData.size();
}

/**
 * Set the coefficients for the temperature controller0.5 * reg[i]
 *
 * @param[in] p_term the P term
 * @param[in] i_term the I term
 */
void Camera::setFanController(int p_term, int i_term) {
    DEB_MEMBER_FUNCT();
    u_int32_t reg[2];
    reg[0] = p_term;
    reg[1] = i_term;
    if (xsp3_write_fan_cont(m_handle, m_card, XSP3_FAN_OFFSET_P_CONST, 2, reg)< 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Start histogramming after resetting the UDP port and frame numbers. The UDP port reset is only required when it is
 * not in farm mode as the scope mode also uses the UDP connection on a different port.
 * In software timing mode, counting is left disabled, start with continueAcq()
 */
void Camera::arm() {
    DEB_MEMBER_FUNCT();
    if (xsp3_histogram_arm(m_handle, m_card) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Pause histograming software mode, can continue into next frame
 */
void Camera::pause() {
    DEB_MEMBER_FUNCT();
    if (xsp3_histogram_pause(m_handle, m_card) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    m_status = Paused;
}

/**
 * Continue counting from armed or paused when time framing is in software controlled (FIXED) mode
 */
void Camera::restart() {
    DEB_MEMBER_FUNCT();
    if (xsp3_histogram_continue(m_handle, m_card) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

void Camera::start() {
    DEB_MEMBER_FUNCT();
    if (xsp3_histogram_start(m_handle, m_card) < 0){
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

void Camera::stop() {
    DEB_MEMBER_FUNCT();
    if (xsp3_histogram_stop(m_handle, m_card) < 0){
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    int idleCount = 0;
    while (idleCount < 2) {
        struct timespec delay, remain;
        delay.tv_sec = 0;
        delay.tv_nsec = (int)(1E9*(0.01)); // sleep for 10msec
        nanosleep(&delay, &remain);
        if (xsp3_histogram_is_any_busy(m_handle) == 0) {
            ++idleCount;
        }
    }
}

/**
 * Enable/disable clearing of the histograming memory.
 *
 * @param[in] clear_flag set true is clear, set false does not clear.
 */
void Camera::setClearMode(bool clear_flag) {
    DEB_MEMBER_FUNCT();
    m_clear_flag = clear_flag;
}

/**
 * Check which frames have been acquired
 *
 * @param[out] frameNos the nos of frames of data acquired.
 */
void Camera::checkProgress(int& frameNos) {
    DEB_MEMBER_FUNCT();
    int fn = xsp3_scaler_check_progress(m_handle);
    DEB_TRACE() << DEB_VAR1(fn);
    if (fn < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    frameNos = fn;
}

/**
 * Select the currently active card
 *
 * @param[in] card is the number of the card in the xspress3 system,
 *        0 for a single card system and up to ({@link xsp3_get_num_cards()} - 1) for a multi-card system.
 *        If card is less than 0 then all cards are selected.
 */
void Camera::setCard(int card) {
    DEB_MEMBER_FUNCT();
    m_card = card;
}

/*
 * Get the current selected card.
 *
 * @param[out] card is the number of the selected card in the xspress3 system,
 *        0 for a single card system and up to ({@link xsp3_get_num_cards()} - 1) for a multi-card system.
 *        If card is less than 0 then all cards are selected.
 */
void Camera::getCard(int& card) {
    DEB_MEMBER_FUNCT();
    card = m_card;
}

/**
 * Get the number of channels currently configured in the system.
 * This value is passed into the {@link #xsp3_config() xsp3_config} routine.
 * The correct value is stored in the top level path of the system.
 *
 * @param[out] nb_chans the number of configured channels in the xspress3 system or a negative error code.
 */
void Camera::getNumChan(int& nb_chans) {
    DEB_MEMBER_FUNCT();
    nb_chans = xsp3_get_num_chan(m_handle);
}

/**
 * Get the number of xspress3 cards currently configured in the system.
 * The correct value is stored in the top level path of the system.
 *
 * @param[out] nb_cards the number of cards currently configured in the xspress3 system or a negative error code.
 */
void Camera::getNumCards(int& nb_cards) {
    DEB_MEMBER_FUNCT();
    nb_cards = xsp3_get_num_cards(m_handle);
}

/**
 * Get the number of channels available per xspress3 card.
 * The correct value is stored in the top level path of the system.
 *
 * @param[out] chans_per_card the number of channels available per xspress3 card or a negative error code.
 */
void Camera::getChansPerCard(int& chans_per_card) {
    DEB_MEMBER_FUNCT();
    chans_per_card = xsp3_get_chans_per_card(m_handle);
}

/**
 * Get the number of bins per MCA configured in the xspress3 system.
 * The correct value is stored in the top level path of the system.
 *
 * @param[out] bins_per_mca the number of bins per MCA for the current configuration of the xspress3 system or a negative error code.
 */
void Camera::getBinsPerMca(int& bins_per_mca) {
    DEB_MEMBER_FUNCT();
    bins_per_mca = xsp3_get_bins_per_mca(m_handle);
}

/**
 * Get the maxmimum number of channels currently available in system.
 * The correct value is stored in the top level path of the system.
 *
 * @param[out] max_chan the maximum number of channels for the current configuration of the xspress3 system or a negative error code.
 */
void Camera::getMaxNumChan(int &max_chan) {
    DEB_MEMBER_FUNCT();
    max_chan = xsp3_get_max_num_chan(m_handle);
}

/**
 * Initialise the regions of interest. Removes any existing regions of interest.
 *
 * @param[in] chan is the number of the channel in the xspress3 system,
 *             if chan is less than 0 then all channels are selected
 */
void Camera::initRoi(int chan) {
    DEB_MEMBER_FUNCT();
    if (xsp3_init_roi(m_handle, chan) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Sets the regions of interest.
 *
 * @param[in] chan is the number of the channel in the xspress3 system, 0 to ({@link xsp3_get_num_chan()} - 1)
 *             if chan is less than 0 then all channels are selected.
 * @param[in] num_roi the number of regions of interest.
 * @param[in] roi a pointer to an array of {@link #XSP3Roi XSP3Roi} structures describing the details the region.
 * @param[out] total number of bins used or a negative error code.
 */
void Camera::setRoi(int chan, Xsp3Roi& roi, int& nbins) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::setRoi - " << DEB_VAR2(chan,roi);
    int num_roi = roi.getNumRoi();
    XSP3Roi rois[num_roi];
    for (int i=0; i<num_roi; i++) {
        rois[i].lhs = roi.getLhs(i);
        rois[i].rhs = roi.getRhs(i);
        rois[i].out_bins = roi.getBins(i);
    }
    if ((nbins = xsp3_set_roi(m_handle, chan, num_roi, rois)) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Use dead time correction when reading scalers
 *
 * @param flag enable or disable dead time correction
 */
void Camera::getUseDtc(bool &flag) {
    flag = m_use_dtc;
}

void Camera::setUseDtc(bool flag) {
    m_use_dtc = flag;
}

/**
 * Read a frame of xspress3 data comprising histogram & scaler data (used by read thread only).
 * The scaler data is added to the end of each channel of histogram data
 * @verbatim
 * The scaler data block will comprise of the following values
 * scaler 0 - Time
 * scaler 1 - ResetTicks
 * scaler 2 - ResetCount
 * scaler 3 - AllEvent
 * scaler 4 - AllGood
 * scaler 5 - InWIn 0
 * scaler 6 - In Win 1
 * scaler 7 - PileUp
 * scaler 8 - TotalTicks
 * @endverbatim
 *
 * @param bptr a pointer to the buffer for the returned data
 * @param frame_nb is the time frame
 */
void Camera::readFrame(void *fptr, int frame_nb) {
    DEB_MEMBER_FUNCT();
    u_int32_t scalerData[m_nscalers*m_nb_chans];
    u_int32_t* bptr = (u_int32_t*)fptr;

    DEB_TRACE() << "Camera::readFrame() scalers " << DEB_VAR2(frame_nb, m_nb_chans);
    if (xsp3_scaler_read(m_handle, scalerData, 0, 0, frame_nb, m_nscalers, m_nb_chans, 1) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    for (int chan=0; chan<m_nb_chans; chan++) {
        DEB_TRACE() << "Camera::readFrame() histogram " << DEB_VAR3(frame_nb, m_npixels, chan);
        if (xsp3_histogram_read3d(m_handle, (u_int32_t*) bptr, 0, chan, frame_nb, m_npixels, 1, 1) < 0) {
            THROW_HW_ERROR(Error) << xsp3_get_error_message();
        }
        bptr += m_npixels;
        for (int i=0; i<m_nscalers; i++) {
            *bptr++ = scalerData[chan*m_nscalers+i];
        }
    }
}

/**
 * Read a frame of scaler data.
 * @verbatim
 * The data block will comprise of the following scaler values
 * scaler 0 - Time
 * scaler 1 - ResetTicks
 * scaler 2 - ResetCount
 * scaler 3 - AllEvent
 * scaler 4 - AllGood
 * scaler 5 - InWindow 0
 * scaler 6 - InWindow 1
 * scaler 7 - PileUp
 * scalar 8 - Total Ticks
 * scalar 9 - Deadtime %
 * scalar 10 - Deadtime correction factor
 * @endverbatim
 *
 * @param scalerData a data buffer to receive scaler data
 * @param frame_nb the time frame to read
 * @param channel the scaler channel to read
 */
void Camera::readScalers(Data& scalerData, int frame_nb, int channel) {
    DEB_MEMBER_FUNCT();
    HwFrameInfo frame_info;
    if (frame_nb >= m_acq_frame_nb) {
        THROW_HW_ERROR(Error) << "Frame not available yet";
    } else {
        StdBufferCbMgr& buffer_mgr = m_bufferCtrlObj.getBuffer();
        buffer_mgr.getFrameInfo(frame_nb,frame_info);
        scalerData.dimensions.push_back(m_nscalers+2);
        scalerData.dimensions.push_back(1);
        scalerData.frameNumber = frame_nb;

        Buffer *fbuf = new Buffer();
        u_int32_t *fptr = (u_int32_t*)frame_info.frame_ptr;
        fptr += channel * (m_npixels + m_nscalers) + m_npixels;
        DEB_TRACE() << DEB_VAR1(m_use_dtc);

        scalerData.type = Data::DOUBLE;
        double *buff = new double[m_nscalers+2];
        double *bptr = buff;

        if (m_use_dtc) {
            double dtcFactor;
            correctScalerData(buff, fptr, channel, dtcFactor);
        } else {
            for (int i = 0; i < m_nscalers; i++) {
                *bptr++ = (double)*fptr++;
            }
        }

        Xspress3_TriggerB trig_b;
        xsp3_get_trigger_b(m_handle, channel, &trig_b);
        double evtwidth = (double)trig_b.event_time;
        double resets = buff[1];
        double allevt = buff[3];
        double ctime = buff[0];

        DEB_TRACE() << DEB_VAR4(evtwidth, resets, allevt, ctime);

        *bptr++ = 100.0*(allevt*(evtwidth+1) + resets)/ctime;
        *bptr++ = ctime/(ctime - (allevt*(evtwidth+1) + resets));

        fbuf->data = buff;
        scalerData.setBuffer(fbuf);
        fbuf->unref();
    }
}

void Camera::correctScalerData(double* buff, u_int32_t* fptr, int channel, double& dtcFactor) {
    DEB_MEMBER_FUNCT();
    double *dptr = buff;
    //  double dtcFactor;
    double dtcAllEvent;
    int flags = 0;

    if (xsp3_calculateDeadtimeCorrectionFactors(m_handle, fptr, &dtcFactor, &dtcAllEvent, 1, channel, 1) < 0) {
//  if (xsp3_calculateDeadtimeCorrectionFactors(m_handle, fptr, &dtcFactor, &dtcAllEvent, 1, 1) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    DEB_TRACE() << "Calculated dead time correction factor " << dtcFactor << " dtc allevent " << dtcAllEvent;
    xsp3_getDeadtimeCorrectionFlags(m_handle, channel, &flags);
    for (int k = 0; k < m_nscalers; k++) {
        if (k == XSP3_SCALER_INWINDOW0 || k == XSP3_SCALER_INWINDOW1) {
            *dptr++ = (double) *fptr++ * dtcFactor;
        } else if (k == XSP3_SCALER_ALLEVENT && !(flags & XSP3_DTC_USE_GOOD_EVENT)) {
            *dptr++ = dtcAllEvent;
            fptr++;
        } else if (k == XSP3_SCALER_ALLGOOD && (flags & XSP3_DTC_USE_GOOD_EVENT)) {
            *dptr++ = dtcAllEvent;
            fptr++;
        } else {
            *dptr++ = (double) *fptr++;
        }
    }
}

/**
 * Read a frame of histogram data for a particular channel.
 *
 * @param histData a data buffer to receive histogram data
 * @param frame_nb the time frame to read
 * @param channel the scaler channel to read
 */
void Camera::readHistogram(Data& histData, int frame_nb, int channel) {
    DEB_MEMBER_FUNCT();
    HwFrameInfo frame_info;
    if (frame_nb >= m_acq_frame_nb) {
        THROW_HW_ERROR(Error) << "Frame not available yet";
    } else {
        StdBufferCbMgr& buffer_mgr = m_bufferCtrlObj.getBuffer();
        buffer_mgr.getFrameInfo(frame_nb, frame_info);

        histData.type = Data::UINT32;
        histData.dimensions.push_back(m_npixels);
        histData.dimensions.push_back(1);
        histData.frameNumber = frame_nb;

        Buffer *fbuf = new Buffer();
        u_int32_t *fptr = (u_int32_t*) frame_info.frame_ptr;
        fptr += channel * (m_npixels + m_nscalers);
        u_int32_t *scalerData = (u_int32_t*) frame_info.frame_ptr;
        scalerData += channel * (m_npixels + m_nscalers) + m_npixels;
        if (m_use_dtc) {
            double *buff = new double[m_npixels];
            double *dptr = buff;
            double dtcFactor;
            double dtcAllEvent;
            if (xsp3_calculateDeadtimeCorrectionFactors(m_handle, scalerData, &dtcFactor, &dtcAllEvent, 1, channel, 1) < 0) {
                THROW_HW_ERROR(Error) << xsp3_get_error_message();
            }
            histData.type = Data::DOUBLE;
            for (int i = 0; i < m_npixels; i++) {
                *dptr++ = (double) *fptr++ * dtcFactor;
            }
            fbuf->data = buff;
        } else {
            u_int32_t *buff = new u_int32_t[m_npixels];
            u_int32_t *bptr = buff;
            histData.type = Data::UINT32;
            for (int i = 0; i < m_npixels; i++) {
                *bptr++ = *fptr++;
            }
            fbuf->data = buff;
        }
        histData.setBuffer(fbuf);
        fbuf->unref();
    }
}

/**
 * Read a frame of raw histogram data for a particular channel.
 *
 * @param histData a data buffer to receive histogram data
 * @param frame_nb the time frame to read
 * @param channel the scaler channel to read
 */
void Camera::readRawHistogram(Data& histData, int frame_nb, int channel) {
    DEB_MEMBER_FUNCT();
    HwFrameInfo frame_info;

    histData.type = Data::UINT32;
    histData.dimensions.push_back(m_npixels);
    histData.dimensions.push_back(1);
    histData.frameNumber = frame_nb;

    Buffer *fbuf = new Buffer();

    u_int32_t *buff = new u_int32_t[m_npixels];
    u_int32_t *bptr = buff;

    DEB_TRACE() << "Camera::readRawHistogram " << DEB_VAR3(frame_nb, m_npixels, channel);
    if (xsp3_histogram_read3d(m_handle, (u_int32_t*) bptr, 0, channel, frame_nb,
            m_npixels, 1, 1) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }

    fbuf->data = buff;
    histData.setBuffer(fbuf);
    fbuf->unref();
}

void Camera::setAdcTempLimit(int temp) {
    DEB_MEMBER_FUNCT();
    if (xsp3_i2c_set_adc_temp_limit(m_handle, m_card, temp) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

void Camera::setPlayback(bool enable) {
    DEB_MEMBER_FUNCT();
    int flags;

    if ((flags = xsp3_get_run_flags(m_handle)) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    if (enable) {
        flags |= XSP3_RUN_FLAGS_PLAYBACK;
    } else {
        flags &= ~XSP3_RUN_FLAGS_PLAYBACK;
    }
    if (xsp3_set_run_flags(m_handle, flags) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

void Camera::loadPlayback(string filename, int src0, int src1, int streams, int digital) {
    DEB_MEMBER_FUNCT();
    DEB_TRACE() << "Camera::loadPlayback() " << DEB_VAR5(filename,src0,src1,streams,digital);
    int src[16];
    int str0dig;
    int smooth_join;
    int enb_higher_chan;

    src[0] = src0;
    src[1] = src1;

    if (xsp3_get_generation(m_handle, m_card) == 2) enb_higher_chan = 1;
    if (xsp3_playback_load_x3(m_handle, m_card, (char*)filename.c_str(), src, streams, str0dig, smooth_join, enb_higher_chan) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }

    setRunMode(true, false, true, true);
    setDataSource(0, Camera::PlaybackStream0);
}

void Camera::startScope() {
    DEB_MEMBER_FUNCT();
    bool count_enb = true;
    int pb_num_t = 0;
    int scope_num_t = 0;

    if (xsp3_scope_settings_from_mod(m_handle) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    // if (xsp3_system_start_count_enb(m_handle, m_card, count_enb) < 0) {
    if (xsp3_system_start_count_enb(m_handle, m_card, count_enb, pb_num_t, scope_num_t) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    if (xsp3_scope_wait(m_handle, m_card) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    if (xsp3_read_scope_data(m_handle, m_card) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Setup Time Framing source
 *
 * @param[in] time_src 0 = default software
 *                     1 = Use TTL input 1 as Veto (default is software controlled Veto)
 *                     2 = Use TTL inputs 0 and 1 as Frame Zero and Veto respectively
 *                     3 = Use 4 Pin LVDS input for veto only
 *                     4 = Use 4 Pin LVDS input for Veto and Frame 0
 * @param[in] first_frame Specify first time frame (default 0)
 * @param[in] alt_ttl_mode 0 = default timing mode
 *                         1 = TTL 0..3 output in-window0 for channels 0..3 respectively
                           2 = TTL 0..3 output in-win0(0), in-win0(1), live-level(0) and live-level(1) respectively. Count live when high
                           3 = TTL 0..3 output in-win0(0), in-win0(1), live-toggle(0) and live-toggle(1) respectively. Count live (rising edges)
                           4 = TTL 0..3 output in-win0, all-event, all-good and live-level from channel 0
                           5 = TTL 0..3 output in-win0, all-event, all-good and live-toggle from channel 0
 * @param[in] debounce Set debounce time in 80 MHz cycles to ignore glitches and ringing on Frame 0 and Framing signals
 * @param[in] loop_io Loop TTL In 0..3 to TTL OUt 0..3 for hardware testing
 * @param[in] f0_invert Invert Frame 0 input
 * @param[in] veto_invert Invert Veto input
 */
void Camera::setTiming(int time_src, int first_frame, int alt_ttl_mode, int debounce, bool loop_io, bool f0_invert, bool veto_invert) {
    DEB_MEMBER_FUNCT();
    int t_src=0;
    u_int32_t time_fixed=0;
    u_int32_t time_a=0;
    int debounce_val;
    int alt_ttl;
    DEB_TRACE() << "Camera::setTiming() " << DEB_VAR7(time_src,first_frame,alt_ttl_mode,debounce,loop_io,f0_invert,veto_invert);

    switch (time_src)
    {
    case 0:
        t_src = XSP3_GTIMA_SRC_SOFTWARE;
        break;
    case 1:
        t_src = XSP3_GTIMA_SRC_INTERNAL;
        break;
    case 3:
        t_src = XSP3_GTIMA_SRC_IDC;
        break;
    case 4:
        t_src = XSP3_GTIMA_SRC_TTL_VETO_ONLY;
        break;
    case 5:
        t_src = XSP3_GTIMA_SRC_TTL_BOTH;
        break;
    case 6:
        t_src = XSP3_GTIMA_SRC_LVDS_VETO_ONLY;
        break;
    case 7:
        t_src = XSP3_GTIMA_SRC_LVDS_BOTH;
        break;
    default:
        THROW_HW_ERROR(Error) << "Invalid time frame source";
    }
    time_a = XSP3_GLOB_TIMA_TF_SRC(t_src);

    if (f0_invert)
        time_a |= XSP3_GLOB_TIMA_F0_INV;

    if (veto_invert)
        time_a |= XSP3_GLOB_TIMA_VETO_INV;

    if (debounce > 255) {
        THROW_HW_ERROR(Error) << "debounce value %d, should be less 255";
    }
    /* Default debounce = 80 cycles, 1 us */
    debounce_val = (debounce < 0) ? 80 : debounce;
    time_a |= XSP3_GLOB_TIMA_DEBOUNCE(debounce_val);

    if (loop_io)
        time_a |= XSP3_GLOB_TIMA_LOOP_IO;

    switch (alt_ttl_mode) {
        case       0 : alt_ttl = XSP3_ALT_TTL_TIMING_VETO; break;
        case 0x00400 : alt_ttl = XSP3_ALT_TTL_TIMING_ALL; break;
        case 0x00800 : alt_ttl = XSP3_ALT_TTL_INWINDOW; break;
        case 0x01000 : alt_ttl = XSP3_ALT_TTL_INWINLIVE; break;
        case 0x02000 : alt_ttl = XSP3_ALT_TTL_INWINLIVETOGGLE; break;
        case 0x04000 : alt_ttl = XSP3_ALT_TTL_INWINGOODLIVE; break;
        case 0x08000 : alt_ttl = XSP3_ALT_TTL_INWINGOODLIVETOGGLE; break;
        case 0x10000 : alt_ttl = XSP3_ALT_TTL_TIMING_VETO_GR; break;
        case 0x20000 : alt_ttl = XSP3_ALT_TTL_TIMING_ALL_GR; break;
        default:
            THROW_HW_ERROR(Error) << "Invalid alternate ttl mode for TTL Out";
    }
    time_a |= XSP3_GLOB_TIMA_ALT_TTL(alt_ttl);
    DEB_TRACE() << "global time_a register " << time_a;

    time_fixed = (first_frame < 0) ? 0 : first_frame;
    DEB_TRACE() << "first time frame " << time_fixed;

    if (xsp3_set_glob_timeA(m_handle, m_card, time_a) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    if (xsp3_set_glob_timeFixed(m_handle, m_card, time_fixed) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}

/**
 * Set the running format control parameters
 *
 * @param[in] chan is the number of the channel in the xspress3 system, 0 to ({@link xsp3_get_num_chan()} - 1)
 *             if chan is less than 0 then all channels are selected
 * @param[in] nbits_eng number bits of energy (default 12)
 * @param[in] adc_bits set bits for ADC range (set to 0 for user operation)
 *              1 = Use Top 4 bits of ADC Value for aux2 data
 *              2 = Use Top 6 bits of Aux or ADC data
 *              3 = Use Top 8 bits of Aux data
 *              4 = Use Top 10 bits of Aux
 *              5 = Use Top 12 bits of Aux
 *              6 = Use Top 14 bits of ADC value
 *              7 = Use all 16 bits of ADC data
 * @param[in] min_samples minimum samples (set to 0 for user operation)
 * @param[in] aux2_mode (set to 0 for user operation)
 *              1 = AUX=Event Width (Default=(Raw) ADC)
 *              2 = Aux=Top 4 ADC bits at reset start and the top 10 ADC bits
 *              3 = Aux=My and 3 neighbour Resets then top 10 ADC bits
 *              4 = Aux=My and 3 neighbour Resets, 4 bit ADC @ Rst Start & Top 6 ADC bits
 *              5 = Aux=My Reset and Time from last reset or glitch
 *              6 = Aux=My &Neb Rst & Time from last reset or glitch
 *              7 = Aux=My &Neb Rst & Time from last reset or glitch*8
 * @param[in] pile_reject Enable pileup rejection (set to 0 for user operation)
 */
void Camera::formatRun(int chan, int nbits_eng, int aux1_mode, int adc_bits, int min_samples, int aux2_mode, bool pileup_reject) {
    DEB_MEMBER_FUNCT();
    int adc = 0;
    u_int32_t disables = 0;
    int aux2 = 0;
    int aux1 = 0;

    if (nbits_eng < XSP3_MIN_BITS_ENG || nbits_eng > XSP3_MAX_BITS_ENG) {
        THROW_HW_ERROR(Error) << "Expected nbits eng in range " << XSP3_MIN_BITS_ENG << " .. " << XSP3_MAX_BITS_ENG << " not " << nbits_eng;
    }

    switch (aux1_mode) {
        case    0:      // make no res grade the default.
        case    1: aux1 = XSP3_FORMAT_RES_MODE_NONE;       break;
        case 0x02: aux1 = XSP3_FORMAT_RES_MODE_TOP;        break;
        case 0x04: aux1 = XSP3_FORMAT_RES_MODE_BOT;        break;
        case 0x08: aux1 = XSP3_FORMAT_RES_MODE_PILEUP;     break;
        default:
            THROW_HW_ERROR(Error) << "Invalid aux1 mode specified";
    }

    switch (adc_bits) {
        case 0: adc = XSP3_FORMAT_NBITS_AUX0; break;
        case 4: adc = XSP3_FORMAT_NBITS_AUX4;  break;
        case 6: adc = XSP3_FORMAT_NBITS_AUX6;  break;
        case 8: adc = XSP3_FORMAT_NBITS_AUX8;  break;
        case 10: adc = XSP3_FORMAT_NBITS_AUX10; break;
        case 12: adc = XSP3_FORMAT_NBITS_AUX12; break;
        case 14: adc = XSP3_FORMAT_NBITS_AUX14; break;
        case 16: adc = XSP3_FORMAT_NBITS_AUX16; break;
        default:
            THROW_HW_ERROR(Error) << "Invalid number of ADC/AUX bits";
    }

    if (pileup_reject)
        disables |= XSP3_FORMAT_PILEUP_REJECT; /* Actually an enable rather than disable but sorry */

    switch (aux2_mode) {
        case 0 : aux2 = 0; break;
        case 1 : aux2 = XSP3_FORMAT_AUX2_WIDTH; break;
        case 2 : aux2 = XSP3_FORMAT_AUX2_RST_START_ADC; break;
        case 3 : aux2 = XSP3_FORMAT_AUX2_NEB_RST_ADC; break;
        case 4 : aux2 = XSP3_FORMAT_AUX2_NEB_RST_RST_START_ADC; break;
        case 5 : aux2 = XSP3_FORMAT_AUX2_TIME_FROM_RST; break;
        case 6 : aux2 = XSP3_FORMAT_AUX2_NEB_RST_TIME_FROM_RST; break;
        case 7 : aux2 = XSP3_FORMAT_AUX2_NEB_RST_TIME_FROM_RSTX8; break;
        default:
            THROW_HW_ERROR(Error) << "Invalid aux2 mode specified";
    }

    if (xsp3_format_run(m_handle, chan, aux1, min_samples, adc, disables, aux2, nbits_eng) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    if (nbits_eng == 12) {
        if (xsp3_init_roi(m_handle, -1) < 0) {
            THROW_HW_ERROR(Error) << xsp3_get_error_message();
        }
    }
}

/**
 * Get the input data source in the channel control register.
 *
 * @param[in] chan is the number of the channel in the xspress3 system, 0 to ({@link xsp3_get_num_chan()} - 1)
 *             if chan is less than 0 then all channels are selected
 * @param[out] data_src input data source {@see DataSrc}
 */
void Camera::getDataSource(int chan, DataSrc& data_src) {
    DEB_MEMBER_FUNCT();
    u_int32_t chan_control;
    if (xsp3_get_chan_cont(m_handle, chan, &chan_control) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
    switch (chan_control & 0x7) {
        case Camera::Normal:
            data_src = Normal;
            break;
        case Camera::Alternate:
            data_src = Alternate;
            break;
        case Camera::Multiplexer:
            data_src = Multiplexer;
            break;
        case Camera::PlaybackStream0:
            data_src = PlaybackStream0;
            break;
        case Camera::PlaybackStream1:
            data_src = PlaybackStream1;
            break;
    }
    DEB_TRACE() << DEB_VAR1(data_src);
}

/**
 * Set the input data source in the channel control register.
 *
 * @param[in] chan is the number of the channel in the xspress3 system, 0 to ({@link xsp3_get_num_chan()} - 1)
 *             if chan is less than 0 then all channels are selected
 * @param[in] data_src input data source {@see DataSrc}
 */
void Camera::setDataSource(int chan, DataSrc data_src) {
    DEB_MEMBER_FUNCT();
    u_int32_t chan_control;
    int startChan;
    int numChan;
    int i;
    DEB_TRACE() << DEB_VAR2(chan, data_src);
    if (chan < 0) {
        startChan = 0;
        numChan = m_nb_chans;
    } else {
        startChan = chan;
        numChan = chan+1;
    }
    for (i = startChan; i < numChan; i++) {
        if (xsp3_get_chan_cont(m_handle, i, &chan_control) < 0) {
            THROW_HW_ERROR(Error) << xsp3_get_error_message();
        }
        chan_control &= ~0x7;
        chan_control |= data_src;
        if (xsp3_set_chan_cont(m_handle, i, chan_control) < 0) {
            THROW_HW_ERROR(Error) << xsp3_get_error_message();
        }
    }
}

/**
 * Setup xspress3 internal time frame generator.
 *
 * @param[in] nframe Number of time frames
 * @param[in] triggerMode
 * @param[in] gapMode Gap between frame
 */
//void setItfgTiming(int nframes, ItfgTriggerMode triggerMode, ItfgGapMode gapMode) {
void Camera::setItfgTiming(int nframes, int triggerMode, int gapMode) {
    DEB_MEMBER_FUNCT();
    int trig_mode;
    u_int32_t itime;
    int gap_mode;

    if (m_exp_time <= 0.0) {
        THROW_HW_ERROR(Error) << "Exposure time has not been set";
    }
    switch (triggerMode) {
        case Burst:
            trig_mode = XSP3_ITFG_TRIG_MODE_BURST;
            break;
        case SoftwarePause:
            trig_mode = XSP3_ITFG_TRIG_MODE_SOFTWARE;
            break;
        case HardwarePause:
            trig_mode = XSP3_ITFG_TRIG_MODE_HARDWARE;
            break;
        case SoftwareOnlyFirst:
            trig_mode = XSP3_ITFG_TRIG_MODE_SOFTWARE_ONLY_FIRST;
            break;
        case HardwareOnlyFirst:
            trig_mode = XSP3_ITFG_TRIG_MODE_HARDWARE_ONLY_FIRST;
            break;
        default:
            THROW_HW_ERROR(Error) << "Invalid trigger mode qualifiers";
    }

    if (m_exp_time > 12.5E-9*2.0*0x7FFFFFFF) {
        THROW_HW_ERROR(Error) << "Collection time " << m_exp_time << " too long, must be <= 12.5E-9*2.0*0x7FFFFFFF";
    }
    itime = (u_int32_t)(m_exp_time/12.5E-9);
    if (itime < 2) {
        THROW_HW_ERROR(Error) << "Minimum collection = 25 ns";
    }
    switch (gapMode) {
        case Gap25ns:
            gap_mode = XSP3_ITFG_GAP_MODE_25NS;
            break;
        case Gap200ns:
            gap_mode = XSP3_ITFG_GAP_MODE_200NS;
            break;
        case Gap500ns:
            gap_mode = XSP3_ITFG_GAP_MODE_500NS;
            break;
        case Gap1us:
        default:
            gap_mode = XSP3_ITFG_GAP_MODE_1US;
            break;
    }
    DEB_TRACE() << DEB_VAR4(nframes, itime, trig_mode, gap_mode);
    // use cardNos = 0 even in multi card systems for synchronisation
    if (xsp3_itfg_setup(m_handle, 0, nframes, itime, trig_mode, gap_mode) < 0) {
        THROW_HW_ERROR(Error) << xsp3_get_error_message();
    }
}
