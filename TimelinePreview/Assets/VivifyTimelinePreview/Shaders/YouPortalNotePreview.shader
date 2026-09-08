Shader "VivifyTimelinePreview/YouPortalNotePreview"
{
    Properties { _Color("Color",Color)=(1,0,0,1) _Cutout("Cutout",Range(0,1))=1 _Void("Void",Int)=0 _PlaneDistance("Plane Distance",Float)=100 }
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
            fixed4 _Color; float _Cutout; int _Void; float _PlaneDistance;
            v2f vert(appdata v){v2f o;o.vertex=UnityObjectToClipPos(v.vertex);o.normal=UnityObjectToWorldNormal(v.normal);o.worldPos=mul(unity_ObjectToWorld,v.vertex).xyz;o.localPos=v.vertex.xyz;return o;}
            fixed4 frag(v2f i):SV_Target { clip(_Cutout-(-i.localPos.y)-0.5); if(_Void!=0) return 0; float3 V=normalize(_WorldSpaceCameraPos-i.worldPos); float fres=pow(1-saturate(abs(dot(normalize(i.normal),V))),1.4); float n=0.5+0.5*sin(i.worldPos.x*4.1+i.worldPos.y*5.3+i.worldPos.z*2.7+_VivifyPreviewTime.y*2.3); float3 rainbow=float3(0.5+0.5*sin(n*8),0.5+0.5*sin(n*8+2.09),0.5+0.5*sin(n*8+4.18)); float3 col=rainbow*_Color.rgb*(0.35+n*1.6)+_Color.rgb*fres*1.7; return fixed4(col,1); }
            ENDCG
        }
    }
}
