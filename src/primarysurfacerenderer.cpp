// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Derek Lin <derekhongdalin@gmail.com>

#include "primarysurfacerenderer.h"

#include <QMutexLocker>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>

#include "karton_debug.h"

void PrimarySurfaceRenderer::attach(SpiceDisplayChannel *channel)
{
    detach();
    m_channel = channel;
    m_primaryCreateHandlerId = g_signal_connect(channel, "display-primary-create", G_CALLBACK(display_primary_create_callback), this);
    m_invalidateHandlerId = g_signal_connect(channel, "display-invalidate", G_CALLBACK(display_invalidate_callback), this);
}

void PrimarySurfaceRenderer::detach()
{
    if (m_channel) {
        if (m_primaryCreateHandlerId) {
            g_signal_handler_disconnect(m_channel, m_primaryCreateHandlerId);
        }
        if (m_invalidateHandlerId) {
            g_signal_handler_disconnect(m_channel, m_invalidateHandlerId);
        }
    }
    m_primaryCreateHandlerId = 0;
    m_invalidateHandlerId = 0;
    m_channel = nullptr;

    QMutexLocker locker(&m_frameLock);
    m_frameBuffer = nullptr;
    m_frame = QImage();
    m_frameUpdated = false;
}

void PrimarySurfaceRenderer::display_primary_create_callback(SpiceChannel *channel,
                                                             gint format,
                                                             gint width,
                                                             gint height,
                                                             gint stride,
                                                             gint shmid,
                                                             gpointer imgdata,
                                                             gpointer user_data)
{
    Q_UNUSED(channel);
    Q_UNUSED(format);
    Q_UNUSED(shmid);

    auto *self = static_cast<PrimarySurfaceRenderer *>(user_data);
    qCInfo(KARTON_DEBUG) << "SPICE: primary surface received! size:" << width << "x" << height;
    qCInfo(KARTON_DEBUG) << "SPICE: format is:" << format;

    {
        QMutexLocker locker(&self->m_frameLock);
        self->m_frameBuffer = static_cast<uchar *>(imgdata);
        self->m_imageWidth = width;
        self->m_imageHeight = height;
        // TODO: map incoming SPICE format properly instead of assuming RGB32.
        self->m_frame = QImage(self->m_frameBuffer, width, height, stride, QImage::Format_RGB32);
        self->m_frameUpdated = true;
    }

    Q_EMIT self->frameSizeChanged();
    Q_EMIT self->frameReady();
}

void PrimarySurfaceRenderer::display_invalidate_callback(SpiceDisplayChannel *channel, gint x, gint y, gint width, gint height, gpointer user_data)
{
    Q_UNUSED(channel);

    auto *self = static_cast<PrimarySurfaceRenderer *>(user_data);
    {
        QMutexLocker locker(&self->m_frameLock);
        if (self->m_frameBuffer && !self->m_frame.isNull()) {
            // Copy from spice-glib framebuffer to the QImage to render - inefficient
            const auto *source = reinterpret_cast<const uint *>(self->m_frameBuffer);
            for (int row = y; row < y + height; ++row) {
                for (int col = x; col < x + width; ++col) {
                    self->m_frame.setPixel(col, row, source[self->m_imageWidth * row + col]);
                }
            }
        }
        self->m_frameUpdated = true;
    }

    Q_EMIT self->frameReady();
}

QSGNode *PrimarySurfaceRenderer::updatePaintNode(QQuickWindow *window, QSGNode *oldNode, const QRectF &bounds)
{
    QMutexLocker locker(&m_frameLock);

    if (!m_frameUpdated || m_frame.isNull() || m_frame.width() <= 0 || m_frame.height() <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
    }

    QSGTexture *texture = window->createTextureFromImage(m_frame);
    if (texture) {
        node->setTexture(texture);
        node->setRect(bounds);
        m_frameUpdated = false;
    }

    return node;
}
