#!/bin/bash
if [[ $1 == "feed" ]] ; then
    gst-launch-1.0 videotestsrc is-live=true ! video/x-raw,width=1280,width=720,framerate=30/1 ! queue ! \
        videoconvert ! \
        jpegenc ! \
        tcpclientsink host=127.0.0.1 port=8885

        # x264enc tune=zerolatency ! h264parse ! mpegtsmux ! \
        # tcpserversink host=0.0.0.0 port=8885
else
    gst-launch-1.0 -v tcpclientsrc host=127.0.0.1 port=8885 ! \
        jpegdec ! \
        autovideosink

        # tsdemux ! h264parse ! avdec_h264 ! \
fi
