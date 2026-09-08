Shader "VivifyTimelinePreview/ParticlePreview"
{
    Properties
    {
        _MainTex ("Particle", 2D) = "white" {}
        _Color ("Tint", Color) = (1,1,1,1)
        _Opacity ("Opacity", Range(0,1)) = 1
        _FlareOpacity ("Flare Opacity", Range(0,1)) = 1
        _FlareBrightness ("Flare Brightness", Float) = 0.1
    }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" }
        Blend SrcAlpha One
        ZWrite Off
        Cull Off
        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            struct appdata { float4 vertex:POSITION; float2 uv:TEXCOORD0; fixed4 color:COLOR; };
            struct v2f { float4 vertex:SV_POSITION; float2 uv:TEXCOORD0; fixed4 color:COLOR; };
            sampler2D _MainTex;
            fixed4 _Color;
            float _Opacity, _FlareOpacity, _FlareBrightness;
            v2f vert(appdata v)
            {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = v.uv;
                o.color = v.color;
                return o;
            }
            fixed4 frag(v2f i):SV_Target
            {
                float radius = length(i.uv * 2 - 1);
                float coverage = pow(saturate(1 - radius), 4);
                fixed4 tex = tex2D(_MainTex, i.uv);
                float alpha = tex.a * i.color.a * _Color.a * coverage * _Opacity * _FlareOpacity;
                return fixed4(tex.rgb * i.color.rgb * _Color.rgb * max(0, _FlareBrightness), alpha);
            }
            ENDCG
        }
    }
    Fallback Off
}
