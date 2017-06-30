# Xspress3 Test 1
# No file saving

import PyTango
import numpy
import time
import random

# definitions
nframes = 10
exp_time = 1 # seconds

dev = PyTango.DeviceProxy('lima/xspress3/1')
lima = PyTango.DeviceProxy('lima/limaccd/1')

dev.set_timeout_millis(30000)

lima.write_attribute("acq_nb_frames",nframes)
lima.write_attribute("acq_expo_time", exp_time)

# INTERNAL = Internal trigger
# EXTERNAL = TTL Veto (input TTL1)
#lima.write_attribute("acq_trigger_mode", "EXTERNAL_GATE")
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
                print "lastFrame", lastFrame, "ch", channel, "time", data[0]/80.0e6, "allevent", data[3], "allgood", data[4], "dt%", data[9], "dtf", data[10]
                hdata = dev.command_inout("ReadHistogram",[lastFrame, channel])
                print "hist data ",hdata

    except (KeyboardInterrupt, SystemExit):
        lima.command_inout("stopAcq")