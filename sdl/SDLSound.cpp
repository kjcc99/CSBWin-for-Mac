// SDLSound.cpp : Sound output through an SDL audio device.
// The game hands us complete in-memory .wav images (see SOUNDER in Sound.cpp).
// We copy the samples and mix any number of overlapping sounds.

#include "stdafx.h"
#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

#include <cmath>

namespace
{
struct Voice
{
   std::vector<float> samples; // mono, -1..1, at 'rate' samples per second
   double rate;
   double position; // in source samples
   float gain;
};

SDL_AudioDeviceID g_device;
int g_deviceRate;
std::vector<Voice> g_voices; // Guarded by SDL_LockAudioDevice

ui32 ReadLE32(const ui8 *p)
{
   return p[0] | (p[1] << 8) | (p[2] << 16) | (ui32(p[3]) << 24);
}

ui16 ReadLE16(const ui8 *p)
{
   return ui16(p[0] | (p[1] << 8));
}

void SDLCALL MixAudio(void * /*userdata*/, Uint8 *stream, int len)
{
   float *out = (float *)stream;
   int count = len / int(sizeof(float));
   std::fill(out, out + count, 0.0f);
   for(auto &voice : g_voices)
   {
      double step = voice.rate / g_deviceRate;
      size_t numSamples = voice.samples.size();
      for(int i = 0; i < count && voice.position < numSamples; i++)
      {
         size_t index = size_t(voice.position);
         float frac = float(voice.position - index);
         float a = voice.samples[index];
         float b = (index + 1 < numSamples) ? voice.samples[index + 1] : a;
         out[i] += (a + (b - a) * frac) * voice.gain;
         voice.position += step;
      }
   }
   for(int i = 0; i < count; i++)
      out[i] = std::clamp(out[i], -1.0f, 1.0f);
   g_voices.erase(std::remove_if(g_voices.begin(), g_voices.end(),
                                 [](const Voice &voice) { return voice.position >= voice.samples.size(); }),
                  g_voices.end());
}

// Decode an in-memory RIFF/WAVE image (8-bit unsigned or 16-bit signed PCM).
bool DecodeWave(const ui8 *wave, Voice &voice)
{
   if(memcmp(wave, "RIFF", 4) != 0 || memcmp(wave + 8, "WAVE", 4) != 0)
      return false;
   ui32 riffEnd = 8 + ReadLE32(wave + 4);
   ui32 offset = 12;
   ui16 channels = 1, bitsPerSample = 8;
   ui32 rate = 11025;
   while(offset + 8 <= riffEnd)
   {
      const ui8 *chunk = wave + offset;
      ui32 size = ReadLE32(chunk + 4);
      if(memcmp(chunk, "fmt ", 4) == 0)
      {
         channels = std::max<ui16>(ReadLE16(chunk + 10), 1);
         rate = ReadLE32(chunk + 12);
         bitsPerSample = ReadLE16(chunk + 22);
      }
      else if(memcmp(chunk, "data", 4) == 0)
      {
         const ui8 *data = chunk + 8;
         ui32 bytesPerFrame = channels * (bitsPerSample / 8);
         if(bytesPerFrame == 0 || rate == 0)
            return false;
         ui32 frames = size / bytesPerFrame;
         voice.samples.resize(frames);
         for(ui32 i = 0; i < frames; i++)
         {
            const ui8 *frame = data + i * bytesPerFrame;
            voice.samples[i] = (bitsPerSample == 16) ? i16(ReadLE16(frame)) / 32768.0f
                                                     : (frame[0] - 128) / 128.0f;
         }
         voice.rate = rate;
         return true;
      }
      offset += 8 + size + (size & 1);
   }
   return false;
}
} // namespace

void SDLSound_Initialize()
{
   SDL_AudioSpec want{}, have{};
   want.freq = 44100;
   want.format = AUDIO_F32SYS;
   want.channels = 1;
   want.samples = 512;
   want.callback = MixAudio;
   g_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
   if(g_device == 0)
   {
      fprintf(stderr, "Unable to open audio device: %s\n", SDL_GetError());
      return;
   }
   g_deviceRate = have.freq;
   SDL_PauseAudioDevice(g_device, 0);
}

void SDLSound_Shutdown()
{
   if(g_device != 0)
      SDL_CloseAudioDevice(g_device);
   g_device = 0;
}

bool UI_PlaySound(const BYTE *wave, i32 /*flags*/, i32 attenuation)
{
   // Sounds are mixed, so a new sound never has to wait for the previous one.
   if(g_device == 0 || wave == nullptr)
      return true;
   if(usingDirectX && attenuation >= 100)
      return true; // Volume off
   Voice voice;
   if(!DecodeWave(wave, voice))
      return true;
   voice.position = 0;
   // When usingDirectX is false, Sound.cpp has already scaled the samples.
   voice.gain = usingDirectX ? powf(10.0f, -attenuation / 20.0f) : 1.0f;
   SDL_LockAudioDevice(g_device);
   g_voices.push_back(std::move(voice));
   SDL_UnlockAudioDevice(g_device);
   return true;
}

void UI_StopSound()
{
   if(g_device == 0)
      return;
   SDL_LockAudioDevice(g_device);
   g_voices.clear();
   SDL_UnlockAudioDevice(g_device);
}
