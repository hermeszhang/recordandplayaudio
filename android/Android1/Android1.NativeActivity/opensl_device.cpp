#include "opensl_device.h"
#include "stdio.h"
#include <android/log.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "AndroidProject1.NativeActivity", __VA_ARGS__))
OpenSLDevice::OpenSLDevice()
{
    initialize_ = false;
    init_playout_ = false;
    init_recording = false;
    playing_ = false;
    recording_ = false;
    sample_rate_rec_ = 48000;
    sample_rate_ply_ = 48000;
    channle_rec_ = 2;
    channle_ply_ = 2;
    engineObject = NULL;
    engineEngine = NULL;
    outputMixObject = NULL;
    bqPlayerObject = NULL;
    bqPlayerBufferQueue = NULL;
    bqPlayerEffectSend = NULL;
    bqPlayerPlay = NULL;
    recorderObject = NULL;
    recorderRecord = NULL;
    recorderBufferQueue = NULL;
    currentOutputBuffer = 0;
    currentInputIndex = 0;
    currentOutputIndex = 0;
    currentInputBuffer = 0;
}

OpenSLDevice::~OpenSLDevice()
{
    Terminate();
}

bool OpenSLDevice::Init()
{
    if (initialize_)
    {
        return true;
    }


    // 创建OpenSL引擎
    SLresult result;
    // create engine
    result = slCreateEngine( &engineObject, 0, NULL, 0, NULL, NULL );
    if ( result != SL_RESULT_SUCCESS ) return  result;

    // realize the engine 
    result = ( *engineObject )->Realize( engineObject, SL_BOOLEAN_FALSE );
    if ( result != SL_RESULT_SUCCESS ) return result;

    // get the engine interface, which is needed in order to create other objects
    result = ( *engineObject )->GetInterface( engineObject, SL_IID_ENGINE, &engineEngine );
    if ( result != SL_RESULT_SUCCESS ) return result;


    // for test
    SLuint32 numinterfaces;
    result = slQueryNumSupportedEngineInterfaces( &numinterfaces );
    if (result == SL_RESULT_SUCCESS)
    {
        LOGW( "slQueryNumSupportedEngineInterfaces:%d",numinterfaces );
    }
    initialize_ = true;
    return true;
}

void OpenSLDevice::Terminate()
{
    if (!initialize_)
    {
        return;
    }

    StopPlayout();
    StopRecording();

    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if ( bqPlayerObject != NULL )
    {
        ( *bqPlayerObject )->Destroy( bqPlayerObject );
        bqPlayerObject = NULL;
        bqPlayerPlay = NULL;
        bqPlayerBufferQueue = NULL;
        bqPlayerEffectSend = NULL;
    }

    // destroy audio recorder object, and invalidate all associated interfaces
    if ( recorderObject != NULL )
    {
        ( *recorderObject )->Destroy( recorderObject );
        recorderObject = NULL;
        recorderRecord = NULL;
        recorderBufferQueue = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if ( outputMixObject != NULL )
    {
        ( *outputMixObject )->Destroy( outputMixObject );
        outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if ( engineObject != NULL )
    {
        ( *engineObject )->Destroy( engineObject );
        engineObject = NULL;
        engineEngine = NULL;
    }
    initialize_ = false;
}

bool OpenSLDevice::InitPlayout( int sample_rate, int channels, int bufferframes )
{
    LOGW(" OpenSLDevice::InitPlayout enter");
    if ( !initialize_ )
    {
        return false;
    }

    if ( init_playout_ )
    {
        return false;
    }
    sample_rate_ply_ = sample_rate;
    channle_ply_ = channels;
    outBufSamples = bufferframes * channle_ply_;
    for (int i = 0; i < kNumBuffer; i++)
    {
        outputBuffer[i] = (SLint16*)calloc( outBufSamples, sizeof( SLint16 ) );
    }

    for ( int i = 0; i < 100; i++ )
    {
        buffer_pool_.push_back( (SLint16*)calloc( outBufSamples, sizeof( SLint16 ) ));
    }

    if ( channle_ply_ != 1 && channle_ply_ != 2 )
    {
        return false;
    }



    switch ( sample_rate_ply_ )
    {

    case 8000:
        sample_rate_ply_ = SL_SAMPLINGRATE_8;
        break;
    case 11025:
        sample_rate_ply_ = SL_SAMPLINGRATE_11_025;
        break;
    case 16000:
        sample_rate_ply_ = SL_SAMPLINGRATE_16;
        break;
    case 22050:
        sample_rate_ply_ = SL_SAMPLINGRATE_22_05;
        break;
    case 24000:
        sample_rate_ply_ = SL_SAMPLINGRATE_24;
        break;
    case 32000:
        sample_rate_ply_ = SL_SAMPLINGRATE_32;
        break;
    case 44100:
        sample_rate_ply_ = SL_SAMPLINGRATE_44_1;
        break;
    case 48000:
        sample_rate_ply_ = SL_SAMPLINGRATE_48;
        break;
    case 64000:
        sample_rate_ply_ = SL_SAMPLINGRATE_64;
        break;
    case 88200:
        sample_rate_ply_ = SL_SAMPLINGRATE_88_2;
        break;
    case 96000:
        sample_rate_ply_ = SL_SAMPLINGRATE_96;
        break;
    case 192000:
        sample_rate_ply_ = SL_SAMPLINGRATE_192;
        break;
    default:
        return false;
    }

    SLresult result;
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, kNumBuffer };
    const SLInterfaceID ids[] = { SL_IID_VOLUME };
    const SLboolean req[] = { SL_BOOLEAN_FALSE };
    result = ( *engineEngine )->CreateOutputMix( engineEngine, &outputMixObject, 1, ids, req );
    if ( result != SL_RESULT_SUCCESS )
        return result;

    // realize the output mix
    result = ( *outputMixObject )->Realize( outputMixObject, SL_BOOLEAN_FALSE );

    SLuint32 speakers;
    speakers = channle_ply_ > 1 ? SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER;

    SLDataFormat_PCM format_pcm = { SL_DATAFORMAT_PCM,channle_ply_, sample_rate_ply_,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        speakers, SL_BYTEORDER_LITTLEENDIAN };

    SLDataSource audioSrc = { &loc_bufq, &format_pcm };

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, outputMixObject };
    SLDataSink audioSnk = { &loc_outmix, NULL };

    // create audio player
    const SLInterfaceID ids1[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean req1[] = { SL_BOOLEAN_TRUE };
    result = ( *engineEngine )->CreateAudioPlayer( engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                    1, ids1, req1 );
    if ( result != SL_RESULT_SUCCESS )
        return result;

    // realize the player
    result = ( *bqPlayerObject )->Realize( bqPlayerObject, SL_BOOLEAN_FALSE );
    if ( result != SL_RESULT_SUCCESS )
        return result;

    // get the play interface
    result = ( *bqPlayerObject )->GetInterface( bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay );
    if ( result != SL_RESULT_SUCCESS )
        return result;

    // get the buffer queue interface
    result = ( *bqPlayerObject )->GetInterface( bqPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                &bqPlayerBufferQueue );
    if ( result != SL_RESULT_SUCCESS )
        return result;

    // register callback on the buffer queue
    result = ( *bqPlayerBufferQueue )->RegisterCallback( bqPlayerBufferQueue, PlayBufferQueueCallback, this );
    if ( result != SL_RESULT_SUCCESS )
        return result;
    init_playout_ = true;
    LOGW( " OpenSLDevice::InitPlayout leave" );
    return true;
}


bool OpenSLDevice::InitRecording( int sample_rate, int channels, int bufferframes )
{
    LOGW( "OpenSLDevice::InitRecording enter" );
    if ( !initialize_ )
    {
        return false;
    }

    sample_rate_rec_ = sample_rate;
    channle_rec_ = channels;

    if ( channle_rec_ != 1 && channle_rec_ != 2 )
    {
        return false;
    }

    inBufSamples = bufferframes * channle_rec_;
    for ( int i = 0; i < kNumBuffer; i++ )
    {
        inputBuffer[i] = (SLint16*)calloc( inBufSamples, sizeof( SLint16 ) );
    }

    switch ( sample_rate_rec_ )
    {

    case 8000:
        sample_rate_rec_ = SL_SAMPLINGRATE_8;
        break;
    case 11025:
        sample_rate_rec_ = SL_SAMPLINGRATE_11_025;
        break;
    case 16000:
        sample_rate_rec_ = SL_SAMPLINGRATE_16;
        break;
    case 22050:
        sample_rate_rec_ = SL_SAMPLINGRATE_22_05;
        break;
    case 24000:
        sample_rate_rec_ = SL_SAMPLINGRATE_24;
        break;
    case 32000:
        sample_rate_rec_ = SL_SAMPLINGRATE_32;
        break;
    case 44100:
        sample_rate_rec_ = SL_SAMPLINGRATE_44_1;
        break;
    case 48000:
        sample_rate_rec_ = SL_SAMPLINGRATE_48;
        break;
    case 64000:
        sample_rate_rec_ = SL_SAMPLINGRATE_64;
        break;
    case 88200:
        sample_rate_rec_ = SL_SAMPLINGRATE_88_2;
        break;
    case 96000:
        sample_rate_rec_ = SL_SAMPLINGRATE_96;
        break;
    case 192000:
        sample_rate_rec_ = SL_SAMPLINGRATE_192;
        break;
    default:
        return false;
    }

    SLresult result;
    // configure audio source
    SLDataLocator_IODevice loc_dev = { SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
        SL_DEFAULTDEVICEID_AUDIOINPUT, NULL };

    SLDataSource audioSrc = { &loc_dev, NULL };

    // configure audio sink
    SLuint32 speakers = channle_rec_ > 1 ? SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER;

    SLDataLocator_AndroidSimpleBufferQueue loc_bq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, kNumBuffer };
    SLDataFormat_PCM format_pcm = { SL_DATAFORMAT_PCM, channle_rec_, sample_rate_rec_,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        speakers, SL_BYTEORDER_LITTLEENDIAN };
    SLDataSink audioSnk = { &loc_bq, &format_pcm };

    // create audio recorder
    // (requires the RECORD_AUDIO permission)
    const SLInterfaceID id[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean req[1] = { SL_BOOLEAN_TRUE };
    result = ( *engineEngine )->CreateAudioRecorder( engineEngine, &recorderObject, &audioSrc,
                                                        &audioSnk, 1, id, req );
    if ( SL_RESULT_SUCCESS != result ) return false;

    // realize the audio recorder
    result = ( *recorderObject )->Realize( recorderObject, SL_BOOLEAN_FALSE );
    if ( SL_RESULT_SUCCESS != result ) return false;

    // get the record effect interface
    result = ( *engineObject )->GetInterface( engineObject, SL_IID_ANDROIDEFFECT, &audio_effect_obj );
    if (result != SL_RESULT_SUCCESS)
    {
        LOGW( "the device is not supported the audio effect:%d",result );
    }

    result = ( *engineObject )->GetInterface( engineObject, SL_IID_ANDROIDEFFECTCAPABILITIES, &audio_effect_capbilities_obj );
    if ( result != SL_RESULT_SUCCESS )
    {
        LOGW( "the device is not supported the audio effect capbility:%d", result );
    }

    // get the record interface
    result = ( *recorderObject )->GetInterface( recorderObject, SL_IID_RECORD, &recorderRecord );
    if ( SL_RESULT_SUCCESS != result ) return false;

    // get the buffer queue interface
    result = ( *recorderObject )->GetInterface( recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                   &recorderBufferQueue );
    if ( SL_RESULT_SUCCESS != result ) return false;

    // register callback on the buffer queue
    result = ( *recorderBufferQueue )->RegisterCallback( recorderBufferQueue, RecordBufferQueueCallback,
                                                            this );
    if ( SL_RESULT_SUCCESS != result ) return false;

    init_recording = true;
    LOGW( "OpenSLDevice::InitRecording leave" );
    return true;
}


bool OpenSLDevice::StartPlayOut()
{
    LOGW("OpenSLDevice::StartPlayOut() enter");
    if (!initialize_ || !init_playout_ )
    {
        return false;
    }

    if (playing_)
    {
        return true;
    }

    // set the player's state to playing
    SLresult result = ( *bqPlayerPlay )->SetPlayState( bqPlayerPlay, SL_PLAYSTATE_PLAYING );
    if (result != SL_RESULT_SUCCESS)
    {
        return false;
    }
    playing_ = true;
    for ( int i = 0; i < kNumBuffer; ++i )
    {
        //EnqueuePlayoutData();
        // Enqueue the decoded audio buffer for playback.
        SLint16* buffer = outputBuffer[currentOutputBuffer];
        SLresult err = ( *bqPlayerBufferQueue )->Enqueue( bqPlayerBufferQueue, buffer, outBufSamples * 2 );
        if ( SL_RESULT_SUCCESS != err )
        {
            LOGW( "add playout data fail,err=%d", err );
        }
        currentOutputBuffer = ( currentOutputBuffer + 1 ) % kNumBuffer;
    }
    LOGW( "OpenSLDevice::StartPlayOut() leave" );

    return true;
}

bool OpenSLDevice::StartRecording()
{
    LOGW( "OpenSLDevice::StartRecording() enter" );
    if (!initialize_ || !init_recording)
    {
        return false;
    }

    if (recording_)
    {
        return true;
    }
    SLresult result = ( *recorderRecord )->SetRecordState( recorderRecord, SL_RECORDSTATE_RECORDING );
    if (result != SL_RESULT_SUCCESS)
    {
        return false;
    }
    recording_ = true;
    for ( int i = 0; i < kNumBuffer; ++i )
    {
        SLint16* buffer = inputBuffer[currentInputBuffer];
        SLresult err = ( *recorderBufferQueue )->Enqueue( recorderBufferQueue, buffer, inBufSamples * sizeof( SLint16 ) );
        if ( SL_RESULT_SUCCESS != err )
        {
            LOGW( "get recording data fail,err=%d", err );
        }
        currentInputBuffer = ( currentInputBuffer + 1 ) % kNumBuffer;
    }
    LOGW( "OpenSLDevice::StartRecording() leave" );
    return true;
}


bool OpenSLDevice::StopPlayout()
{
    if ( !initialize_ || !init_playout_ )
    {
        return false;
    }

    if (!playing_)
    {
        return false;
    }
    // set the player's state to playing
    SLresult result = ( *bqPlayerPlay )->SetPlayState( bqPlayerPlay, SL_PLAYSTATE_PAUSED ); // 暂停还是停止呢？
    if ( result != SL_RESULT_SUCCESS )
    {
        return false;
    }
    playing_ = false;
    return true;
}

bool OpenSLDevice::StopRecording()
{
    if ( !initialize_ || !init_playout_ )
    {
        return false;
    }

    if (!recording_)
    {
        return true;
    }

    SLresult result = ( *recorderRecord )->SetRecordState( recorderRecord, SL_RECORDSTATE_PAUSED );
    if ( result != SL_RESULT_SUCCESS )
    {
        return false;
    }

    recording_ = false;

    return true;
}



// this callback handler is called every time a buffer finishes playing
void OpenSLDevice::PlayBufferQueueCallback( SLAndroidSimpleBufferQueueItf bq, void *context )
{
   // LOGW(" OpenSLDevice::PlayBufferQueueCallback");
    OpenSLDevice *p = (OpenSLDevice*)context;
    p->FillBufferQueue();
}


// this callback handler is called every time a buffer finishes recording
void OpenSLDevice::RecordBufferQueueCallback( SLAndroidSimpleBufferQueueItf bq, void *context )
{
   // LOGW( " OpenSLDevice::RecordBufferQueueCallback" );
    OpenSLDevice *p = (OpenSLDevice *)context;
    p->EnqueueRecordData();
}

void OpenSLDevice::FillBufferQueue()
{
    if ( !playing_ )
        return;
    EnqueuePlayoutData();
}

void OpenSLDevice::EnqueuePlayoutData()
{
    // Read audio data  from callback function registered by user to adjust for differences in buffer size between user and native
    // OpenSL ES.
    SLint16* buffer = outputBuffer[currentOutputBuffer];
   // Get play data from callback;
    int len = outBufSamples * 2;
    if ( playcallback_ )
    {
        len = playcallback_( buffer, outBufSamples*sizeof( SLint16 ) );
    }
    else
    {
        std::lock_guard<std::mutex> lg(lock_);
        if ( !buffer_list_.empty() )
        {
            SLint16* data = buffer_list_.front();
            memcpy( buffer, data, outBufSamples * 2 );
            buffer_list_.pop_front();
            buffer_pool_.push_back( data );
        }
    }
    // Enqueue the decoded audio buffer for playback.
    SLresult err = ( *bqPlayerBufferQueue )->Enqueue( bqPlayerBufferQueue, buffer, outBufSamples * 2 );
    if ( SL_RESULT_SUCCESS != err )
    {
        LOGW( "add playout data fail,err=%d", err );
    }

    currentOutputBuffer = ( currentOutputBuffer + 1 ) % kNumBuffer;
}



void OpenSLDevice::EnqueueRecordData()
{
    if (!recording_)
    {
        return;
    }
    SLint16* buffer = inputBuffer[currentInputBuffer];
    SLresult err = ( *recorderBufferQueue )->Enqueue( recorderBufferQueue, buffer, inBufSamples*sizeof(SLint16) );
    if ( SL_RESULT_SUCCESS != err )
    {
        LOGW( "get recording data fail,err=%d", err );
    }
    if ( recordcallback_ )
    {
        recordcallback_( buffer, inBufSamples * sizeof(SLint16) );
    }
    else
    {
        std::lock_guard<std::mutex> lg( lock_ );
        if (!buffer_pool_.empty())
        {
            SLint16*p = buffer_pool_.front();
            memcpy( p, buffer, inBufSamples * sizeof( SLint16 ) );
            buffer_pool_.pop_front();
            buffer_list_.push_back( p );
        }
    }
    currentInputBuffer = ( currentInputBuffer + 1 ) % kNumBuffer;

}