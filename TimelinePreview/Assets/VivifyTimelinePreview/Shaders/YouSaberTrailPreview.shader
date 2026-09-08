Shader "VivifyTimelinePreview/YouSaberTrailPreview"
{
    Properties { _Color("Color",Color)=(1,0,0,1) _Mask("Mask",2D)="white"{} }
    SubShader
    {
        Tags { "RenderType"="Transparent" "Queue"="Transparent" }
        Blend One OneMinusSrcColor
        Cull Off
        ZWrite Off
        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            float4 _VivifyPreviewTime;
            struct appdata { float4 vertex:POSITION; float2 uv:TEXCOORD0; };
            struct v2f { float4 vertex:SV_POSITION; float2 uv:TEXCOORD0; };
            sampler2D _Mask; float4 _Mask_ST; fixed4 _Color;
            v2f vert(appdata v){v2f o;o.vertex=UnityObjectToClipPos(v.vertex);o.uv=v.uv;return o;}
            fixed4 frag(v2f i):SV_Target {
                float edge=smoothstep(0,0.08,i.uv.x)*smoothstep(1,0.92,i.uv.x);
                float tail=saturate(1-i.uv.y);
                float wave=0.65+0.35*sin(i.uv.y*33-_VivifyPreviewTime.y*8+i.uv.x*9);
                float mask=tex2D(_Mask,TRANSFORM_TEX(i.uv,_Mask)).r;
                float a=edge*tail*wave*mask;
                float3 col=lerp(float3(0.35,0.45,1),_Color.rgb,saturate(1-i.uv.y*1.7))*a*2.4;
                return fixed4(col,a*0.65);
            }
            ENDCG
        }
    }
}
