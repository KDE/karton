// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Derek Lin <derekhongdalin@gmail.com>

#pragma once

#include "spicedisplayrenderer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

// hardware-accel renderer
class GlScanoutRenderer : public SpiceDisplayRenderer
{
    Q_OBJECT

public:
    using SpiceDisplayRenderer::SpiceDisplayRenderer;
    ~GlScanoutRenderer() override;

    void attach(SpiceDisplayChannel *channel) override;
    void detach() override;

    QSGNode *updatePaintNode(QQuickWindow *window, QSGNode *oldNode, const QRectF &bounds) override;

    QSize frameSize() const override
    {
        return QSize(m_imageWidth, m_imageHeight);
    }

private:
    static void gl_draw_callback(SpiceDisplayChannel *channel, guint x, guint y, guint width, guint height, gpointer user_data);
    void handleGlScanout(const SpiceGlScanout *scanout);
    void createTextureFromScanout(const SpiceGlScanout *scanout);
    void cleanupEGLImage();
    void cleanupEGLResources();

    SpiceDisplayChannel *m_channel = nullptr;
    gulong m_glDrawHandlerId = 0;

    int m_imageWidth = 0;
    int m_imageHeight = 0;
    SpiceGlScanout m_scanout = {};
    bool m_hasScanout = false;
    EGLImageKHR m_eglImage = EGL_NO_IMAGE_KHR;
    PFNEGLDESTROYIMAGEKHRPROC m_eglDestroyImageKHR = nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_glEGLImageTargetTexture2DOES = nullptr;
    PFNEGLCREATEIMAGEKHRPROC m_eglCreateImageKHR = nullptr;
    GLuint m_texId = 0;
};
