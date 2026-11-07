#include "planet_renderer.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace sco_v3 {
namespace {
template<class T> void Drop(T*& p){if(p){p->Release();p=nullptr;}}
constexpr char shader[]=R"HLSL(
cbuffer Globe : register(b0) {
 float4 camera; float4 atmo; float4 light; float4 tuning;
 float4 baseA; float4 baseB;
};
Texture2D dayTex : register(t0); Texture2D nightTex : register(t1);
SamplerState linearTex : register(s0);
struct Pixel { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
Pixel VS(uint id:SV_VertexID){
 float2 points[6]={float2(-1,-1),float2(-1,1),float2(1,1),float2(-1,-1),float2(1,1),float2(1,-1)};
 Pixel o;float2 p=points[id];o.pos=float4(p,0,1);o.uv=p*1.18;return o;
}
float4 PS(Pixel i):SV_TARGET {
 float d=length(i.uv);float cover=saturate((1-d)/max(fwidth(d),0.0001)+0.5);
 float px=max(d-1,0)*camera.z;
 float glow=(exp(-px*px*.85)*.35+exp(-px*px*.0045)*.04+tuning.z*exp(-px*.22)*.3);
 float3 color=atmo.rgb;float alpha=glow;
 if(cover>0){
  float2 xy=i.uv*min(1,0.9995/max(d,.0001));float z=sqrt(max(0,1-dot(xy,xy)));
  float3 n=float3(xy,z);float sl=sin(camera.y),cl=cos(camera.y),so=sin(camera.x),co=cos(camera.x);
  float3 world=float3(-so,co,0)*n.x+float3(-sl*co,-sl*so,cl)*n.y+float3(cl*co,cl*so,sl)*n.z;
  float2 uv=float2(frac(atan2(world.y,world.x)/6.2831853+.5),clamp(.5-asin(clamp(world.z,-1,1))/3.14159265,.0001,.9999));
  float3 albedo=dayTex.Sample(linearTex,uv).rgb;
  if(tuning.w<.5){
   float grain=sin(world.x*17+sin(world.y*11))*.22+cos(world.z*23-world.y*5)*.16+.5;
   if(camera.w==1)grain=.5+.28*sin(world.z*35+sin(world.x*8));
   albedo=lerp(baseA.rgb,baseB.rgb,saturate(grain));
  }
  float ndl=dot(n,normalize(light.xyz));float diffuse=smoothstep(-.28,.72,ndl);
  float mu=saturate(n.z),fres=pow(1-mu,2.5);
  color=albedo*(tuning.x+(1-tuning.x)*diffuse)*lerp(.46,1,pow(mu,.48));
  if(camera.w==1)color=albedo*(.55+.45*diffuse);
  if(camera.w==4)color=albedo*(1.05+.22*diffuse)+atmo.rgb*fres*.4;
  color+=atmo.rgb*fres*tuning.y*(.4+.9*(1-diffuse));
  color+=nightTex.Sample(linearTex,uv).rgb*(1-smoothstep(-.18,.22,ndl))*light.w;
  color+=atmo.rgb*pow(1-mu,4)*tuning.z*(.35+.65*smoothstep(-.35,.5,ndl));
  alpha=cover+glow*(1-cover);
  color=(color*cover+atmo.rgb*glow*(1-cover))/max(alpha,.0001);
 }
 return float4(color,saturate(alpha));
}
)HLSL";
struct Constants{float camera[4],atmo[4],light[4],tuning[4],a[4],b[4];};
struct Sprite{ID3D11ShaderResourceView*day=nullptr,*night=nullptr,*view=nullptr;ID3D11Texture2D*target=nullptr;ID3D11RenderTargetView*rt=nullptr;int size=0;bool loaded=false;};
ID3D11Device*dev=nullptr;ID3D11DeviceContext*ctx=nullptr;
ID3D11VertexShader*vs=nullptr;ID3D11PixelShader*ps=nullptr;ID3D11Buffer*cb=nullptr;
ID3D11SamplerState*sampler=nullptr;ID3D11RasterizerState*raster=nullptr;ID3D11ShaderResourceView*black=nullptr;
Sprite sprites[17];std::wstring folder;bool comOwned=false;
const wchar_t*names[]={L"hurston",L"ariel",L"aberdeen",L"magda",L"ita",L"crusader",L"cellin",L"daymar",L"yela",L"arccorp_v3",L"lyria",L"wala",L"microtech",L"calliope",L"clio",L"euterpe",L"stanton_star"};
bool Compile(const char* entry,const char* profile,ID3DBlob** result,std::string&error){
 ID3DBlob*log=nullptr;HRESULT hr=D3DCompile(shader,sizeof(shader)-1,"v3-planet",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,result,&log);
 if(FAILED(hr))error=log?std::string(static_cast<const char*>(log->GetBufferPointer()),log->GetBufferSize()):"D3DCompile failed";
 Drop(log);return SUCCEEDED(hr);
}
ID3D11ShaderResourceView* Load(const std::wstring&path){
 IWICImagingFactory*f=nullptr;IWICBitmapDecoder*d=nullptr;IWICBitmapFrameDecode*frame=nullptr;IWICFormatConverter*c=nullptr;
 UINT w=0,h=0;std::vector<unsigned char>pixels;ID3D11ShaderResourceView*out=nullptr;
 bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f)))&&
 SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&d))&&SUCCEEDED(d->GetFrame(0,&frame))&&
 SUCCEEDED(f->CreateFormatConverter(&c))&&SUCCEEDED(c->Initialize(frame,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))&&
 SUCCEEDED(c->GetSize(&w,&h))&&w>0&&h>0&&w<=4096&&h<=2048;
 if(ok){pixels.resize(size_t(w)*h*4);ok=SUCCEEDED(c->CopyPixels(nullptr,w*4,UINT(pixels.size()),pixels.data()));}
 if(ok){D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{pixels.data(),w*4,0};ID3D11Texture2D*tex=nullptr;if(SUCCEEDED(dev->CreateTexture2D(&desc,&data,&tex)))dev->CreateShaderResourceView(tex,nullptr,&out);Drop(tex);}
 Drop(c);Drop(frame);Drop(d);Drop(f);return out;
}
void Rgb(float*out,unsigned value){out[0]=float(value>>16&255)/255;out[1]=float(value>>8&255)/255;out[2]=float(value&255)/255;out[3]=1;}
}
bool ValidatePlanetShaders(std::string&error){ID3DBlob*a=nullptr,*b=nullptr;const bool ok=Compile("VS","vs_4_0",&a,error)&&Compile("PS","ps_4_0",&b,error);Drop(a);Drop(b);return ok;}
bool InitPlanets(ID3D11Device*d,ID3D11DeviceContext*c,const wchar_t*path){
 StopPlanets();dev=d;ctx=c;folder=path;comOwned=SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));
 std::string error;ID3DBlob*a=nullptr,*b=nullptr;
 bool ok=Compile("VS","vs_4_0",&a,error)&&Compile("PS","ps_4_0",&b,error);
 if(ok)ok=SUCCEEDED(dev->CreateVertexShader(a->GetBufferPointer(),a->GetBufferSize(),nullptr,&vs))&&SUCCEEDED(dev->CreatePixelShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,&ps));
 Drop(a);Drop(b);if(!ok){StopPlanets();return false;}
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=D3D11_TEXTURE_ADDRESS_WRAP;sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
 D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
 ok=SUCCEEDED(dev->CreateBuffer(&bd,nullptr,&cb))&&SUCCEEDED(dev->CreateSamplerState(&sd,&sampler))&&SUCCEEDED(dev->CreateRasterizerState(&rd,&raster));
 unsigned pixel=0;D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA data{&pixel,4,0};ID3D11Texture2D*tex=nullptr;
 if(SUCCEEDED(dev->CreateTexture2D(&td,&data,&tex)))ok=ok&&SUCCEEDED(dev->CreateShaderResourceView(tex,nullptr,&black));else ok=false;Drop(tex);
 if(!ok)StopPlanets();return ok;
}
void StopPlanets(){for(auto&s:sprites){Drop(s.day);Drop(s.night);Drop(s.view);Drop(s.rt);Drop(s.target);s={};}Drop(black);Drop(raster);Drop(sampler);Drop(cb);Drop(vs);Drop(ps);dev=nullptr;ctx=nullptr;if(comOwned){CoUninitialize();comOwned=false;}}
bool Planet(ImDrawList*draw,ImVec2 center,float radius,double lat,double lon,int body){
 if(!dev||!ctx||body<0||body>16||radius<=1||!std::isfinite(radius))return false;
 auto&s=sprites[body];if(!s.loaded){s.loaded=true;s.day=Load(folder+L"\\"+names[body]+L".jpg");if(body==9)s.night=Load(folder+L"\\arccorp_v3_night.jpg");}
 const int size=(std::clamp)(((int(std::ceil(radius*2.36f))+63)/64)*64,128,1536);
 if(s.size!=size){Drop(s.view);Drop(s.rt);Drop(s.target);s.size=0;
 D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=size;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
 if(FAILED(dev->CreateTexture2D(&td,nullptr,&s.target))||FAILED(dev->CreateRenderTargetView(s.target,nullptr,&s.rt))||FAILED(dev->CreateShaderResourceView(s.target,nullptr,&s.view)))return false;s.size=size;}
 Constants values{};values.camera[0]=float(lon);values.camera[1]=float(lat);values.camera[2]=radius;values.camera[3]=body==5?1.f:body==16?4.f:0.f;
 Rgb(values.atmo,body==5?0xF0E4EF:body==0?0xD9C79A:0xCFE6FF);Rgb(values.a,body==5?0xCEA79B:body==12?0x799DA6:0xA99985);Rgb(values.b,body==5?0xEEE0C5:body==12?0xF6F2E8:0xDBCEB1);
 values.light[0]=body==9?-.96f:-.45f;values.light[1]=body==9?.22f:.42f;values.light[2]=body==9?.16f:.85f;values.light[3]=s.night?1.15f:0;
 values.tuning[0]=s.night?.05f:.35f;values.tuning[1]=.24f;values.tuning[2]=body==9?.8f:.15f;values.tuning[3]=s.day?1.f:0.f;
 ID3D11RenderTargetView*oldRT=nullptr;ID3D11DepthStencilView*oldDS=nullptr;ctx->OMGetRenderTargets(1,&oldRT,&oldDS);UINT count=1;D3D11_VIEWPORT oldVP{};ctx->RSGetViewports(&count,&oldVP);
 const float clear[4]={};ctx->OMSetRenderTargets(1,&s.rt,nullptr);ctx->ClearRenderTargetView(s.rt,clear);D3D11_VIEWPORT vp{0,0,float(size),float(size),0,1};ctx->RSSetViewports(1,&vp);ctx->RSSetState(raster);
 ctx->OMSetBlendState(nullptr,nullptr,0xffffffff);ctx->OMSetDepthStencilState(nullptr,0);ctx->IASetInputLayout(nullptr);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 ctx->VSSetShader(vs,nullptr,0);ctx->PSSetShader(ps,nullptr,0);ctx->UpdateSubresource(cb,0,nullptr,&values,0,0);ctx->PSSetConstantBuffers(0,1,&cb);ctx->PSSetSamplers(0,1,&sampler);
 ID3D11ShaderResourceView*textures[]={s.day?s.day:black,s.night?s.night:black};ctx->PSSetShaderResources(0,2,textures);ctx->Draw(6,0);
 ID3D11ShaderResourceView*empty[2]={};ctx->PSSetShaderResources(0,2,empty);ctx->OMSetRenderTargets(1,&oldRT,oldDS);if(count)ctx->RSSetViewports(1,&oldVP);Drop(oldRT);Drop(oldDS);
 const float r=radius*1.18f;draw->AddImage(ImTextureID(reinterpret_cast<uintptr_t>(s.view)),{center.x-r,center.y-r},{center.x+r,center.y+r},{0,0},{1,1},ImGui::GetColorU32(IM_COL32_WHITE));return true;
}
}
