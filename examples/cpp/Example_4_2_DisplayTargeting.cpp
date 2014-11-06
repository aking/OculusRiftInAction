#include "Common.h"

class DisplayTargetingExample : public RiftGlfwApp {
public:
  DisplayTargetingExample(bool fullscreen) : RiftGlfwApp(fullscreen) {
  }

  void initGl() {
    RiftGlfwApp::initGl();
  }

  void draw() {
      {
        static ovrVector3f eyeOffsets[2];
        static ovrPosef eyePoses[2];
//        ovrHmd_GetEyePoses(hmd, getFrame(), eyeOffsets, eyePoses, nullptr);
      }

    glm::uvec2 eyeSize(hmd->Resolution.w / 2, hmd->Resolution.h);
    glm::ivec2 position = glm::ivec2(0, 0);
    glm::vec4 color = glm::vec4(1, 0, 0, 1);
    
    using namespace oglplus;
    DefaultFramebuffer().Bind(Framebuffer::Target::Draw);
    Context::Enable(Capability::ScissorTest);
    Context::Scissor(position.x, position.y, eyeSize.x, eyeSize.y);
    Context::ClearColor(color.r, color.g, color.b, color.a);
    Context::Clear().ColorBuffer();

    position = glm::ivec2(eyeSize.x, 0);
    color = glm::vec4(0, 0, 1, 1);

    Context::Scissor(position.x, position.y, eyeSize.x, eyeSize.y);
    Context::ClearColor(color.r, color.g, color.b, color.a);
    Context::Clear().ColorBuffer();
    Context::Disable(Capability::ScissorTest);
  }
};

class Windowed : public DisplayTargetingExample {
public:
  Windowed() : DisplayTargetingExample(false) {}
};

class Fullscreen : public DisplayTargetingExample {
public:
  Fullscreen() : DisplayTargetingExample(true) {}
};

RUN_OVR_APP(Windowed);
