#!/usr/bin/env python
from PyTango import Database, DbDevInfo, DeviceProxy
import argparse

parser = argparse.ArgumentParser(description='Create Xspress3 Devices in Tango DB')
parser.add_argument('-n','--name', help='Instance name, usually xspress3', required=True, metavar='instance_name')
parser.add_argument('-x','--xspress3', help='Xspress 3 device name, default lima/xspress3/1', default='lima/xspress3/1', metavar='x3_dev_name')
parser.add_argument('-l','--lima', help='LimaCCD device name, default lima/limaccd/1', default='lima/limaccd/1', metavar='lima_dev_name')
parser.add_argument('-c','--channels', help='No of channels, default 1', type=int, default=1, metavar='no_channels')
args = vars(parser.parse_args())

db = Database()

print 'Creating LimaCCDs device'
lima = DbDevInfo()
lima._class = 'LimaCCDs'
lima.server = 'LimaCCDs/{instance}'.format(instance=args['name'])
lima.name = args['lima']
db.add_device(lima)

print 'Creating Xspress3 device'
x3 = DbDevInfo()
x3._class = 'Xspress3'
x3.server = 'LimaCCDs/{instance}'.format(instance=args['name'])
x3.name = args['xspress3']
db.add_device(x3)

print 'Setting Properties'
lima = DeviceProxy(args['lima'])
lima.put_property({ 'LimaCameraType': 'Xspress3' })

x3 = DeviceProxy(args['xspress3'])
x3.put_property({
	'baseIPaddress': '192.168.0.1',
	'baseMacAddress': '02.00.00.00.00.00',
	'basePort':	30123,
	'maxFrames': 16384,
	'nbChans': args['channels'],
	'directoryName': '',
	'cardIndex': 0
})

print 'Done'
