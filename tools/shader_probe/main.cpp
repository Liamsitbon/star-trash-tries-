// Isolated compile/link ONLY. Never injected into Beat Saber, no draw calls,
// no raymarch dispatch, no game files/config changes, no network/auth access.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <array>
#include <cctype>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

struct Context {
  EGLDisplay display=EGL_NO_DISPLAY;
  EGLSurface surface=EGL_NO_SURFACE;
  EGLContext context=EGL_NO_CONTEXT;
  ~Context() {
    if(display==EGL_NO_DISPLAY) return;
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
    if(surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
    eglTerminate(display);
  }
  void Init() {
    display=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display==EGL_NO_DISPLAY || !eglInitialize(display,nullptr,nullptr)) throw std::runtime_error("EGL unavailable");
    if(!eglBindAPI(EGL_OPENGL_ES_API)) throw std::runtime_error("EGL ES API unavailable");
    EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,
                         EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint count=0;
    if(!eglChooseConfig(display,attributes,&config,1,&count) || count!=1) throw std::runtime_error("EGL ES3 pbuffer config unavailable");
    EGLint dimensions[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
    surface=eglCreatePbufferSurface(display,config,dimensions);
    for(int minor=2;minor>=0 && context==EGL_NO_CONTEXT;--minor) {
      EGLint version[]={EGL_CONTEXT_MAJOR_VERSION_KHR,3,EGL_CONTEXT_MINOR_VERSION_KHR,minor,EGL_NONE};
      context=eglCreateContext(display,config,EGL_NO_CONTEXT,version);
      eglGetError(); // A rejected higher version is not the next attempt's error.
    }
    if(surface==EGL_NO_SURFACE || context==EGL_NO_CONTEXT || !eglMakeCurrent(display,surface,surface,context))
      throw std::runtime_error("EGL ES3 context unavailable");
  }
};
bool SafeName(std::string const& name) {
  if(name.empty() || name.size()>64 || name.find("..")!=std::string::npos) return false;
  for(unsigned char c:name) if(!std::isalnum(c) && c!='_' && c!='-' && c!='.') return false;
  return true;
}
std::string Read(std::string const& root,std::string const& name) {
  if(!SafeName(name)) throw std::runtime_error("invalid local probe filename");
  std::ifstream file(root+"/"+name,std::ios::binary|std::ios::ate);
  auto size=file.tellg();
  if(!file || size<=0 || size>1024*1024) throw std::runtime_error("missing/oversized shader source");
  std::string text(static_cast<std::size_t>(size),'\0'); file.seekg(0);
  if(!file.read(text.data(),size)) throw std::runtime_error("incomplete source read");
  return text;
}
void Log(GLuint object,bool program) {
  std::array<char,8192> log{}; GLsizei count=0;
  if(program) glGetProgramInfoLog(object,log.size(),&count,log.data());
  else glGetShaderInfoLog(object,log.size(),&count,log.data());
  for(int i=0;i<count;++i) {
    unsigned char c=log[i]; std::cout<<(c=='\n' || c=='\r' || c=='\t' ? ' ' : c>=32 ? static_cast<char>(c) : '?');
  }
}
GLuint Compile(std::string const& root,std::string const& name,GLenum stage) {
  auto text=Read(root,name); auto shader=glCreateShader(stage);
  if(!shader) {std::cout<<" unsupported-stage"; return 0;}
  auto ptr=text.data(); GLint length=static_cast<GLint>(text.size());
  glShaderSource(shader,1,&ptr,&length); glCompileShader(shader);
  GLint ok=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
  if(!ok) {std::cout<<" compile-failed "<<name<<" "; Log(shader,false); glDeleteShader(shader); return 0;}
  return shader;
}
int main(int argc,char** argv) {
  if(argc!=2) {std::cerr<<"Usage: quest_shader_probe extracted-directory\n"; return 2;}
  std::signal(SIGALRM,[](int){_exit(124);}); alarm(60); // Bound driver compile hangs in this isolated process.
  std::cout.setf(std::ios::unitbuf);
  try {
    Context egl; egl.Init();
    std::cout<<"GPU "<<glGetString(GL_RENDERER)<<" | "<<glGetString(GL_VERSION)<<" | GLSL "<<glGetString(GL_SHADING_LANGUAGE_VERSION)<<"\n";
    GLint extensionCount=0; glGetIntegerv(GL_NUM_EXTENSIONS,&extensionCount);
    for(GLint i=0;i<extensionCount;++i) {
      std::string e=reinterpret_cast<char const*>(glGetStringi(GL_EXTENSIONS,i));
      if(e.find("multiview")!=std::string::npos || e.find("geometry_shader")!=std::string::npos ||
         e.find("tessellation")!=std::string::npos || e.find("color_buffer_float")!=std::string::npos)
        std::cout<<"EXT "<<e<<"\n";
    }
    GLint range[2]={},precision=0; glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER,GL_HIGH_FLOAT,range,&precision);
    std::cout<<"fragmentHighp range="<<range[0]<<","<<range[1]<<" precision="<<precision<<"\n";
    std::ifstream manifest(std::string(argv[1])+"/manifest.txt");
    if(!manifest) throw std::runtime_error("missing manifest");
    int total=0,passed=0; std::string line;
    while(std::getline(manifest,line)) {
      if(line.size()>300 || ++total>4096) throw std::runtime_error("manifest limit exceeded");
      std::istringstream fields(line); std::string id,vert,frag,geom,extra;
      if(!(fields>>id>>vert>>frag>>geom) || fields>>extra || !SafeName(id)) throw std::runtime_error("invalid manifest line");
      std::cout<<id;
      std::array<GLuint,3> shaders{Compile(argv[1],vert,GL_VERTEX_SHADER),Compile(argv[1],frag,GL_FRAGMENT_SHADER),0};
      if(geom!="-") shaders[2]=Compile(argv[1],geom,GL_GEOMETRY_SHADER);
      if(shaders[0] && shaders[1] && (geom=="-" || shaders[2])) {
        auto program=glCreateProgram(); for(auto shader:shaders) if(shader) glAttachShader(program,shader);
        glLinkProgram(program); GLint ok=0; glGetProgramiv(program,GL_LINK_STATUS,&ok);
        if(ok) {++passed; std::cout<<" LINK_PASS";} else {std::cout<<" LINK_FAIL "; Log(program,true);}
        glDeleteProgram(program);
      }
      for(auto shader:shaders) if(shader) glDeleteShader(shader);
      std::cout<<"\n";
    }
    if(!total) throw std::runtime_error("empty manifest is not a successful shader test");
    std::cout<<"RESULT "<<passed<<"/"<<total<<" linked; no drawing, Unity bindings, Vulkan or per-eye visual proof\n";
    return passed==total ? 0 : 1;
  } catch(std::exception const& e) {std::cerr<<"PROBE_UNAVAILABLE "<<e.what()<<" eglError="<<eglGetError()<<"\n"; return 2;}
}
