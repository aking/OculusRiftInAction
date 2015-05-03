/************************************************************************************

Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
Copyright   :   Copyright Brad Davis. All Rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#include "Common.h"

#ifdef _DEBUG
#define BRAD_DEBUG 1
#endif

namespace ovr {

  /**
   * Build an OpenGL window, respecting the Rift's current display mode choice of
   * extended or direct HMD.
   */
  GLFWwindow * createRiftRenderingWindow(ovrHmd hmd, glm::uvec2 & outSize, glm::ivec2 & outPosition) {
    outSize = glm::uvec2(hmd->Resolution.w, hmd->Resolution.h);
    outSize /= 2;
    GLFWwindow * window = glfw::createSecondaryScreenWindow(outSize);
    glfwGetWindowPos(window, &outPosition.x, &outPosition.y);
    return window;
  }

}

RiftFramebufferWrapper::RiftFramebufferWrapper() {
}

RiftFramebufferWrapper::RiftFramebufferWrapper(ovrHmd hmd, const glm::uvec2 & size) {
  Init(hmd, size);
}

RiftFramebufferWrapper::~RiftFramebufferWrapper() {
  ovrHmd_DestroySwapTextureSet(hmd, textureSet);
}

void RiftFramebufferWrapper::initColor() {
  using namespace oglplus;
  if (!OVR_SUCCESS(ovrHmd_CreateSwapTextureSetGL(hmd, GL_RGBA, size.x, size.y, &textureSet))) {
    FAIL("Unable to create swap textures");
  }

  for (int i = 0; i < textureSet->TextureCount; ++i) {
    ovrGLTexture& ovrTex = (ovrGLTexture&)textureSet->Textures[i];
    glBindTexture(GL_TEXTURE_2D, ovrTex.OGL.TexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void RiftFramebufferWrapper::initDepth() {
  using namespace oglplus;
  Context::Bound(Renderbuffer::Target::Renderbuffer, depth)
    .Storage(
    PixelDataInternalFormat::DepthComponent,
    size.x, size.y);
}

void RiftFramebufferWrapper::initDone() {
  using namespace oglplus;
  Bound([&] {
    fbo.AttachRenderbuffer(Framebuffer::Target::Draw, FramebufferAttachment::Depth, depth);
    fbo.Complete(Framebuffer::Target::Draw);
  });
}

void RiftFramebufferWrapper::Init(ovrHmd hmd, const glm::uvec2 & size) {
  this->hmd = hmd;
  this->size = size;
  initColor();
  initDepth();
  initDone();
}

void RiftFramebufferWrapper::Bind(oglplus::Framebuffer::Target target) {
  using namespace oglplus;
  fbo.Bind(target);
  ovrGLTexture& tex = (ovrGLTexture&)(textureSet->Textures[textureSet->CurrentIndex]);
  GLenum glTarget = target == Framebuffer::Target::Draw ? GL_DRAW_FRAMEBUFFER : GL_READ_FRAMEBUFFER;
  glFramebufferTexture2D(glTarget, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.OGL.TexId, 0);
}

void RiftFramebufferWrapper::Unbind(oglplus::Framebuffer::Target target) {
  using namespace oglplus;
  GLenum glTarget = target == Framebuffer::Target::Draw ? GL_DRAW_FRAMEBUFFER : GL_READ_FRAMEBUFFER;
  glFramebufferTexture2D(glTarget, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
  DefaultFramebuffer().Bind(target);
}

void RiftFramebufferWrapper::Viewport() {
  oglplus::Context::Viewport(0, 0, size.x, size.y);
}

void RiftFramebufferWrapper::Increment() {
  ++textureSet->CurrentIndex;
  textureSet->CurrentIndex %= textureSet->TextureCount;
}

