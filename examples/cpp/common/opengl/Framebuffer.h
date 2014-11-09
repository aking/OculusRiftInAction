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

#pragma once

typedef std::shared_ptr<oglplus::Framebuffer> FramebufferPtr;
typedef std::shared_ptr<oglplus::Renderbuffer> RenderbufferPtr;

// A wrapper for constructing and using a
struct FramebufferWrapper {
  glm::uvec2       size;
  FramebufferPtr   fbo;
  TexturePtr       color;
  RenderbufferPtr  depth;

  FramebufferWrapper() {
  }

  FramebufferWrapper(const glm::uvec2 & size) {
    init(size);
  }

  void init(const glm::uvec2 & size) {
    using namespace oglplus;
    this->size = size;
    color = TexturePtr(new Texture());
    depth = RenderbufferPtr(new Renderbuffer());
    fbo = FramebufferPtr(new Framebuffer());
    Platform::addShutdownHook([&]{
      color.reset();
      depth.reset();
      fbo.reset();
    });


    Context::Bound(Texture::Target::_2D, *color)
      .MinFilter(TextureMinFilter::Linear)
      .MagFilter(TextureMagFilter::Linear)
      .WrapS(TextureWrap::ClampToEdge)
      .WrapT(TextureWrap::ClampToEdge)
      .Image2D(
      0, PixelDataInternalFormat::RGBA8,
      size.x, size.y,
      0, PixelDataFormat::RGB, PixelDataType::UnsignedByte, nullptr
      );

    Context::Bound(Renderbuffer::Target::Renderbuffer, *depth)
      .Storage(
          PixelDataInternalFormat::DepthComponent,
          size.x, size.y);

    Context::Bound(Framebuffer::Target::Draw, *fbo)
      .AttachTexture(FramebufferAttachment::Color, *color, 0)
      .AttachRenderbuffer(FramebufferAttachment::Depth, *depth)
      .Complete();
  }

  void Bind(oglplus::Framebuffer::Target target = oglplus::Framebuffer::Target::Draw) {
    fbo->Bind(target);
    oglplus::Context::Viewport(0, 0, size.x, size.y);
  }

  static void Unbind(oglplus::Framebuffer::Target target = oglplus::Framebuffer::Target::Draw) {
    oglplus::DefaultFramebuffer().Bind(target);
  }
};
