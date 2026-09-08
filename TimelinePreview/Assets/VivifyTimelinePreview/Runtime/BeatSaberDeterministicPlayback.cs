using System;
using System.Collections;
using UnityEngine;
using UnityEngine.Audio;
using UnityEngine.Playables;

namespace VivifyTimelinePreview
{
    [DisallowMultipleComponent]
    [DefaultExecutionOrder(-5000)]
    public class BeatSaberDeterministicPlayback : MonoBehaviour
    {
        [Header("References")]
        public PlayableDirector director;
        public BeatSaberVivifyPreviewController preview;
        public AudioSource audioSource;
        public AudioClip song;

        [Header("Always x1.0")]
        [Tooltip("V6 samples the actual song position. Loading, a scheduled start, or paused audio cannot advance the visuals.")]
        public bool lockToOneX = true;
        public bool autoPlayAfterPreBake = true;

        [Header("Audible audio sync")]
        public bool compensateForOutputLatency = true;
        [Tooltip("Additional visual delay. Output latency is an estimate, not a measurement of Bluetooth or external speakers.")]
        public float extraVisualDelaySeconds;
        [SerializeField] private float detectedOutputLatencySeconds;

        [Header("Pre-bake before playback")]
        public bool preBakeBeforePlayback = true;
        public bool warmAllLoadedShaders;
        [Range(0, 4)] public int settleFrames = 2;

        [Header("Status")]
        [SerializeField] private bool ready;
        [SerializeField] private bool playingOneX;
        [SerializeField] private double currentLockedSeconds;
        [SerializeField] private string playbackStatus = "Not prepared";

        private double dspStart;
        private double startTimelineSeconds;
        private double realtimeAnchor;
        private double audibleClockDelay;
        private double lastAudioSeconds;
        private int scheduledSample;
        private bool audioHasAdvanced;
        private bool playRequested;
        private double requestedSeconds;
        private Coroutine preparation;

        public bool Ready { get { return ready; } }
        public bool PlayingOneX { get { return playingOneX; } }
        public double CurrentLockedSeconds { get { return currentLockedSeconds; } }
        public string PlaybackStatus { get { return playbackStatus; } }

        private void Awake() { Resolve(); ConfigureClock(); }
        private void Start()
        {
            if (preparation == null && !ready) BeginPreparation(autoPlayAfterPreBake, 0d);
        }

        private void Resolve()
        {
            if (director == null) director = GetComponent<PlayableDirector>();
            if (preview == null) preview = GetComponent<BeatSaberVivifyPreviewController>();
            if (audioSource == null) audioSource = GetComponent<AudioSource>();
            if (song == null && preview != null) song = preview.song;
            if (song == null && audioSource != null) song = audioSource.clip;
            if (preview != null) preview.syncAudioSourceWhenPlaying = false;
        }

        private void ConfigureClock()
        {
            if (director != null)
            {
                director.playOnAwake = false;
                director.timeUpdateMode = DirectorUpdateMode.Manual;
                director.Pause();
            }
            if (audioSource == null) return;
            audioSource.Stop();
            audioSource.playOnAwake = false;
            audioSource.spatialBlend = 0f;
            audioSource.dopplerLevel = 0f;
            audioSource.pitch = 1f;
            audioSource.loop = false;
            audioSource.clip = song;
        }

        private void BeginPreparation(bool play, double seconds)
        {
            playRequested = play;
            requestedSeconds = Math.Max(0d, seconds);
            if (preparation != null) return;
            preparation = StartCoroutine(PreparePlayback());
        }

        private IEnumerator PreparePlayback()
        {
            ready = false;
            playingOneX = false;
            ConfigureClock();
            playbackStatus = "Preparing materials and effects";
            // Ensure the coroutine handle is assigned even if preparation is fast.
            yield return null;
            if (preBakeBeforePlayback && preview != null) preview.PreBakePreview(warmAllLoadedShaders);
            Evaluate(requestedSeconds);
            if (song != null)
            {
                playbackStatus = "Loading song; visuals held";
                if (audioSource == null)
                {
                    FailPreparation("A song is assigned but the AudioSource is missing.");
                    yield break;
                }
                if (song.loadState == AudioDataLoadState.Unloaded && !song.LoadAudioData())
                {
                    FailPreparation("Unity could not load the song audio data.");
                    yield break;
                }
                float deadline = Time.realtimeSinceStartup + 60f;
                while (song.loadState == AudioDataLoadState.Loading && Time.realtimeSinceStartup < deadline)
                    yield return null;
                if (song.loadState != AudioDataLoadState.Loaded || song.samples <= 0 || song.frequency <= 0)
                {
                    FailPreparation("Song data is not ready. Playback was not started.");
                    yield break;
                }
            }
            for (int i = 0; i < settleFrames; i++) yield return null;
            ready = true;
            preparation = null;
            playbackStatus = "Ready";
            if (playRequested) StartLockedPlayback(requestedSeconds);
        }

        private void FailPreparation(string message)
        {
            preparation = null;
            playRequested = false;
            playbackStatus = message;
            Debug.LogError("[Vivify Preview] " + message, this);
        }

        public void PreBakeAgain()
        {
            Resolve();
            if (!Application.isPlaying)
            {
                if (preview != null) preview.PreBakePreview(warmAllLoadedShaders);
                return;
            }
            bool resume = playingOneX || playRequested;
            StopClockOnly();
            BeginPreparation(resume, currentLockedSeconds);
        }

        public void StartLockedPlayback(double seconds)
        {
            Resolve();
            if (!Application.isPlaying) { Evaluate(seconds); return; }
            if (!ready || (song != null && song.loadState != AudioDataLoadState.Loaded))
            {
                BeginPreparation(true, seconds);
                return;
            }
            StopClockOnly();
            playRequested = false;
            startTimelineSeconds = Math.Max(0d, seconds);
            if (song != null) startTimelineSeconds = Math.Min(startTimelineSeconds, (song.samples - 1d) / song.frequency);
            currentLockedSeconds = startTimelineSeconds;
            detectedOutputLatencySeconds = compensateForOutputLatency ? EstimateOutputLatencySeconds() : 0f;
            audibleClockDelay = detectedOutputLatencySeconds + extraVisualDelaySeconds;
            Evaluate(startTimelineSeconds);
            if (song != null && audioSource != null)
            {
                audioSource.clip = song;
                audioSource.pitch = 1f;
                scheduledSample = Math.Min(song.samples - 1, (int)(startTimelineSeconds * song.frequency));
                audioSource.timeSamples = scheduledSample;
                lastAudioSeconds = startTimelineSeconds;
                audioHasAdvanced = false;
                dspStart = AudioSettings.dspTime + 0.15d;
                audioSource.PlayScheduled(dspStart);
                playbackStatus = "Waiting for the first audio samples";
            }
            else
            {
                realtimeAnchor = Time.realtimeSinceStartup - startTimelineSeconds;
                playbackStatus = "x1.0 (no song assigned)";
            }
            playingOneX = true;
        }

        public void PauseLockedPlayback()
        {
            // Stop also cancels an upcoming scheduled start, unlike an isPlaying-only Pause.
            playRequested = false;
            StopClockOnly();
            playbackStatus = "Paused";
        }
        public void ResumeLockedPlayback() { StartLockedPlayback(currentLockedSeconds); }
        public void RestartLockedPlayback() { StartLockedPlayback(0d); }
        public void StopLockedPlayback()
        {
            playRequested = false;
            requestedSeconds = 0d;
            StopClockOnly();
            Evaluate(0d);
            playbackStatus = "Stopped";
        }
        public void SeekLocked(double seconds)
        {
            bool resume = playingOneX || playRequested;
            StopClockOnly();
            requestedSeconds = Math.Max(0d, seconds);
            Evaluate(requestedSeconds);
            if (resume) StartLockedPlayback(requestedSeconds);
        }

        private void StopClockOnly()
        {
            if (audioSource != null) audioSource.Stop();
            playingOneX = false;
            audioHasAdvanced = false;
        }

        private double GetMasterSeconds()
        {
            if (song == null) return Math.Max(0d, Time.realtimeSinceStartup - realtimeAnchor);
            if (audioSource == null || AudioSettings.dspTime < dspStart) return currentLockedSeconds;
            int sample = audioSource.timeSamples;
            if (!audioHasAdvanced)
            {
                if (!audioSource.isPlaying || sample <= scheduledSample)
                {
                    if (AudioSettings.dspTime - dspStart > 10d)
                    {
                        StopClockOnly();
                        playbackStatus = "Audio did not start; visuals held. Check the AudioSource and output device.";
                        Debug.LogWarning("[Vivify Preview] " + playbackStatus, this);
                    }
                    return currentLockedSeconds;
                }
                audioHasAdvanced = true;
                playbackStatus = "x1.0 — song sample clock";
            }
            if (!audioSource.isPlaying)
            {
                playingOneX = false;
                playbackStatus = "Audio stopped";
                // Unity resets timeSamples when a non-looping clip finishes.
                if (lastAudioSeconds >= song.length - 0.1d) return song.length;
                return currentLockedSeconds;
            }
            lastAudioSeconds = sample / (double)song.frequency;
            // No DSP extrapolation: stalled audio cannot leave the effects running ahead.
            return Math.Max(currentLockedSeconds, Math.Max(startTimelineSeconds, lastAudioSeconds - audibleClockDelay));
        }

        private void Evaluate(double seconds)
        {
            currentLockedSeconds = Math.Max(0d, seconds);
            if (director == null)
            {
                if (preview != null) preview.EvaluateAtSeconds((float)currentLockedSeconds);
                return;
            }
            director.timeUpdateMode = DirectorUpdateMode.Manual;
            if (!director.playableGraph.IsValid()) director.RebuildGraph();
            // Disable old Timeline AudioTrack outputs only in the runtime graph. Do not
            // alter the Timeline asset or let a second AudioSource start late and echo.
            var graph = director.playableGraph;
            if (graph.IsValid()) for (int i = 0; i < graph.GetOutputCount(); i++)
            {
                var output = graph.GetOutput(i);
                if (output.GetPlayableOutputType() == typeof(AudioPlayableOutput))
                    ((AudioPlayableOutput)output).SetTarget(null);
            }
            director.time = currentLockedSeconds;
            director.Evaluate();
            if (preview != null && Math.Abs(preview.CurrentSeconds - currentLockedSeconds) > 0.0001d)
                preview.EvaluateAtSeconds((float)currentLockedSeconds);
        }

        private static float EstimateOutputLatencySeconds()
        {
            int bufferLength, numBuffers;
            AudioSettings.GetDSPBufferSize(out bufferLength, out numBuffers);
            return Mathf.Clamp(bufferLength * Math.Max(1, numBuffers) / (float)Math.Max(1, AudioSettings.outputSampleRate), 0f, 0.25f);
        }
        private void Update()
        {
            if (!lockToOneX || !playingOneX) return;
            double seconds = GetMasterSeconds();
            double duration = director != null ? director.duration : 0d;
            if (!double.IsNaN(duration) && !double.IsInfinity(duration) && duration > 0d && seconds >= duration)
            {
                seconds = duration;
                StopClockOnly();
                playbackStatus = "Finished";
            }
            Evaluate(seconds);
        }
        private void OnApplicationPause(bool paused) { if (paused) PauseLockedPlayback(); }
        private void OnDisable()
        {
            if (preparation != null) StopCoroutine(preparation);
            preparation = null;
            playRequested = false;
            StopClockOnly();
            ready = false;
        }
    }
}
