Shader "VivifyTimelinePreview/YouReflectiveNotePreview"
{
    Properties { _Color("Color",Color)=(1,0,0,1) _Cutout("Cutout",Range(0,1))=1 _CutPlane("Cut Plane",Vector)=(0,0,1,0) }
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
            struct v2f { float4 vertex:SV_POSITION; float3 normal:TEXCOORD0; float3 worldPos:TEXCOORD1; float3 localPos:TEXCOORD2; };
            fixed4 _Color; float _Cutout; float4 _CutPlane;
            v2f vert(appdata v){v2f o;o.vertex=UnityObjectToClipPos(v.vertex);o.normal=UnityObjectToWorldNormal(v.normal);o.worldPos=mul(unity_ObjectToWorld,v.vertex).xyz;o.localPos=v.vertex.xyz;return o;}
            fixed4 frag(v2f i):SV_Target { clip(_Cutout-(-i.localPos.y)-0.5); float3 V=normalize(i.worldPos-_WorldSpaceCameraPos); float fres=pow(1-saturate(abs(dot(normalize(i.normal),normalize(-V)))),2); float sky=saturate(normalize(i.normal).y*0.5+0.5); float3 refl=lerp(float3(0.04,0.05,0.08),float3(0.72,0.82,1.0),sky); float3 col=lerp(refl,_Color.rgb,0.28)+_Color.rgb*fres*1.2; return fixed4(col,1); }
            ENDCG
        }
    }
}
