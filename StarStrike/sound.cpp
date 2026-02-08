#include "pch.h"
#include "sound.h"
#include "AudioCore.h"
#include "SoundEffect.h"
#include "MusicTrack.h"

constexpr int NUM_SAMPLES = 14;
constexpr int NUM_SONGS = 2;

static bool sm_soundOn = false;

struct SoundSample
{
  SoundEffect m_effect;
  std::wstring m_filename;
  int runtime;
  int timeleft;
};

static SoundSample sm_sampleList[NUM_SAMPLES] = {{{}, L"Audio\\launch.wav", 32, 0}, {{}, L"Audio\\crash.wav", 7, 0}, {{}, L"Audio\\dock.wav", 36, 0}, {{}, L"Audio\\gameover.wav", 24, 0}, {{}, L"Audio\\pulse.wav", 4, 0}, {{}, L"Audio\\hitem.wav", 4, 0}, {{}, L"Audio\\explode.wav", 23, 0}, {{}, L"Audio\\ecm.wav", 23, 0}, {{}, L"Audio\\missile.wav", 25, 0}, {{}, L"Audio\\hyper.wav", 37, 0}, {{}, L"Audio\\incom1.wav", 4, 0}, {{}, L"Audio\\incom2.wav", 5, 0}, {{}, L"Audio\\beep.wav", 2, 0}, {{}, L"Audio\\boop.wav", 7, 0},};

static MusicTrack sm_songs[NUM_SONGS];
static constexpr std::wstring_view sm_songFiles[NUM_SONGS] = {L"Audio\\Ambient.wav", L"Audio\\OpeningTheme.wav"};

void snd_sound_startup()
{
  IXAudio2 *sfxEngine = Audio::Core::SoundEffectEngine();
  IXAudio2 *musicEngine = Audio::Core::MusicEngine();

  if (!sfxEngine || !musicEngine)
  {
    DebugTrace("Failed to initialize audio engines");
    sm_soundOn = false;
    return;
  }

  sm_soundOn = true;

  // Load sound effects
  for (int i = 0; i < NUM_SAMPLES; i++) sm_sampleList[i].m_effect.Load(sfxEngine, sm_sampleList[i].m_filename);

  // Load music 
  for (int i = 0; i < NUM_SONGS; i++) sm_songs[i].Load(musicEngine, sm_songFiles[i]);
}

void snd_sound_shutdown(void)
{
  if (!sm_soundOn) return;

  for (int i = 0; i < NUM_SAMPLES; i++) sm_sampleList[i].m_effect.Unload();

  for (int i = 0; i < NUM_SONGS; i++) sm_songs[i].Unload();

  sm_soundOn = false;
}

void snd_play_sample(int sample_no)
{
  if (!sm_soundOn) return;

  if (sample_no < 0 || sample_no >= NUM_SAMPLES) return;

  if (sm_sampleList[sample_no].timeleft != 0) return;

  sm_sampleList[sample_no].timeleft = sm_sampleList[sample_no].runtime;
  sm_sampleList[sample_no].m_effect.Play();
}

void snd_play_music(int song_no, bool loop)
{
  if (!sm_soundOn) return;

  if (song_no < 0 || song_no >= NUM_SONGS) return;

  sm_songs[song_no].Play(loop);
}

void snd_stop_music(int song_no)
{
  if (!sm_soundOn) return;

  if (song_no < 0 || song_no >= NUM_SONGS) return;

  sm_songs[song_no].Stop();
}

void snd_set_music_volume(int song_no, float volume)
{
  if (!sm_soundOn) return;

  if (song_no < 0 || song_no >= NUM_SONGS) return;

  sm_songs[song_no].SetVolume(volume);
}

void snd_update_sound(void)
{
  if (!sm_soundOn) return;

  for (int i = 0; i < NUM_SAMPLES; i++) { if (sm_sampleList[i].timeleft > 0) sm_sampleList[i].timeleft--; }
}
