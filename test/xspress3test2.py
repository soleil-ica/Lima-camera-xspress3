import PyTango
import numpy
import time

# definitions
mode = 0 # burst
gap = 3 # 1us
debounce = 20 # clock cycles
nframes = 300
exp_time = 100.0 # seconds
cardNos = 0;
timeSource = 4 # 
firstFrame = 0
ttl = 0 # default
allChannels = -1

dev = PyTango.DeviceProxy('xspress3/tango/1')
lima=PyTango.DeviceProxy('limaccd/tango/1')
# do not change the order of the saving attributes!
lima.write_attribute("saving_directory","/home/grm84/data")
lima.write_attribute("saving_format","HDf5")
lima.write_attribute("saving_overwrite_policy","Abort")
lima.write_attribute("saving_suffix", ".hdf")
lima.write_attribute("saving_prefix","xsp3_")
lima.write_attribute("saving_mode","MANUAL")
lima.write_attribute("saving_managed_mode","HARDWARE")
lima.write_attribute("saving_frame_per_file", nframes)

dev.write_attribute("runMode",[True])
dev.write_attribute("useDtc",False)
lima.write_attribute("acq_nb_frames",nframes)
dev.write_attribute("setTiming",[timeSource, firstFrame, ttl, debounce])
dev.write_attribute("setItfgTiming", [nframes,mode,gap])
lima.write_attribute("acq_trigger_mode", "EXTERNAL_GATE")
lima.command_inout("prepareAcq")
lima.command_inout("startAcq")

while dev.read_attribute("acqRunning").value :
    time.sleep(0.5)
