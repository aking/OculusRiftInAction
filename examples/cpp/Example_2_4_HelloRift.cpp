#include "Common.h"
#include <OVR_CAPI_GL.h>

#if defined(OVR_OS_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif defined(OVR_OS_MAC)
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#elif defined(OVR_OS_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif

#include <GLFW/glfw3native.h>

struct EyeArgs {
  glm::mat4               projection;
  glm::mat4               viewOffset;
  gl::FrameBufferWrapper  framebuffer;
};

class HelloRift : public GlfwApp {
protected:
  ovrHmd                  hmd{ 0 };
  bool                    directMode{ false };
  bool                    debugDevice{ false };
  EyeArgs                 perEyeArgs[2];
  ovrTexture              textures[2];
  float                   eyeHeight{ OVR_DEFAULT_EYE_HEIGHT };
  float                   ipd{ OVR_DEFAULT_IPD };
  glm::mat4               player;
  GLFWwindow *            renderContext;

public:
  HelloRift() {
    ovr_Initialize();
    hmd = ovrHmd_Create(0);
    if (nullptr == hmd) {
      debugDevice = true;
      hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
    }

    ON_WINDOWS([&]{
      directMode = (0 == (ovrHmd_GetEnabledCaps(hmd) & ovrHmdCap_ExtendDesktop));
    });
    ovrHmd_ConfigureTracking(hmd,
      ovrTrackingCap_Orientation |
      ovrTrackingCap_Position, 0);
    windowPosition = glm::ivec2(hmd->WindowsPos.x, hmd->WindowsPos.y);
    windowSize = glm::uvec2(hmd->Resolution.w, hmd->Resolution.h);
    ON_LINUX([&]{
      std::swap(windowSize.x, windowSize.y);
    });
    resetPosition();
  }

  ~HelloRift() {
    ovrHmd_Destroy(hmd);
    hmd = 0;
  }

  virtual void resetPosition() {
    static const glm::vec3 EYE = glm::vec3(0, eyeHeight, ipd * 5);
    static const glm::vec3 LOOKAT = glm::vec3(0, eyeHeight, 0);
    player = glm::inverse(glm::lookAt(EYE, LOOKAT, GlUtils::UP));
    ovrHmd_RecenterPose(hmd);
  }

  static void swapBuffers(void * contextData) {
    HelloRift * pApp = reinterpret_cast<HelloRift*>(contextData);
    glfwSwapBuffers(pApp->window);
  }

  static void contextSwitch(void * contextData, ovrBool enable) {
    HelloRift * pApp = reinterpret_cast<HelloRift*>(contextData);
    if (enable) {
      glfwMakeContextCurrent(pApp->window);
    } else {
      glfwMakeContextCurrent(pApp->renderContext);
    }
  }

  virtual void finishFrame() {
    /*
     * The parent class calls glfwSwapBuffers in finishFrame,
     * but with the Oculus SDK, the SDK it responsible for buffer
     * swapping, so we have to override the method and ensure it
     * does nothing, otherwise the dual buffer swaps will
     * cause a constant flickering of the display.
     */
  }

  virtual void createRenderingTarget() {
    if (directMode) {
      // FIXME Doesn't work as expected
      //glm::uvec2 mirrorSize = windowSize;
      //mirrorSize /= 4;
      //createSecondaryScreenWindow(mirrorSize);
      createSecondaryScreenWindow(windowSize);
    } else {
      if (debugDevice) {
        windowSize /= 4;
        createSecondaryScreenWindow(windowSize);
      } else {
        glfwWindowHint(GLFW_DECORATED, 0);
        createWindow(windowSize, windowPosition);
      }
    }

    ovrHmd_SetEnabledCaps(hmd, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);
    if (directMode) {
        void * windowIdentifier = nullptr;
        ON_WINDOWS([&]{
          windowIdentifier = glfwGetWin32Window(window);
        });
        ON_MAC([&]{
          windowIdentifier = glfwGetCocoaWindow(window);
        });
        ON_LINUX([&]{
          windowIdentifier = (void*)glfwGetX11Window(window);
        });
        ovrHmd_AttachToWindow(hmd, windowIdentifier, nullptr, nullptr);
    } else {
      if (!debugDevice && glfwGetWindowAttrib(window, GLFW_DECORATED)) {
        FAIL("Unable to create undecorated window");
      }
    }
  }

  void initGl() {
    glfwWindowHint(GLFW_VISIBLE, 0);
    renderContext = glfwCreateWindow(100, 100, "shared context", nullptr, window);
    glfwMakeContextCurrent(renderContext);

    GlfwApp::initGl();

    ovrFovPort eyeFovPorts[2];
    for_each_eye([&](ovrEyeType eye){
      EyeArgs & eyeArgs = perEyeArgs[eye];
      ovrTextureHeader & eyeTextureHeader = textures[eye].Header;
      eyeFovPorts[eye] = hmd->DefaultEyeFov[eye];
      eyeTextureHeader.TextureSize = ovrHmd_GetFovTextureSize(hmd, eye, hmd->DefaultEyeFov[eye], 1.0f);
      eyeTextureHeader.RenderViewport.Size = eyeTextureHeader.TextureSize;
      eyeTextureHeader.RenderViewport.Pos.x = 0;
      eyeTextureHeader.RenderViewport.Pos.y = 0;
      eyeTextureHeader.API = ovrRenderAPI_OpenGL;

      eyeArgs.framebuffer.init(Rift::fromOvr(eyeTextureHeader.TextureSize));
      ((ovrGLTexture&)textures[eye]).OGL.TexId = eyeArgs.framebuffer.color->texture;
    });

    ovrGLConfig cfg;
    memset(&cfg, 0, sizeof(ovrGLConfig));
    cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
    cfg.OGL.Header.RTSize = Rift::toOvr(windowSize);
    cfg.OGL.Header.Multisample = 1;
    cfg.OGL.ContextData = this;
    cfg.OGL.ContextSwitch = &HelloRift::contextSwitch;
    cfg.OGL.SwapBuffers = &HelloRift::swapBuffers;

    int distortionCaps =
      ovrDistortionCap_TimeWarp |
      ovrDistortionCap_Chromatic |
//      ovrDistortionCap_Vignette |
      0;

    ON_LINUX([&]{
      distortionCaps |= ovrDistortionCap_LinuxDevFullscreen;
    });

    ovrEyeRenderDesc              eyeRenderDescs[2];
    int configResult = ovrHmd_ConfigureRendering(hmd, &cfg.Config,
      distortionCaps, eyeFovPorts, eyeRenderDescs);

    for_each_eye([&](ovrEyeType eye){
      EyeArgs & eyeArgs = perEyeArgs[eye];
      eyeArgs.projection = Rift::fromOvr(
        ovrMatrix4f_Projection(eyeFovPorts[eye], 0.01, 100, true));
      eyeArgs.viewOffset = glm::translate(glm::mat4(),
        Rift::fromOvr(eyeRenderDescs[eye].HmdToEyeViewOffset));
    });
  }

  void onKey(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
      static ovrHSWDisplayState hswDisplayState;
      ovrHmd_GetHSWDisplayState(hmd, &hswDisplayState);
      if (hswDisplayState.Displayed) {
        ovrHmd_DismissHSWDisplay(hmd);
        return;
      }
    }

    if (CameraControl::instance().onKey(key, scancode, action, mods)) {
      return;
    }

    if (GLFW_PRESS != action) {
      GlfwApp::onKey(key, scancode, action, mods);
      return;
    }

    int caps = ovrHmd_GetEnabledCaps(hmd);
    switch (key) {
    case GLFW_KEY_V:
      if (caps & ovrHmdCap_NoVSync) {
        ovrHmd_SetEnabledCaps(hmd, caps & ~ovrHmdCap_NoVSync);
      } else {
        ovrHmd_SetEnabledCaps(hmd, caps | ovrHmdCap_NoVSync);
      }
      break;

    case GLFW_KEY_P:
      if (caps & ovrHmdCap_LowPersistence) {
        ovrHmd_SetEnabledCaps(hmd, caps & ~ovrHmdCap_LowPersistence);
      }
      else {
        ovrHmd_SetEnabledCaps(hmd, caps | ovrHmdCap_LowPersistence);
      }
      break;

    case GLFW_KEY_R:
      resetPosition();
      break;

    default:
      GlfwApp::onKey(key, scancode, action, mods);
      break;
    }
  }

  virtual void update() {
    //CameraControl::instance().applyInteraction(player);
    gl::Stacks::modelview().top() = glm::inverse(player);
  }

  void draw() {
    static int frameIndex = 0;
    static ovrPosef eyePoses[2];
    static ovrVector3f eyeOffsets[2];
    memset(eyeOffsets, 0, sizeof(ovrVector3f) * 2);
    ++frameIndex;
    ovrHmd_GetEyePoses(hmd, frameIndex, eyeOffsets, eyePoses, nullptr);

    ovrHmd_BeginFrame(hmd, frameIndex);
    for( int i = 0; i < 2; ++i) {
      ovrEyeType eye = hmd->EyeRenderOrder[i];
      EyeArgs & eyeArgs = perEyeArgs[eye];
      gl::Stacks::projection().top() = eyeArgs.projection;
      gl::MatrixStack & mv = gl::Stacks::modelview();

      eyeArgs.framebuffer.activate();
      mv.withPush([&]{
        // Apply the per-eye offset & the head pose
        mv.top() = eyeArgs.viewOffset * glm::inverse(Rift::fromOvr(eyePoses[eye])) * mv.top();
        renderScene();
      });
      eyeArgs.framebuffer.deactivate();
    };
    ovrHmd_EndFrame(hmd, eyePoses, textures);
  }

  virtual void renderScene() {
    glClear(GL_DEPTH_BUFFER_BIT);
    GlUtils::renderSkybox(Resource::IMAGES_SKY_CITY_XNEG_PNG);
    GlUtils::renderFloor();

    gl::MatrixStack & mv = gl::Stacks::modelview();
    mv.with_push([&]{
      mv.translate(glm::vec3(0, 0, ipd * -5));
      GlUtils::renderManikin();
    });

    mv.with_push([&]{
      mv.translate(glm::vec3(0, eyeHeight, 0));
      mv.scale(ipd);
      GlUtils::drawColorCube();
    });
  }
};

RUN_APP(HelloRift);

