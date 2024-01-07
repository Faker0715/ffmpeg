#pragma once

#include <iostream>
#include <thread>


#include "singleton_template.h"


class StreamWriterObserver {
public:
	virtual ~StreamWriterObserver() {}
	virtual void GetVideoDecodedFrame(const uint8_t** data,int *datalen,int *width,int *height,int *fps) = 0;
    virtual void GetVideoRawFrame(const uint8_t** data,int *datalen,int *width,int *height,int *fps) = 0;
    
    virtual void GetAudioDecodedFrame(const uint8_t** data,int *datalen,int *channels,int *samples) = 0;
    virtual void GetAudioRawFrame(const uint8_t** data,int *datalen,int *channels,int *samples) = 0;
    
    void setWriter(void * This) {
        m_pWriter = This;
    }
    void *getWriter() {
        return m_pWriter;
    }
private:
    void * m_pWriter = NULL;

};

class StreamWriter {
public:
    static std::shared_ptr<StreamWriter> create(int type);

    virtual ~StreamWriter() {};
    virtual int OpenStream(std::string strUrl, StreamWriterObserver *observer) = 0;
    virtual int CloseStream() = 0;
    virtual void SetNotDecoder(bool on) = 0;
};


