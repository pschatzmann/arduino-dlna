#!/bin/bash
# Generate header files from xml
xxd -i connmgr.xml ../../../../src/DLNA/devices/MediaRenderer/connmgr.h
xxd -i control.xml ../../../../src/DLNA/devices/MediaRenderer/control.h
xxd -i transport.xml ../../../../src/DLNA/devices/MediaRenderer/transport.h