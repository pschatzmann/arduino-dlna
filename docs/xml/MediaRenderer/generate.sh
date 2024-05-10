#!/bin/bash
# Generate header files from xml
xxsd -i connmgr.xml ../../../../src/DLNA/devices/MediaRenderer/connmgr.h
xxsd -i control.xml ../../../../src/DLNA/devices/MediaRenderer/control.h
xxsd -i transport.xml ../../../../src/DLNA/devices/MediaRenderer/transport.h