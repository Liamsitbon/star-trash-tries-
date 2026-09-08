using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Animations;
using UnityEngine.Playables;

namespace VivifyTimelinePreview
{
    // These are clocks for generated preview instances only, never for source prefabs.
    public sealed class BeatSaberPreviewObjectClock : IDisposable
    {
        private readonly GameObject owner;
        private readonly ParticleSystem[] particles;
        private readonly bool[] playOnAwake;
        private readonly bool[] autoSeeds;
        private readonly uint[] seeds;
        private readonly Animator[] animators;
        private readonly bool[] animatorEnabled;
        private readonly Animation[] legacy;
        private readonly bool[] legacyEnabled;
        private sealed class TrailClock
        {
            public TrailRenderer renderer;
            public float duration;
            public bool emitting;
            public readonly List<Vector3> positions = new List<Vector3>();
            public readonly List<float> times = new List<float>();
        }
        private readonly List<TrailClock> trails = new List<TrailClock>();
        private PlayableGraph graph;
        private float previous = -1f;

        public BeatSaberPreviewObjectClock(GameObject instance)
        {
            owner = instance;
            particles = owner.GetComponentsInChildren<ParticleSystem>(true);
            playOnAwake = new bool[particles.Length];
            autoSeeds = new bool[particles.Length];
            seeds = new uint[particles.Length];
            for (int i = 0; i < particles.Length; i++)
            {
                var main = particles[i].main;
                playOnAwake[i] = main.playOnAwake;
                autoSeeds[i] = particles[i].useAutoRandomSeed;
                seeds[i] = particles[i].randomSeed;
                main.playOnAwake = false;
                particles[i].useAutoRandomSeed = false;
                if (seeds[i] == 0) particles[i].randomSeed = (uint)(12345 + i);
                particles[i].Pause(false);
            }
            animators = owner.GetComponentsInChildren<Animator>(true);
            animatorEnabled = new bool[animators.Length];
            for (int i = 0; i < animators.Length; i++)
            {
                animatorEnabled[i] = animators[i].enabled;
                animators[i].enabled = false;
            }
            legacy = owner.GetComponentsInChildren<Animation>(true);
            legacyEnabled = new bool[legacy.Length];
            for (int i = 0; i < legacy.Length; i++)
            {
                legacyEnabled[i] = legacy[i].enabled;
                legacy[i].enabled = false;
            }
            foreach (var trail in owner.GetComponentsInChildren<TrailRenderer>(true))
            {
                trails.Add(new TrailClock { renderer = trail, duration = trail.time, emitting = trail.emitting });
                trail.emitting = false;
                trail.time = 86400f; // History is expired by song time below, including pause.
                trail.Clear();
            }
        }

        private void ResetAnimators()
        {
            if (graph.IsValid()) graph.Destroy();
            if (animators.Length == 0) return;
            graph = PlayableGraph.Create("Vivify Preview Object Clock");
            graph.SetTimeUpdateMode(DirectorUpdateMode.Manual);
            for (int i = 0; i < animators.Length; i++)
            {
                var animator = animators[i];
                if (animator == null || !animatorEnabled[i] || animator.runtimeAnimatorController == null) continue;
                animator.enabled = true;
                var playable = AnimatorControllerPlayable.Create(graph, animator.runtimeAnimatorController);
                var output = AnimationPlayableOutput.Create(graph, animator.name, animator);
                output.SetSourcePlayable(playable);
            }
            graph.Play();
            graph.Evaluate(0f);
        }

        public void Sample(float seconds)
        {
            if (owner == null || !owner.activeInHierarchy) return;
            seconds = Mathf.Max(0f, seconds);
            bool reset = previous < 0f || seconds < previous;
            float delta = reset ? seconds : seconds - previous;
            if (reset) ResetAnimators();
            if (reset || delta > 0f)
            {
                if (graph.IsValid()) graph.Evaluate(delta);
                for (int i = 0; i < particles.Length; i++)
                {
                    var ps = particles[i];
                    if (ps == null || !ps.gameObject.activeInHierarchy) continue;
                    // Each system is sampled exactly once, not once for every ancestor.
                    ps.Simulate(delta, false, reset, true);
                    ps.Pause(false);
                }
                for (int i = 0; i < legacy.Length; i++)
                {
                    if (legacy[i] == null || !legacyEnabled[i] || legacy[i].clip == null) continue;
                    AnimationState state = legacy[i][legacy[i].clip.name];
                    if (state == null) continue;
                    state.enabled = true;
                    state.weight = 1f;
                    state.time = seconds;
                    legacy[i].Sample();
                    state.enabled = false;
                }
            }
            foreach (var trail in trails)
            {
                if (trail.renderer == null || !trail.renderer.gameObject.activeInHierarchy) continue;
                if (reset || delta > 0.25f) { trail.positions.Clear(); trail.times.Clear(); }
                if (!reset && delta <= 0f) continue;
                while (trail.times.Count > 0 && seconds - trail.times[0] > trail.duration)
                {
                    trail.times.RemoveAt(0);
                    trail.positions.RemoveAt(0);
                }
                Vector3 tip = trail.renderer.transform.position;
                int count = trail.positions.Count;
                if (trail.emitting && (count == 0 || Vector3.Distance(tip, trail.positions[count - 1]) >= trail.renderer.minVertexDistance))
                {
                    trail.positions.Add(tip);
                    trail.times.Add(seconds);
                }
                trail.renderer.Clear();
                foreach (var position in trail.positions) trail.renderer.AddPosition(position);
            }
            previous = seconds;
        }

        public void Dispose()
        {
            if (graph.IsValid()) graph.Destroy();
            for (int i = 0; i < animators.Length; i++) if (animators[i] != null) animators[i].enabled = animatorEnabled[i];
            for (int i = 0; i < legacy.Length; i++) if (legacy[i] != null) legacy[i].enabled = legacyEnabled[i];
            foreach (var trail in trails) if (trail.renderer != null)
            {
                trail.renderer.time = trail.duration;
                trail.renderer.emitting = trail.emitting;
                trail.renderer.Clear();
            }
            for (int i = 0; i < particles.Length; i++)
            {
                if (particles[i] == null) continue;
                var main = particles[i].main;
                main.playOnAwake = playOnAwake[i];
                particles[i].randomSeed = seeds[i];
                particles[i].useAutoRandomSeed = autoSeeds[i];
            }
        }
    }
}
