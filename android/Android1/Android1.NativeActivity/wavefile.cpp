#include "wavefile.h"
#include "webrtc\modules\include\module_common_types.h"
#include "webrtc/system_wrappers/include/tick_util.h"


WaveFile::WaveFile()
{
    sample_rate_ = 16000;
    channels_ = 1;
    processing_ = nullptr;
}

WaveFile::~WaveFile()
{
    delete processing_;
}
void WaveFile::Init( size_t sample_rate, size_t channels )
{
    sample_rate_ = sample_rate;
    channels_ = channels;
    std::string filename("/sdcard/log/source.wav");
    reader_ = std::make_shared<WavReader>( filename );
    filename = "/sdcard/log/rec-pro.wav";
    writer_rec_pro_ = std::make_shared<WavWriter>( filename, sample_rate, channels );
    filename = "/sdcard/log/rec-src.wav";
    writer_rec_src_ = std::make_shared<WavWriter>( filename, sample_rate, channels );

    processing_ = AudioProcessing::Create();
    processing_->echo_control_mobile()->Enable( true );
    processing_->echo_control_mobile()->enable_comfort_noise( false );
    processing_->echo_control_mobile()->set_routing_mode( EchoControlMobile::kLoudSpeakerphone );
    processing_->noise_suppression()->Enable( true );
    processing_->noise_suppression()->set_level( NoiseSuppression::kVeryHigh );
    processing_->high_pass_filter()->Enable( true );
    processing_->voice_detection()->Enable( true );
    processing_->gain_control()->Enable( false );
    processing_->gain_control()->set_compression_gain_db( 90 );
    processing_->gain_control()->enable_limiter( false );
    processing_->gain_control()->set_mode( GainControl::kAdaptiveDigital );
    processing_->gain_control()->set_target_level_dbfs( 9 );
    processing_->Initialize();

}

int WaveFile::GetPlayoutData( void* data, size_t need_byte_size )
{
    AudioFrame af;
    int64_t ts = TickTime::MillisecondTimestamp();


    size_t readlen = reader_->ReadSamples( need_byte_size / 2, (int16_t*)data );

    if ( readlen < need_byte_size / 2 )
    {
        reader_.reset();
        writer_rec_pro_.reset();
        writer_rec_src_.reset();
        delete processing_;
        processing_ = nullptr;
        exit( 0 );
    }

    af.UpdateFrame( 0, ts, (const int16_t*)data, 160, 16000, AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, channels_ );

    processing_->ProcessReverseStream( &af );

    return true;

}

void WaveFile::PushRecordData( const void* data, size_t byte_size )
{
    int64_t ts = TickTime::MillisecondTimestamp();
    writer_rec_src_->WriteSamples( (const int16_t*)data, byte_size / 2 );
    AudioFrame af;
    af.UpdateFrame( 0, ts, (const int16_t*)data, 160, 16000, AudioFrame::kNormalSpeech, AudioFrame::kVadUnknown, channels_ );

    processing_->set_delay_offset_ms( 0 );
    processing_->set_stream_delay_ms( 190 );
    processing_->ProcessStream( &af );
    writer_rec_pro_->WriteSamples( (const int16_t*)af.data_, byte_size / 2 );
}

