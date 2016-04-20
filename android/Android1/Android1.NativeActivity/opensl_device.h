#pragma once
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <functional>
#include <mutex>
#include <list>
typedef std::function<int( void* data, size_t bytes_size )> playCallback;
typedef std::function<void( const void* data, size_t bytes_size )> recordCallback;
class OpenSLDevice
{
public:
    static const int kNumBuffer = 5;
    OpenSLDevice();
    ~OpenSLDevice();
    bool Init();
    void Terminate();
    bool InitPlayout( int sample_rate, int channels,int bufferframes );
    bool InitRecording( int sample_rate, int channels, int bufferframes );
    bool StartPlayOut();
    bool StartRecording();
    bool StopPlayout();
    bool StopRecording();
    playCallback playcallback_;
    recordCallback recordcallback_;
private:
    static void PlayBufferQueueCallback( SLAndroidSimpleBufferQueueItf bq, void *context );
    static void RecordBufferQueueCallback( SLAndroidSimpleBufferQueueItf bq, void *context );
    void FillBufferQueue();
    void EnqueuePlayoutData();
    void EnqueueRecordData();
private:
    bool initialize_;
    bool init_playout_;
    bool init_recording;
    bool playing_;
    bool recording_;
    SLuint32 sample_rate_rec_;
    SLuint32 sample_rate_ply_;
    SLuint32 channle_rec_;
    SLuint32 channle_ply_;

    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;

    // output mix interfaces
    SLObjectItf outputMixObject;

    // buffer queue player interfaces
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLEffectSendItf bqPlayerEffectSend;

    // recorder interfaces
    SLObjectItf recorderObject;
    SLRecordItf recorderRecord;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue;

    // size of buffers
    int outBufSamples;
    int inBufSamples;

    // buffer indexes
    int currentInputIndex;
    int currentOutputIndex;

    // current buffer half (0, 1)
    int currentOutputBuffer;
    int currentInputBuffer;

    // buffers
    SLint16*outputBuffer[kNumBuffer];
    SLint16 *inputBuffer[kNumBuffer];
    std::mutex lock_;
    std::list<SLint16*> buffer_pool_;
    std::list<SLint16*> buffer_list_;


    // audio effect
    SLAndroidEffectItf audio_effect_obj;
    SLAndroidEffectCapabilitiesItf audio_effect_capbilities_obj;
};