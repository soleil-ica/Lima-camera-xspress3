import PyTango
import numpy
import time

dev=PyTango.DeviceProxy('xspress3/tango/1')
print "NumCards              :", dev.read_attribute("numcards").value
print "NumChan               :", dev.read_attribute("numchan").value
print "MaxNumChan            :", dev.read_attribute("maxnumchan").value
print "ChansPerCard          :", dev.read_attribute("chanspercard").value
print "BinsPerMca            :", dev.read_attribute("binspermca").value
print "Card (default)        :", dev.read_attribute("card").value

nframes = 10
exp_time = 2.0
allChannels = -1

lima=PyTango.DeviceProxy('limaccd/tango/1')
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
dev.write_attribute("card",0)
dev.write_attribute("channel", allChannels)
dev.write_attribute("dataSource",['PlaybackStream0'])
dev.write_attribute("playbackFilename","/home/xspress3/desy/data/Zr_mca15_pass0.d16")
dev.set_timeout_millis(30000)
dev.command_inout("loadPlayback",[0,0])
dev.write_attribute("runMode",[True])
dev.write_attribute("useDtc",False)
lima.write_attribute("acq_nb_frames",nframes)
lima.write_attribute("acq_expo_time",exp_time)
dev.write_attribute("setTiming",[0, 0, 0, 100])
lima.write_attribute("acq_trigger_mode", "INTERNAL_TRIGGER")
lima.command_inout("prepareAcq")
lima.command_inout("startAcq")

lastFrame = -1
channel = 0;
while dev.read_attribute("acqRunning").value :
    time.sleep(0.1)
    currentFrame=lima.read_attribute("last_image_ready").value
    if currentFrame > lastFrame:
        lastFrame = currentFrame
        data = dev.command_inout("ReadScalers",[lastFrame, channel])
        print "lastFrame ", lastFrame, " allevent " ,data[3]," allgood ",data[4]
        hdata = dev.command_inout("ReadHistogram",[lastFrame, channel])
        print "hist data ",hdata[1572]
