#!/bin/bash
if [[ $1 == "feed" ]] ; then
    gst-launch-1.0 -v videotestsrc is-live=true ! video/x-raw,width=640,width=480,framerate=30/1 ! queue ! \
        videoconvert ! jpegenc ! \
        tcpclientsink host=127.0.0.1 port=8885
else
    gst-launch-1.0 -v tcpclientsrc host=127.0.0.1 port=8885 ! \
        jpegdec ! \
        autovideosink
fi
