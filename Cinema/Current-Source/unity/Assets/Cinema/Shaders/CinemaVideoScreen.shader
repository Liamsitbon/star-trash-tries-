Shader "CinemaQuest/VideoScreen"
{
    Properties
    {
        _MainTex ("Video", 2D) = "black" {}
        _Brightness ("Brightness", Range(0,4)) = 1
        _Contrast ("Contrast", Range(0,5)) = 1
        _Saturation ("Saturation", Range(0,5)) = 1
        _Hue ("Hue", Range(-1,1)) = 0
        _Exposure ("Exposure", Range(0,5)) = 1
        _Gamma ("Gamma", Range(0.1,5)) = 1
        _Opacity ("Opacity", Range(0,1)) = 1
    }

    SubShader
    {
        Tags { "Queue"="Geometry-2" "RenderType"="Transparent" "IgnoreProjector"="True" }
        Cull Off
        ZWrite Off
        ZTest LEqual
        Blend SrcAlpha OneMinusSrcAlpha

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
            float _Brightness;
            float _Contrast;
            float _Saturation;
            float _Hue;
            float _Exposure;
            float _Gamma;
            float _Opacity;

            struct appdata
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
                UNITY_VERTEX_INPUT_INSTANCE_ID
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;
                UNITY_VERTEX_OUTPUT_STEREO
            };

            float3 RotateHue(float3 color, float angle)
            {
                float s = sin(angle * 6.2831853);
                float c = cos(angle * 6.2831853);
                float3x3 hueMatrix = float3x3(
                    0.299 + 0.701*c + 0.168*s, 0.587 - 0.587*c + 0.330*s, 0.114 - 0.114*c - 0.497*s,
                    0.299 - 0.299*c - 0.328*s, 0.587 + 0.413*c + 0.035*s, 0.114 - 0.114*c + 0.292*s,
                    0.299 - 0.300*c + 1.250*s, 0.587 - 0.588*c - 1.050*s, 0.114 + 0.886*c - 0.203*s);
                return mul(hueMatrix, color);
            }

            v2f vert(appdata input)
            {
                v2f output;
                UNITY_SETUP_INSTANCE_ID(input);
                UNITY_INITIALIZE_OUTPUT(v2f, output);
                UNITY_INITIALIZE_VERTEX_OUTPUT_STEREO(output);
                output.vertex = UnityObjectToClipPos(input.vertex);
                output.uv = TRANSFORM_TEX(input.uv, _MainTex);
                return output;
            }

            fixed4 frag(v2f input) : SV_Target
            {
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
                float4 sample = tex2D(_MainTex, input.uv);
                float3 color = RotateHue(sample.rgb, _Hue);
                float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
                color = lerp(luminance.xxx, color, _Saturation);
                color = (color - 0.5) * _Contrast + 0.5;
                color *= _Brightness * _Exposure;
                color = pow(max(color, 0.00001), 1.0 / max(_Gamma, 0.1));
                return fixed4(color, sample.a * _Opacity);
            }
            ENDCG
        }
    }
    Fallback Off
}
