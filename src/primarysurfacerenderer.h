// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Derek Lin <derekhongdalin@gmail.com>

#pragma once

#include "spicedisplayrenderer.h"

#include <QImage>
#include <QMutex>

// CPU-based VM display renderer
class PrimarySurfaceRenderer : public SpiceDisplayRenderer
{
    Q_OBJECT

public:
    using SpiceDisplayRenderer::SpiceDisplayRenderer;

    void attach(SpiceDisplayChannel *channel) override;
    void detach() override;

    QSGNode *updatePaintNode(QQuickWindow *window, QSGNode *oldNode, const QRectF &bounds) override;

    QSize frameSize() const override
    {
        return QSize(m_imageWidth, m_imageHeight);
    }

private:
    static void
    display_primary_create_callback(SpiceChannel *channel, gint format, gint width, gint height, gint stride, gint shmid, gpointer imgdata, gpointer user_data);
    static void display_invalidate_callback(SpiceDisplayChannel *channel, gint x, gint y, gint width, gint height, gpointer user_data);

    SpiceDisplayChannel *m_channel = nullptr;
    gulong m_primaryCreateHandlerId = 0;
    gulong m_invalidateHandlerId = 0;

    int m_imageWidth = 0;
    int m_imageHeight = 0;
    uchar *m_frameBuffer = nullptr;
    QImage m_frame;
    QMutex m_frameLock;
    bool m_frameUpdated = false;
};
