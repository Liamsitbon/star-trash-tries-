Shader "VivifyTimelinePreview/YouSaberHiltPreview"
{
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
            struct appdata { float4 vertex:POSITION; float3 normal:NORMAL; };
            struct v2f { float4 vertex:SV_POSITION; float3 normal:TEXCOORD0; float3 worldPos:TEXCOORD1; };
            v2f vert(appdata v){v2f o;o.vertex=UnityObjectToClipPos(v.vertex);o.normal=UnityObjectToWorldNormal(v.normal);o.worldPos=mul(unity_ObjectToWorld,v.vertex).xyz;return o;}
            fixed4 frag(v2f i):SV_Target { float3 V=normalize(_WorldSpaceCameraPos-i.worldPos); float d=saturate(dot(normalize(i.normal),V)); float g=0.12+d*0.7; return fixed4(g,g,g,1); }
            ENDCG
        }
    }
}
