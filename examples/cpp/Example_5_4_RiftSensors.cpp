#include "Common.h"

class CubeScene_RiftSensors : public RiftGlfwApp {
  int frameIndex{ 0 };
  FramebufferWrapperPtr  eyeFramebuffers[2];
  ovrTexture eyeTextures[2];
  ovrVector3f eyeOffsets[2];
  glm::mat4 eyeProjections[2];

public:
  CubeScene_RiftSensors() {
    Stacks::modelview().top() = glm::lookAt(
      vec3(0, OVR_DEFAULT_EYE_HEIGHT, 5 * OVR_DEFAULT_IPD),
      vec3(0, OVR_DEFAULT_EYE_HEIGHT, 0),
      Vectors::UP);
    if (!ovrHmd_ConfigureTracking(hmd,
      ovrTrackingCap_Orientation |
      ovrTrackingCap_Position, 0)) {
      SAY("Warning: Unable to locate Rift sensor device.  This demo is boring now.");
    }
  }

  virtual void initGl() {
    GlfwApp::initGl();

    ovrRenderAPIConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.Header.API = ovrRenderAPI_OpenGL;
    cfg.Header.RTSize = ovr::fromGlm(getSize());
    cfg.Header.Multisample = 1;

    int distortionCaps =
      ovrDistortionCap_Chromatic |
      ovrDistortionCap_TimeWarp |
      ovrDistortionCap_Vignette;
    ovrEyeRenderDesc eyeRenderDescs[2];
    int configResult = ovrHmd_ConfigureRendering(hmd, &cfg,
      distortionCaps, hmd->DefaultEyeFov, eyeRenderDescs);

    for_each_eye([&](ovrEyeType eye){
      ovrFovPort fov = hmd->DefaultEyeFov[eye];
      ovrSizei texSize = ovrHmd_GetFovTextureSize(hmd, eye, fov, 1.0f);
      eyeFramebuffers[eye] = FramebufferWrapperPtr(new oria::FramebufferWrapper());
      eyeFramebuffers[eye]->init(ovr::toGlm(texSize));

      ovrTextureHeader & textureHeader = eyeTextures[eye].Header;
      textureHeader.API = ovrRenderAPI_OpenGL;
      textureHeader.TextureSize = texSize;
      textureHeader.RenderViewport.Size = texSize;
      textureHeader.RenderViewport.Pos.x = 0;
      textureHeader.RenderViewport.Pos.y = 0;
      ((ovrGLTexture&)eyeTextures[eye]).OGL.TexId = oglplus::GetName(eyeFramebuffers[eye]->color);

      eyeOffsets[eye] = eyeRenderDescs[eye].HmdToEyeViewOffset;

      ovrMatrix4f projection = ovrMatrix4f_Projection(fov, 0.01f, 100, true);
      eyeProjections[eye] = ovr::toGlm(projection);
    });
  }

  virtual void finishFrame() {
  }

  virtual void draw() {
    ++frameIndex;
    ovrPosef eyePoses[2];
    // Bug in SDK prevents direct mode from activating unless I call this
    ovrHmd_GetEyePoses(hmd, frameIndex, eyeOffsets, eyePoses, nullptr);

    ovrHmd_BeginFrame(hmd, frameIndex);
    MatrixStack & mv = Stacks::modelview();
    for (int i = 0; i < ovrEye_Count; ++i) {
      ovrEyeType eye = hmd->EyeRenderOrder[i];
      Stacks::projection().top() = eyeProjections[eye];

      eyeFramebuffers[eye]->fbo.Bind(oglplus::Framebuffer::Target::Draw);
      const ovrRecti & vp = eyeTextures[eye].Header.RenderViewport;
      oglplus::Context::Viewport(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);

      oglplus::Context::Clear().DepthBuffer();
      Stacks::withPush(mv, [&]{
        mv.preMultiply(glm::inverse(ovr::toGlm(eyePoses[eye])));
        oria::renderCubeScene(OVR_DEFAULT_IPD, OVR_DEFAULT_EYE_HEIGHT);
      });
    }
    oglplus::DefaultFramebuffer().Bind(oglplus::Framebuffer::Target::Draw);
    ovrHmd_EndFrame(hmd, eyePoses, eyeTextures);
  }
};

RUN_OVR_APP(CubeScene_RiftSensors);
