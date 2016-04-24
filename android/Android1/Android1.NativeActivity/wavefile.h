#pragma once
#include <memory>
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
using namespace webrtc;
class WaveFile
{
public:
    WaveFile();
    ~WaveFile();
    void Init( size_t sample_rate, size_t channels ); // ‘§¥Ê»›¡ø
    int GetPlayoutData( void* data, size_t need_byte_size );
    void PushRecordData( const void* data, size_t byte_size );
private:
    size_t sample_rate_;
    size_t channels_;
    std::shared_ptr<WavReader> reader_;
    std::shared_ptr<WavWriter> writer_rec_pro_;
    std::shared_ptr<WavWriter> writer_rec_src_;
    AudioProcessing* processing_;
};