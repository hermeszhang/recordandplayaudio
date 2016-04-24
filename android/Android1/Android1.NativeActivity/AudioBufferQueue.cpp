#include "AudioBufferQueue.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/system_wrappers/include/sleep.h"

//#define TEST
AudioBufferQueue::AudioBufferQueue()
{
    processing = nullptr;
}

AudioBufferQueue::~AudioBufferQueue()
{
    if (processing)
    {
        delete processing;
    }
}

void AudioBufferQueue::Init(size_t sample_rate, size_t channels )
{
    sample_rate_ = sample_rate;
    channels_ = channels;
    processing = AudioProcessing::Create();
    processing->echo_control_mobile()->Enable( true );
    processing->echo_control_mobile()->enable_comfort_noise( false );
    processing->echo_control_mobile()->set_routing_mode( EchoControlMobile::kLoudSpeakerphone );
    processing->noise_suppression()->Enable( true );
    processing->noise_suppression()->set_level( NoiseSuppression::kVeryHigh );
    processing->high_pass_filter()->Enable( true );
    processing->voice_detection()->Enable( true );
    processing->gain_control()->Enable( false );
    processing->gain_control()->set_compression_gain_db( 90 );
    processing->gain_control()->enable_limiter( false );
    processing->gain_control()->set_mode( GainControl::kAdaptiveDigital );
    processing->gain_control()->set_target_level_dbfs( 9 );
    processing->Initialize();
}

void AudioBufferQueue::PushBuffer( const void* buffer, size_t len )
{
    int64_t ts = TickTime::MillisecondTimestamp();

    AudioFrame af;
    af.UpdateFrame( 0, ts, (const int16_t*)buffer, 160, 16000, AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 1 );

    processing->set_delay_offset_ms( 0 );
    processing->set_stream_delay_ms(190);
    processing->ProcessStream( &af );
 
    {
        lock_guard ls( lock_ );
        if ( queue_pool_.empty() )
        {
            audio_buffer_list_.push( std::make_shared<AudioBuffer>( af.data_, len ) );
        }
        else
        {
            AudioBufferPtr ab = queue_pool_.front();
            queue_pool_.pop();
            ab->Reset( af.data_, len );
            audio_buffer_list_.push( ab );
        }
    }
}

bool AudioBufferQueue::PopBuffer( void* buffer,size_t & max_len )
{
    AudioFrame af;
    int64_t ts = TickTime::MillisecondTimestamp();

    AudioBufferPtr ab;
    {
        lock_guard ls( lock_ );
        if ( audio_buffer_list_.size() <200 )
        {
            memset( buffer, 0, max_len );
            return true;
        }
        else
        {
            ab = audio_buffer_list_.front();
            audio_buffer_list_.pop();

        }
    }
    af.UpdateFrame( 0, ts, (const int16_t*)ab->data, 160, 16000, AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 1 );

    processing->ProcessReverseStream( &af );
    max_len = std::min( ab->len, max_len );
    memcpy( buffer, af.data_, max_len );

    queue_pool_.push( ab );
    
    ts = TickTime::MicrosecondTimestamp() - ts;
    return true;
}

