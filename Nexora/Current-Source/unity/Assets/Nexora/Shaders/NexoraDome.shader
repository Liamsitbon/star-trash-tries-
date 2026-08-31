Shader "Nexora/VideoDome"
{
    Properties
    {
        _MainTex ("360 Video", 2D) = "black" {}
        [HideInInspector] _VideoReady ("Decoded Video Ready", Float) = 0
        _Tint ("Tint", Color) = (1,1,1,1)
        _Opacity ("Opacity", Range(0,1)) = 1
        _Brightness ("Brightness", Range(0,8)) = 1
        _Exposure ("Exposure", Range(-4,4)) = 0
        _Saturation ("Saturation", Range(0,3)) = 1
        _HueShift ("Hue Shift", Range(-1,1)) = 0
        _ProjectionMode ("Projection: 0 Mono, 1 OU, 2 SBS", Float) = 0
        _FlipX ("Flip Horizontal", Range(0,1)) = 0
        _FlipY ("Flip Vertical", Range(0,1)) = 0
        _SwapEyes ("Swap Stereo Eyes", Range(0,1)) = 0
        _DeformAmplitude ("Deform", Range(0,1)) = 0
        _DeformFrequency ("Deform Frequency", Float) = 3
        _DeformSpeed ("Deform Speed", Float) = 1
        _RippleAmount ("Ripple", Range(0,1)) = 0
        _RippleFrequency ("Ripple Frequency", Float) = 8
        _RippleSpeed ("Ripple Speed", Float) = 1
        _Twist ("Twist", Range(-3,3)) = 0
        _Pinch ("Pinch", Range(-1,1)) = 0
        _Pulse ("Pulse", Range(-0.8,2)) = 0
        _Kaleidoscope ("Kaleidoscope", Range(0,16)) = 0
        _Pixelate ("Pixelate", Range(0,1)) = 0
        _Chromatic ("Chromatic", Range(0,0.25)) = 0
        _Scanline ("Scanline", Range(0,1)) = 0
        _Vignette ("Vignette", Range(0,1)) = 0
        _Fog ("Fog", Range(0,1)) = 0
        _CameraAmount ("Quest-Safe Camera Amount", Range(0,1)) = 0
        _CameraTint ("Quest-Safe Camera Tint", Color) = (1,1,1,1)
        _CameraFisheye ("Quest-Safe Camera Fisheye", Range(-1,1)) = 0
        _CameraChromatic ("Quest-Safe Camera Chromatic", Range(0,0.2)) = 0
        _CameraGlitch ("Quest-Safe Camera Glitch", Range(0,1)) = 0
        _CameraVignette ("Quest-Safe Camera Vignette", Range(0,1)) = 0
        _CameraScanline ("Quest-Safe Camera Scanline", Range(0,1)) = 0
        _CameraPixelate ("Quest-Safe Camera Pixelate", Range(0,1)) = 0
        _CameraGrayscale ("Quest-Safe Camera Grayscale", Range(0,1)) = 0
        _CameraExposure ("Quest-Safe Camera Exposure", Range(-4,4)) = 0
        _CameraHueShift ("Quest-Safe Camera Hue Shift", Range(-1,1)) = 0
        _CameraSplit ("Quest-Safe Camera Split", Range(-0.3,0.3)) = 0
        _CameraShake ("Quest-Safe Camera Shake", Range(0,0.2)) = 0
        _CameraSwirl ("Quest-Safe Camera Swirl", Range(-2,2)) = 0
        _CameraKaleidoscope ("Quest-Safe Camera Kaleidoscope", Range(0,16)) = 0
        [Enum(UnityEngine.Rendering.BlendMode)] _SrcBlend ("Source Blend", Float) = 5
        [Enum(UnityEngine.Rendering.BlendMode)] _DstBlend ("Destination Blend", Float) = 10
    }

    SubShader
    {
        Tags { "Queue"="Background+1" "RenderType"="Transparent" "IgnoreProjector"="True" }
        Cull Off
        ZWrite Off
        ZTest LEqual
        Blend [_SrcBlend] [_DstBlend]

        Pass
        {
            CGPROGRAM
            #pragma target 3.0
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile_instancing
            #pragma multi_compile _ UNITY_SINGLE_PASS_STEREO STEREO_INSTANCING_ON STEREO_MULTIVIEW_ON
            #include "UnityCG.cginc"

            sampler2D _MainTex;
            float4 _MainTex_ST;
            float4 _Tint;
            float _VideoReady;
            float _Opacity, _Brightness, _Exposure, _Saturation, _HueShift;
            float _ProjectionMode, _FlipX, _FlipY, _SwapEyes;
            float _DeformAmplitude, _DeformFrequency, _DeformSpeed;
            float _RippleAmount, _RippleFrequency, _RippleSpeed;
            float _Twist, _Pinch, _Pulse, _Kaleidoscope, _Pixelate;
            float _Chromatic, _Scanline, _Vignette, _Fog;
            float4 _CameraTint;
            float _CameraAmount, _CameraFisheye, _CameraChromatic, _CameraGlitch;
            float _CameraVignette, _CameraScanline, _CameraPixelate, _CameraGrayscale;
            float _CameraExposure, _CameraHueShift, _CameraSplit, _CameraShake;
            float _CameraSwirl, _CameraKaleidoscope;

            struct appdata
            {
                float4 vertex : POSITION;
                float3 normal : NORMAL;
                float2 uv : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            float3 HueRotate(float3 color, float angle)
            {
                float s = sin(angle * 6.2831853);
                float c = cos(angle * 6.2831853);
                // `matrix` is reserved by some Unity shader cross-compilers.
                // The shipped bundle itself targets Android GLES3/Vulkan.
                float3x3 hueMatrix = float3x3(
                    0.299 + 0.701*c + 0.168*s, 0.587 - 0.587*c + 0.330*s, 0.114 - 0.114*c - 0.497*s,
                    0.299 - 0.299*c - 0.328*s, 0.587 + 0.413*c + 0.035*s, 0.114 - 0.114*c + 0.292*s,
                    0.299 - 0.300*c + 1.250*s, 0.587 - 0.588*c - 1.050*s, 0.114 + 0.886*c - 0.203*s);
                return mul(hueMatrix, color);
            }

            float Hash(float2 value)
            {
                return frac(sin(dot(value, float2(12.9898, 78.233))) * 43758.5453);
            }

            v2f vert(appdata v)
            {
                v2f o;
                UNITY_SETUP_INSTANCE_ID(v);
                UNITY_INITIALIZE_OUTPUT(v2f, o);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(o);

                float3 p = v.vertex.xyz;
                float3 direction = normalize(p);
                float waveA = sin((p.x + p.y * 1.31 + p.z * 0.73) * _DeformFrequency + _Time.y * _DeformSpeed);
                float waveB = sin((p.y - p.z * 1.17) * (_DeformFrequency * 1.61) - _Time.y * _DeformSpeed * 0.73);
                p += direction * (waveA + waveB * 0.5) * _DeformAmplitude * 0.08;
                p *= 1.0 + sin(_Time.y * 3.0) * _Pulse * 0.035;
                p.xz *= 1.0 + _Pinch * (p.y * p.y - 0.35) * 0.24;
                float twistAngle = p.y * _Twist + sin(_Time.y) * _Twist * 0.05;
                float sn = sin(twistAngle);
                float cs = cos(twistAngle);
                p.xz = mul(float2x2(cs, -sn, sn, cs), p.xz);
                o.vertex = UnityObjectToClipPos(float4(p, 1));
                o.uv = TRANSFORM_TEX(v.uv, _MainTex);
                return o;
            }

            fixed4 frag(v2f i) : SV_Target
            {
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(i);
                float2 uv = i.uv;
                uv.x = lerp(uv.x, 1.0 - uv.x, _FlipX);
                uv.y = lerp(uv.y, 1.0 - uv.y, _FlipY);

                // A Nexora map may intentionally disable the stock Beat Saber
                // environment. Never expose the black texture placeholder while
                // Android is preparing or has rejected the media. This calm,
                // symmetric procedural backdrop is replaced only after Unity's
                // VideoPlayer emits a real frameReady callback.
                if (_VideoReady < 0.5)
                {
                    float2 safetyUV = uv;
                    float3 safetyColor = lerp(
                        float3(0.006, 0.012, 0.035),
                        float3(0.018, 0.055, 0.095),
                        saturate(safetyUV.y));
                    float horizon = exp(-abs(safetyUV.y - 0.5) * 42.0);
                    safetyColor += float3(0.00, 0.16, 0.22) * horizon;

                    float2 gridCell = frac(safetyUV * float2(24.0, 12.0));
                    float2 gridDistance = min(gridCell, 1.0 - gridCell);
                    float grid = max(
                        1.0 - smoothstep(0.0, 0.035, gridDistance.x),
                        1.0 - smoothstep(0.0, 0.055, gridDistance.y));
                    safetyColor += float3(0.018, 0.075, 0.095) * grid;

                    float starNoise = Hash(floor(safetyUV * float2(160.0, 80.0)));
                    float stars = step(0.9965, starNoise) *
                                  (0.45 + 0.25 * sin(_Time.y * 1.7 + starNoise * 23.0));
                    safetyColor += stars * float3(0.20, 0.42, 0.48);
                    return fixed4(safetyColor, 1.0);
                }

                float eye = (float)unity_StereoEyeIndex;
                eye = lerp(eye, 1.0 - eye, step(0.5, _SwapEyes));

                if (_ProjectionMode > 1.5)
                    uv.x = uv.x * 0.5 + (eye > 0.5 ? 0.5 : 0.0);
                else if (_ProjectionMode > 0.5)
                    uv.y = uv.y * 0.5 + (eye > 0.5 ? 0.0 : 0.5);

                float cameraAmount = saturate(_CameraAmount);
                float2 cameraCentered = uv - 0.5;
                float cameraRadius = length(cameraCentered);
                float cameraAngle = atan2(cameraCentered.y, cameraCentered.x);
                cameraAngle += _CameraSwirl * cameraAmount *
                               (1.0 - saturate(cameraRadius)) *
                               (1.0 - saturate(cameraRadius));
                cameraCentered = float2(cos(cameraAngle), sin(cameraAngle)) * cameraRadius;
                cameraCentered *= 1.0 + _CameraFisheye * cameraAmount *
                                  cameraRadius * cameraRadius;
                if (_CameraKaleidoscope > 1.0 && cameraAmount > 0.001)
                {
                    float cameraSlices = max(2.0, floor(_CameraKaleidoscope));
                    float cameraSection = 6.2831853 / cameraSlices;
                    cameraAngle = abs(fmod(cameraAngle + cameraSection * 0.5,
                                           cameraSection) - cameraSection * 0.5);
                    cameraCentered = float2(cos(cameraAngle), sin(cameraAngle)) * cameraRadius;
                }
                float2 cameraUV = cameraCentered + 0.5;
                float noise = Hash(float2(floor(_Time.y * 24.0), floor(cameraUV.y * 72.0)));
                cameraUV.x += (noise - 0.5) * _CameraGlitch * cameraAmount *
                              step(0.78, noise) * 0.12;
                cameraUV += float2(sin(_Time.y * 61.0), cos(_Time.y * 47.0)) *
                            _CameraShake * cameraAmount;
                float cameraPixels = lerp(4096.0, 72.0,
                                          saturate(_CameraPixelate * cameraAmount));
                cameraUV = floor(cameraUV * cameraPixels) / cameraPixels;
                uv = lerp(uv, cameraUV, cameraAmount);

                float2 centered = uv - 0.5;
                if (_Kaleidoscope > 1.0)
                {
                    float radius = length(centered);
                    float angle = atan2(centered.y, centered.x);
                    float slices = max(2.0, floor(_Kaleidoscope));
                    float section = 6.2831853 / slices;
                    angle = abs(fmod(angle + section * 0.5, section) - section * 0.5);
                    centered = float2(cos(angle), sin(angle)) * radius;
                    uv = centered + 0.5;
                }
                uv.y += sin((uv.x + _Time.y * _RippleSpeed) * _RippleFrequency * 6.2831853) * _RippleAmount * 0.018;
                uv.x += sin((uv.y - _Time.y * _RippleSpeed * 0.7) * _RippleFrequency * 5.3) * _RippleAmount * 0.012;

                float pixels = lerp(4096.0, 64.0,
                                    saturate(max(_Pixelate,
                                                 _CameraPixelate * cameraAmount)));
                uv = floor(uv * pixels) / pixels;
                float cameraSplit = _CameraSplit * cameraAmount;
                float2 chromaDirection =
                    normalize(centered + 0.0001) *
                    (_Chromatic + _CameraChromatic * cameraAmount);
                float2 splitDirection = chromaDirection + float2(cameraSplit, 0.0);
                float r = tex2D(_MainTex, uv + splitDirection).r;
                float g = tex2D(_MainTex, uv).g;
                float b = tex2D(_MainTex, uv - splitDirection).b;
                float3 color = float3(r, g, b);
                color = HueRotate(color, _HueShift + _CameraHueShift * cameraAmount);
                float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
                color = lerp(luminance.xxx, color, _Saturation);
                color = lerp(color, luminance.xxx,
                             saturate(_CameraGrayscale * cameraAmount));
                color *= _Brightness * exp2(_Exposure + _CameraExposure * cameraAmount);
                color *= _Tint.rgb;
                color *= lerp(float3(1.0, 1.0, 1.0), _CameraTint.rgb, cameraAmount);
                float scanline = saturate(max(_Scanline,
                                              _CameraScanline * cameraAmount));
                color *= 1.0 - scanline *
                         (0.08 + 0.08 * sin(uv.y * 1800.0 + _Time.y * 25.0));
                float vignette = smoothstep(0.82, 0.18, length(centered));
                color *= lerp(1.0, vignette,
                              saturate(max(_Vignette,
                                           _CameraVignette * cameraAmount)));
                color = lerp(color, color * float3(0.55, 0.62, 0.72), _Fog * smoothstep(0.15, 0.95, uv.y));
                return fixed4(color, _Opacity * _Tint.a);
            }
            ENDCG
        }
    }
    Fallback Off
}
