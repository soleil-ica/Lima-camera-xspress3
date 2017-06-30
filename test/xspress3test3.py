# Xspress3 Test 3
# Using playback data

import PyTango
import numpy
import time
import random
import sys

# definitions
nframes = 10
exp_time = 1 # seconds

dev = PyTango.DeviceProxy('lima/xspress3/1')
lima = PyTango.DeviceProxy('lima/limaccd/1')

if len(sys.argv) > 1:
    file = sys.argv[1]
else:
    file = "/home/xspress3/remote/calibration/initial/playback/Ch1_Pt_37kHz_pass0.d16"

dev.set_timeout_millis(30000)
dev.write_attribute("playbackFilename",file)
dev.command_inout("loadPlayback",[0,0,1])

lima.write_attribute("acq_nb_frames",nframes)
lima.write_attribute("acq_expo_time", exp_time)
lima.write_attribute("acq_trigger_mode", "INTERNAL_TRIGGER")
lima.command_inout("prepareAcq")
lima.command_inout("startAcq")

lastFrame = -1

while dev.read_attribute("acqRunning").value :
    try:
        time.sleep(0.5)
        currentFrame=lima.read_attribute("last_image_ready").value
        if currentFrame > lastFrame:
            for channel in range(dev.numChan):
                lastFrame = currentFrame
                data = dev.command_inout("ReadScalers",[lastFrame, channel])
                print "lastFrame", lastFrame, "ch", channel, "time", data[0]/80.0e6, "allevent" ,data[3], "allgood", data[4], "dt%", data[9], "dtf", data[10]
                hdata = dev.command_inout("ReadHistogram",[lastFrame, channel])
                print "hist data ",hdata

    except (KeyboardInterrupt, SystemExit):
        lima.command_inout("stopAcq")