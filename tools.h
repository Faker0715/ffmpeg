#pragma once

#include <iostream>
#include <fstream>


enum StreamState {
	StreamStateStopped = 0,
	StreamStateStarted,
};



class FfmpegGlobal {
public:
    FfmpegGlobal();

    ~FfmpegGlobal() ;
};

template< typename... Args >
std::string vformat(const char* format, Args... args);
int find_sei(uint8_t *buf, int len);
int find_sps(uint8_t *buf, int len);