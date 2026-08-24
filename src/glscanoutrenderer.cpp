// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

#include "glscanoutrenderer.h"

#include <unistd.h>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include <rhi/qrhi.h> // need

#include "karton_debug.h"

GlScanoutRenderer::~GlScanoutRenderer()
{
    detach();
}

void GlScanoutRenderer::attach(SpiceDisplayChannel *channel)
{
    detach();
    m_channel = channel;
    // gl-draw means a new frame in the existing buffer, gl-scanout means a new buffer
    m_glDrawHandlerId = g_signal_connect(channel, "gl-draw", G_CALLBACK(gl_draw_callback), this);
    m_glScanoutHandlerId = g_signal_connect(channel, "notify::gl-scanout", G_CALLBACK(gl_scanout_notify_callback), this);
    // TODO: Might need to check if gl is enabled. Domains created by virt-manager are not accel3d by default.

    if (auto scanout = spice_display_channel_get_gl_scanout(channel)) {
        handleGlScanout(scanout);
    }
}

void GlScanoutRenderer::detach()
{
    if (m_channel && m_glDrawHandlerId) {
        g_signal_handler_disconnect(m_channel, m_glDrawHandlerId);
    }
    if (m_channel && m_glScanoutHandlerId) {
        g_signal_handler_disconnect(m_channel, m_glScanoutHandlerId);
    }
    m_glDrawHandlerId = 0;
    m_glScanoutHandlerId = 0;
    m_channel = nullptr;

    cleanupEGLResources();
}

void GlScanoutRenderer::gl_draw_callback(SpiceDisplayChannel *channel, guint x, guint y, guint width, guint height, gpointer user_data)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(width);
    Q_UNUSED(height);

    auto *self = static_cast<GlScanoutRenderer *>(user_data);
    Q_EMIT self->frameReady();
    spice_display_channel_gl_draw_done(channel); // releases the GL resource
}

void GlScanoutRenderer::gl_scanout_notify_callback(GObject *object, GParamSpec *pspec, gpointer user_data)
{
    Q_UNUSED(pspec);

    auto *self = static_cast<GlScanoutRenderer *>(user_data);
    if (auto scanout = spice_display_channel_get_gl_scanout(SPICE_DISPLAY_CHANNEL(object))) {
        self->handleGlScanout(scanout);
    }
}

void GlScanoutRenderer::handleGlScanout(const SpiceGlScanout *scanout)
{
    bool sizeChanged = false;

    {
        QMutexLocker locker(&m_scanoutLock);

        if (m_hasScanout && m_scanout.fd >= 0) {
            close(m_scanout.fd);
            m_scanout.fd = -1;
        }

        m_scanout = *scanout; // struct copy

        // duplicate the file descriptor if exists
        // we will be using the duplicate, and the original is freed by SPICE (gl_draw_done).
        if (scanout->fd >= 0) {
            m_scanout.fd = dup(scanout->fd);
            if (m_scanout.fd < 0) {
                qCWarning(KARTON_DEBUG) << "Failed to duplicate scanout FD";
                return;
            }
        }

        m_scanoutDirty = true;

        sizeChanged = m_imageWidth != static_cast<int>(scanout->width) || m_imageHeight != static_cast<int>(scanout->height);
        m_imageHeight = scanout->height;
        m_imageWidth = scanout->width;
        m_hasScanout = true;
    }

    if (sizeChanged) {
        Q_EMIT frameSizeChanged();
    }
    Q_EMIT frameReady();
}

void GlScanoutRenderer::createTextureFromScanout(const SpiceGlScanout *scanout)
{
    qCDebug(KARTON_DEBUG) << "=== createTextureFromScanout()";
    qCDebug(KARTON_DEBUG) << "FD:" << scanout->fd;
    qCDebug(KARTON_DEBUG) << "Size:" << scanout->width << "x" << scanout->height;
    qCDebug(KARTON_DEBUG) << "Format:" << QStringLiteral("0x%1").arg(scanout->format, 0, 16);
    qCDebug(KARTON_DEBUG) << "Stride:" << scanout->stride;

    if (scanout->fd == -1) {
        return;
    }

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qCDebug(KARTON_DEBUG) << "No current OpenGL context";
        return;
    }

    QOpenGLFunctions *gl = context->functions();
    if (!gl) {
        qCDebug(KARTON_DEBUG) << "Failed to get OpenGL functions";
        return;
    }

    // not in handleGlScanout(): no EGL display is current there, so the destroy is skipped
    cleanupEGLImage();

    // generate texture if empty
    if (m_texId == 0) {
        gl->glGenTextures(1, &m_texId);
        if (m_texId == 0) { // glGenTextures modifies m_texId
            qCDebug(KARTON_DEBUG) << "Failed to generate texture";
            return;
        }
    }

    // setup EGL display
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        qCDebug(KARTON_DEBUG) << "Failed to get EGL display";
        return;
    }

    EGLint attrs[] = {EGL_DMA_BUF_PLANE0_FD_EXT,
                      scanout->fd,
                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                      static_cast<EGLint>(scanout->stride),
                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                      0,
                      EGL_WIDTH,
                      static_cast<EGLint>(scanout->width),
                      EGL_HEIGHT,
                      static_cast<EGLint>(scanout->height),
                      EGL_LINUX_DRM_FOURCC_EXT,
                      static_cast<EGLint>(scanout->format),
                      EGL_NONE};

    // create egl image
    if (!m_eglCreateImageKHR) {
        m_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
        if (!m_eglCreateImageKHR) {
            qCDebug(KARTON_DEBUG) << "eglCreateImageKHR not available";
            return;
        }
    }
    m_eglImage = m_eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs);

    if (m_eglImage == EGL_NO_IMAGE_KHR) {
        EGLint error = eglGetError();
        qCDebug(KARTON_DEBUG) << "Failed to create EGL image, error:" << QStringLiteral("0x%1").arg(error, 0, 16);
        return;
    }

    gl->glBindTexture(GL_TEXTURE_2D, m_texId);

    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // set image texture
    if (!m_glEGLImageTargetTexture2DOES) {
        m_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
        if (!m_glEGLImageTargetTexture2DOES) {
            qCDebug(KARTON_DEBUG) << "glEGLImageTargetTexture2DOES not supported";
            return;
        }
    }
    m_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)m_eglImage);

    GLenum glError = gl->glGetError();
    if (glError != GL_NO_ERROR) {
        qCDebug(KARTON_DEBUG) << "OpenGL error after glEGLImageTargetTexture2DOES:" << QStringLiteral("0x%1").arg(glError, 0, 16);
    }

    gl->glBindTexture(GL_TEXTURE_2D, 0); // unbind texture
}

// triggered by update()
QSGNode *GlScanoutRenderer::updatePaintNode(QQuickWindow *window, QSGNode *oldNode, const QRectF &bounds)
{
    // create opengl texture from scanout data
    QSize frameSize;
    {
        QMutexLocker locker(&m_scanoutLock);
        if (m_hasScanout && (m_texId == 0 || m_scanoutDirty)) {
            createTextureFromScanout(&m_scanout);
            m_scanoutDirty = false;
        }
        frameSize = QSize(m_imageWidth, m_imageHeight);
    }

    if (!m_texId) {
        qCDebug(KARTON_DEBUG) << "No texture available yet";
        delete oldNode;
        return nullptr;
    }

    auto *textureNode = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!textureNode) {
        textureNode = new QSGSimpleTextureNode();
        textureNode->setOwnsTexture(true);
        // the node defaults to nearest, which aliases badly once the frame is scaled
        textureNode->setFiltering(QSGTexture::Linear);
    }

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qCDebug(KARTON_DEBUG) << "No current OpenGL context in updatePaintNode";
        return textureNode;
    }

    QOpenGLFunctions *gl = context->functions();
    if (gl) {
        gl->glBindTexture(GL_TEXTURE_2D, m_texId);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glBindTexture(GL_TEXTURE_2D, 0);
    }

    // loading the gl image texture onto the QSG
    QQuickWindow::CreateTextureOptions options;
    QRhi *rhi = window->rhi();
    if (!rhi) {
        qCDebug(KARTON_DEBUG) << "No RHI available";
        return textureNode;
    }

    // the GL texture id is stable, so only rebuild the wrappers when the frame size changes
    if (m_rhiTextureSize != frameSize || !textureNode->texture()) {
        QRhiTexture *rhiTexture = rhi->newTexture(QRhiTexture::RGBA8, frameSize);
        if (!rhiTexture) {
            qCDebug(KARTON_DEBUG) << "Failed to create RHI texture";
            return textureNode;
        }

        // create native texture to be contained in RHI
        QRhiTexture::NativeTexture nativeTex;
        nativeTex.object = m_texId;
        nativeTex.layout = 0;

        if (!rhiTexture->createFrom(nativeTex)) {
            qCDebug(KARTON_DEBUG) << "Failed to create RHI texture from native";
            delete rhiTexture;
            return textureNode;
        }

        QSGTexture *texture = window->createTextureFromRhiTexture(rhiTexture, options);
        if (!texture) {
            qCDebug(KARTON_DEBUG) << "Failed to create QSG texture";
            delete rhiTexture;
            return textureNode;
        }

        // setTexture frees the previous wrapper, and that frees the QRhiTexture under it
        textureNode->setTexture(texture);
        m_rhiTextureSize = frameSize;
    }

    textureNode->setRect(bounds);

    qCDebug(KARTON_DEBUG) << "GlScanoutRenderer: Successfully updated canvas.";
    qCDebug(KARTON_DEBUG) << "  Texture ID:" << m_texId;
    qCDebug(KARTON_DEBUG) << "  Size:" << m_imageWidth << "x" << m_imageHeight;

    return textureNode;
}

// cleans up scanout and egl image, textures
void GlScanoutRenderer::cleanupEGLResources()
{
    QMutexLocker locker(&m_scanoutLock);

    if (m_hasScanout && m_scanout.fd >= 0) {
        close(m_scanout.fd);
        m_scanout.fd = -1;
    }
    m_hasScanout = false;
    m_scanoutDirty = false;

    cleanupEGLImage();

    m_rhiTextureSize = QSize();

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (m_texId && context) {
        QOpenGLFunctions *gl = context->functions();
        if (gl) {
            gl->glDeleteTextures(1, &m_texId);
        }
        m_texId = 0;
    }
}

// for cleanup of duplicate
void GlScanoutRenderer::cleanupEGLImage()
{
    if (m_eglImage != EGL_NO_IMAGE_KHR) {
        EGLDisplay display = eglGetCurrentDisplay();
        if (display != EGL_NO_DISPLAY) {
            if (!m_eglDestroyImageKHR) {
                m_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
            }
            if (m_eglDestroyImageKHR) {
                m_eglDestroyImageKHR(display, m_eglImage);
            }
        }
        m_eglImage = EGL_NO_IMAGE_KHR;
    }
}
