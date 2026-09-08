Shader "VivifyTimelinePreview/GenericTransparentPreview"
{
    Properties
    {
        _MainTex ("Texture", 2D) = "white" {}
        _Color ("Color", Color) = (1,1,1,0.65)
        _EmissionColor ("Emission", Color) = (0,0,0,0)
    }
    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" }
        Blend SrcAlpha OneMinusSrcAlpha
        ZWrite Off
        Cull Off
        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            struct appdata { float4 vertex:POSITION; float2 uv:TEXCOORD0; };
            struct v2f { float4 vertex:SV_POSITION; float2 uv:TEXCOORD0; };
            sampler2D _MainTex; float4 _MainTex_ST; fixed4 _Color; fixed4 _EmissionColor;
            v2f vert(appdata v){ v2f o; o.vertex=UnityObjectToClipPos(v.vertex); o.uv=TRANSFORM_TEX(v.uv,_MainTex); return o; }
            fixed4 frag(v2f i):SV_Target { fixed4 t=tex2D(_MainTex,i.uv); return fixed4(t.rgb*_Color.rgb + _EmissionColor.rgb, t.a*_Color.a); }
            ENDCG
        }
    }
}
