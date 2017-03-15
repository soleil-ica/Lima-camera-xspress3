# Xspress3 Test 3
# Using playback data

import PyTango
import numpy
import time
import random

# definitions
nframes = 10
exp_time = 1 # seconds

dev = PyTango.DeviceProxy('lima/xspress3/1')
lima = PyTango.DeviceProxy('lima/limaccd/1')

dev.write_attribute("playbackFilename","/home/xspress3/remote/calibration/initial/playback/Ch1_Pt_37kHz_pass0.d16")
dev.command_inout("loadPlayback",[0,0,1])
dev.set_timeout_millis(30000)

lima.write_attribute("acq_nb_frames",nframes)
lima.write_attribute("acq_expo_time", exp_time)
lima.write_attribute("acq_trigger_mode", "INTERNAL_TRIGGER")
lima.command_inout("prepareAcq")
lima.command_inout("startAcq")

lastFrame = -1
channel = 0;

while dev.read_attribute("acqRunning").value :
    try:
        time.sleep(0.5)
        currentFrame=lima.read_attribute("last_image_ready").value
        if currentFrame > lastFrame:
            lastFrame = currentFrame
            data = dev.command_inout("ReadScalers",[lastFrame, channel])
            print "lastFrame", lastFrame, "allevent" ,data[3],"allgood",data[4], "dt%", data[9], "dtf", data[10]
            hdata = dev.command_inout("ReadHistogram",[lastFrame, channel])
            print "hist data ",hdata

    except (KeyboardInterrupt, SystemExit):
        lima.command_inout("stopAcq")