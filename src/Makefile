############################################################################
# This file is part of LImA, a Library for Image Acquisition
#
# Copyright (C) : 2009-2013
# European Synchrotron Radiation Facility
# BP 220, Grenoble 38043
# FRANCE
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
############################################################################

xspress3-objs := Xspress3Camera.o Xspress3Interface.o Xspress3DetInfoCtrlObj.o Xspress3SyncCtrlObj.o Xspress3SavingCtrlObj.o

SRCS = $(xspress3-objs:.o=.cpp)

INCLUDES = -I../include \
           -I../../../common/include \
           -I../../../hardware/include \
           -I../../common/espia/include \
           -I../sdk/include \
	       -I../../../third-party/Processlib/core/include

CXXFLAGS += $(INCLUDES) -fPIC -Wall -pthread

ifneq ($(COMPILE_HDF5_SAVING),0)
INCLUDES += -I../../../third-party/hdf5-1.8.12/include
CXXFLAGS += -DWITH_HDF5_SAVING
endif

all: Xspress3.o

Xspress3.o: $(xspress3-objs)
	ld -o $@ -r $+

clean:
	rm -f Xspress3.o $(xspress3-objs) *.P

%.o : %.cpp
	$(COMPILE.cpp) -MD $(CXXFLAGS) -o $@ $<
	@cp $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	rm -f $*.d

-include $(SRCS:.cpp=.P)

.PHONY: check-syntax
check-syntax:
	$(CXX) -Wall -Wextra -fsyntax-only $(CXXFLAGS) $(CHK_SOURCES)
