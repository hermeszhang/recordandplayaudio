#include "AudioBufferQueue.h"
#include "webrtc/modules/include/module_common_types.h"
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
    std::string strfilename("/sdcard/log/2016-04-19-rec-src.wav");
    wav_rec_src = new WavWriter( strfilename, sample_rate, channels );
    strfilename = ( "/sdcard/log/2016-04-19-rec-pro.wav" );
    wav_rec_pro = new WavWriter( strfilename, sample_rate, channels );
    strfilename = "/sdcard/log/2016-04-19-ply-pro.wav";
    wav_ply_pro = new WavWriter( strfilename, sample_rate, channels );
    
    sample_rate_ = sample_rate;
    channels_ = channels;
    processing = AudioProcessing::Create();
    processing->echo_control_mobile()->Enable( true );
    processing->echo_control_mobile()->enable_comfort_noise( false );
    processing->echo_control_mobile()->set_routing_mode( EchoControlMobile::kSpeakerphone );
    processing->noise_suppression()->Enable( true );
    processing->noise_suppression()->set_level( NoiseSuppression::kModerate );
    processing->high_pass_filter()->Enable( true );
    processing->voice_detection()->Enable( true );
    processing->Initialize();
}

void AudioBufferQueue::PushBuffer( const void* buffer, size_t len )
{
    static uint32_t ts = 0;
    static uint32_t m_check_vad = 0;
    wav_rec_src->WriteSamples((const int16_t*) buffer, len / 2 );
    AudioFrame af;
    af.UpdateFrame( 0, ts, (const int16_t*)buffer, 160, 16000, AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 1 );
    ts += 10;
    processing->delay_offset_ms();
    processing->set_stream_delay_ms(50);
    processing->ProcessStream( &af );
    if ( af.vad_activity_ == AudioFrame::kVadPassive )// 静音不需要处理
    {
        //memset( af.data_, 0, len );
        m_check_vad++;
        if ( m_check_vad > 50 )
        {
        //    return ;
        }
    }
    else
    {
        m_check_vad = 0;
    }
    {
        lock_guard ls( lock_ );
        if ( queue_pool_.empty() )
        {
            audio_buffer_list_.push( std::make_shared<AudioBuffer>( af.data_, len ) );
            //wav_rec_pro->WriteSamples( af.data_, len / 2 );
        }
        else
        {
            AudioBufferPtr ab = queue_pool_.front();
            queue_pool_.pop();
            ab->Reset( af.data_, len );
            audio_buffer_list_.push( ab );
           // wav_rec_pro->WriteSamples( (const int16_t*)ab->data, len / 2 );
        }
    }


    wav_rec_pro->WriteSamples( af.data_, len/2 );
}

bool AudioBufferQueue::PopBuffer( void* buffer,size_t & max_len )
{
    LOGW(" AudioBufferQueue::PopBuffer,max_Len:%d",max_len );
    static int ts = 0;
    //wav_ply->ReadSamples( max_len/2, (int16_t*)buffer );
    
    AudioBufferPtr ab;
    {
        lock_guard ls( lock_ );
        if ( audio_buffer_list_.empty() )
        {
            memset( buffer, 0, max_len );
            return true;
        }
        else
        {
            ab = audio_buffer_list_.front();
            audio_buffer_list_.pop();
           // if(ab->data != NULL && ab->len == 320)
           // wav_ply_pro->WriteSamples( (const int16_t*)ab->data, 160 );
        }
    }


    AudioFrame af;
    af.UpdateFrame( 0, ts, (const int16_t*)ab->data, 160, 16000, AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, 1 );
    ts += 10;
    processing->ProcessReverseStream( &af );
    max_len = std::min( ab->len, max_len );
    memcpy( buffer, af.data_, max_len );

    queue_pool_.push( ab );
    
    
    //memset( buffer, 0, max_len );
    return true;
}

