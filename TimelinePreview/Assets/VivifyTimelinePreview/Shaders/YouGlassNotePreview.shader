Shader "VivifyTimelinePreview/YouGlassNotePreview"
{
    Properties { _Color("Color",Color)=(1,0,0,0.45) _Cutout("Cutout",Range(0,1))=1 _ColorMix("Color Mix",Range(0,1))=1 _RGBSplit("RGB Split",Float)=0.01 }
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
            float4 _VivifyPreviewTime;
            struct appdata { float4 vertex:POSITION; float3 normal:NORMAL; };
            struct v2f { float4 vertex:SV_POSITION; float3 normal:TEXCOORD0; float3 worldPos:TEXCOORD1; float3 localPos:TEXCOORD2; };
            fixed4 _Color; float _Cutout; float _ColorMix; float _RGBSplit;
            v2f vert(appdata v){v2f o;o.vertex=UnityObjectToClipPos(v.vertex);o.normal=UnityObjectToWorldNormal(v.normal);o.worldPos=mul(unity_ObjectToWorld,v.vertex).xyz;o.localPos=v.vertex.xyz;return o;}
            fixed4 frag(v2f i):SV_Target { clip(_Cutout-(-i.localPos.y)-0.5); float3 V=normalize(_WorldSpaceCameraPos-i.worldPos); float fres=pow(1-saturate(abs(dot(normalize(i.normal),V))),1.8); float shimmer=0.5+0.5*sin((i.worldPos.x+i.worldPos.y+i.worldPos.z)*12+_VivifyPreviewTime.y*2.5); float3 rainbow=float3(0.5+0.5*sin(shimmer*6.28),0.5+0.5*sin(shimmer*6.28+2.1),0.5+0.5*sin(shimmer*6.28+4.2)); float3 col=lerp(rainbow,_Color.rgb,saturate(_ColorMix))* (0.35+fres*1.8); return fixed4(col,saturate(0.16+fres*0.58)); }
            ENDCG
        }
    }
}
