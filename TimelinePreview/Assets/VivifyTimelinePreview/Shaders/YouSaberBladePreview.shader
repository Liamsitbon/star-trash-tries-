Shader "VivifyTimelinePreview/YouSaberBladePreview"
{
    Properties { _Color ("Color", Color) = (1,0,0,1) }
    SubShader
    {
        Tags { "RenderType"="Opaque" }
        Cull Off
        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            float4 _VivifyPreviewTime;
            struct appdata { float4 vertex:POSITION; float3 normal:NORMAL; };
            struct v2f { float4 vertex:SV_POSITION; float3 normal:TEXCOORD0; float3 worldPos:TEXCOORD1; float3 localPos:TEXCOORD2; };
            fixed4 _Color;
            v2f vert(appdata v){ v2f o; o.vertex=UnityObjectToClipPos(v.vertex); o.normal=UnityObjectToWorldNormal(v.normal); o.worldPos=mul(unity_ObjectToWorld,v.vertex).xyz; o.localPos=v.vertex.xyz; return o; }
            fixed4 frag(v2f i):SV_Target {
                float3 V=normalize(_WorldSpaceCameraPos-i.worldPos);
                float fres=pow(1-saturate(abs(dot(normalize(i.normal),V))),1.5);
                float band=0.55+0.45*sin(i.localPos.y*42 + _VivifyPreviewTime.y*7 + sin(i.localPos.x*31)*1.7);
                band=pow(saturate(band),3);
                float glow=0.45 + fres*1.8 + band*1.2;
                return fixed4(_Color.rgb*glow,1);
            }
            ENDCG
        }
    }
}
