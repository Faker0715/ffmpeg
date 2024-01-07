#pragma once

#include <iostream>
#include <thread>


#include "singleton_template.h"



class StreamReaderObserver {
public:
	virtual ~StreamReaderObserver() {}
	virtual void OnVideoDecodedFrame(const uint8_t* data,int datalen,int width,int height,int fps) = 0;
    virtual void OnVideoRawFrame(const uint8_t* data,int datalen,int width,int height,int fps) = 0;
    
    virtual void OnAudioDecodedFrame(const uint8_t* data,int datalen,int channels,int samples) = 0;
    virtual void OnAudioRawFrame(const uint8_t* data,int datalen,int channels,int samples) = 0;
};

class StreamReader {
public:
    static std::shared_ptr<StreamReader> create(int type);

    virtual ~StreamReader() {};
    virtual int OpenStream(std::string strUrl, StreamReaderObserver *observer) = 0;
    virtual int CloseStream() = 0;
    virtual void SetNotDecoder(bool on) = 0;
};


