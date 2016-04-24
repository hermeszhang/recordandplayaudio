#pragma once
#include <queue>
#include <memory>
#include <mutex>

#include "webrtc/modules/audio_processing/include/audio_processing.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))

using namespace webrtc;
class AudioBufferQueue
{
public:
    AudioBufferQueue();
    ~AudioBufferQueue();
public:
    /*
    buffer_num
    */
    void Init( size_t sample_rate, size_t channels ); // Ô¤´æÈÝÁ¿
    void PushBuffer(const void* buffer, size_t len );
    bool PopBuffer(void* buffer, size_t & max_len );
private:
    struct AudioBuffer
    {
        void* data;
        size_t len;
        AudioBuffer() :data( nullptr ), len( 0 ) {}
        AudioBuffer( const void* data, size_t len )
        {
            this->data = malloc( len );
            memcpy( this->data, data, len );
            this->len = len;
        }
        void Reset( const void* data, size_t len )
        {
            if ( len != this->len )
            {
                this->data = realloc( this->data, len );
            }
            if ( this->data )
            {
                memcpy( this->data, data, len );
            }
            else
            {
                RTC_CHECK( this->data != NULL);
            }
        }
        ~AudioBuffer()
        {
            if ( data )
            {
                free( data );
            }
        }
    };
    typedef std::shared_ptr<AudioBuffer> AudioBufferPtr;
    std::queue<AudioBufferPtr> queue_pool_;
    std::queue<AudioBufferPtr> audio_buffer_list_;
    std::mutex lock_;
    typedef std::lock_guard<std::mutex> lock_guard;

    AudioProcessing* processing;
    size_t sample_rate_;
    size_t channels_;

};