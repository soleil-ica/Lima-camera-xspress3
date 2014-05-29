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

nframes = 3
m_exp_time = 5.0

lima=PyTango.DeviceProxy('limaccd/tango/1')
lima.write_attribute("saving_directory","/home/grm84/data")
lima.write_attribute("saving_format","HDf5")
lima.write_attribute("saving_prefix","xsp3_")
lima.write_attribute("saving_suffix", ".hdf")
lima.write_attribute("saving_mode","MANUAL")
lima.write_attribute("saving_managed_mode","HARDWARE")
lima.write_attribute("saving_frames_per_file",nframes)
lima.write_attribute("saving_overwrite_policy","OVERWRITE")

# do acquisition
dev.write_attribute("card",0)
#Camera::PlaybackStream0 = 3
dev.write_attribute("dataSource",[-1,3])
dev.write_attribute("playbackFilename","/home/grm84/git/Lima/camera/xspress3/test/Zr_mca15_pass0.d16")
dev.command_inout("loadPlayback")
dev.write_attribute("runMode",[True])
dev.write_attribute("useDtc",False)
#dev.write_attribute("dtcEnergy",10000.0)
#dev.write_attribute("dtcParameters", [-1,2.5304E-9,2.2534E-7, 2.5304E-9,2.2534E-7, True, False]);
lima.write_attribute("acq_nb_frames",nframes)
lima.write_attribute("acq_expo_time",m_exp_time)
dev.command_inout("clear")
dev.write_attribute("setTiming",[0, 0, 0, 100]);
lima.command_inout("prepareAcq")
lima.command_inout("startAcq")

while dev.read_attribute("acqRunning").value :
    time.sleep(0.5)
