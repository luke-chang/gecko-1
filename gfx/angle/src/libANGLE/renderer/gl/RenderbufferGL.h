//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferGL.h: Defines the class interface for RenderbufferGL.

#ifndef LIBANGLE_RENDERER_GL_RENDERBUFFERGL_H_
#define LIBANGLE_RENDERER_GL_RENDERBUFFERGL_H_

#include "libANGLE/renderer/RenderbufferImpl.h"

namespace gl
{
class TextureCapsMap;
}

namespace rx
{

class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;

class RenderbufferGL : public RenderbufferImpl
{
  public:
    RenderbufferGL(const FunctionsGL *functions,
                   const WorkaroundsGL &workarounds,
                   StateManagerGL *stateManager,
                   const gl::TextureCapsMap &textureCaps);
    ~RenderbufferGL() override;

    virtual gl::Error setStorage(const gl::Context *context,
                                 GLenum internalformat,
                                 size_t width,
                                 size_t height) override;
    virtual gl::Error setStorageMultisample(const gl::Context *context,
                                            size_t samples,
                                            GLenum internalformat,
                                            size_t width,
                                            size_t height) override;
    virtual gl::Error setStorageEGLImageTarget(const gl::Context *context,
                                               egl::Image *image) override;

    GLuint getRenderbufferID() const;

  private:
    const FunctionsGL *mFunctions;
    const WorkaroundsGL &mWorkarounds;
    StateManagerGL *mStateManager;
    const gl::TextureCapsMap &mTextureCaps;

    GLuint mRenderbufferID;
};

}

#endif // LIBANGLE_RENDERER_GL_RENDERBUFFERGL_H_
