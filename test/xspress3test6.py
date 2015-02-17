import PyTango
import numpy
import time

# definitions
PlaybackStream0 = 3
mode = 0 # burst
gap = 3 # 1us
debounce = 80 # clock cycles
nframes = 10
exp_time = 2.0 # seconds
cardNos = 0;
timeSource = 1 # internal
firstFrame = 0
ttl = 0 # default

dev = PyTango.DeviceProxy('xspress3/tango/1')
lima = PyTango.DeviceProxy('limaccd/tango/1')

# do not change the order of the saving attributes!
lima.write_attribute("saving_directory","/home/xspress3/desy/data")
lima.write_attribute("saving_format","HDf5")
lima.write_attribute("saving_overwrite_policy","Abort")
lima.write_attribute("saving_suffix", ".hdf")
lima.write_attribute("saving_prefix","xsp3_")
lima.write_attribute("saving_mode","MANUAL")
lima.write_attribute("saving_managed_mode","HARDWARE")
lima.write_attribute("saving_frames_per_file", nframes)

# do acquisition
dev.write_attribute("runMode")
dev.write_attribute("useDtc",False)
lima.write_attribute("acq_nb_frames",nframes)
lima.write_attribute("acq_expo_time", exp_time)
dev.write_attribute("setTiming",[timeSource, firstFrame, ttl, debounce])
dev.write_attribute("setItfgTiming", [nframes,mode,gap])
lima.write_attribute("acq_trigger_mode", "EXTERNAL_GATE")
lima.command_inout("prepareAcq")
lima.command_inout("startAcq")

while dev.read_attribute("acqRunning").value :
    time.sleep(0.5)
