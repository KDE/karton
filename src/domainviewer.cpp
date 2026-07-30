// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

#include "domainviewer.h"

#include <spice-client.h>

#include <QGuiApplication>
#include <QQuickWindow>
#include <QString>
#include <QUrl>

#include "domain.h"
#include "glib.h"
#include "glscanoutrenderer.h"
#include "karton_debug.h"
#include "primarysurfacerenderer.h"

#include "evdev_to_xtkbd_map.h"

DomainViewer::DomainViewer(QQuickItem *parent)
    : QQuickItem(parent)
    , m_commandRunner(new CommandRunner(this))
    , m_domain(nullptr)
    , m_spiceUri(QString())
    , m_host(QStringLiteral("localhost"))
    , m_port(5900)
    , m_password(QString())
    , m_connected(false)
    , m_session(nullptr)
    , m_display_channel(nullptr)
    , m_inputs_channel(nullptr)
    , m_playback_channel(nullptr)
    , m_current_button_mask(0)
    , m_audio(nullptr)
    , m_audioSink(nullptr)
    , m_audioDevice(nullptr)
    , m_audioFormat()
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(ItemIsFocusScope, true);

    connect(m_commandRunner, &CommandRunner::commandFinished, this, &DomainViewer::handleHostPort);
}

DomainViewer::~DomainViewer()
{
    disconnectFromSpice();
}

void DomainViewer::setDomain(Domain *domain)
{
    if (m_domain == domain) {
        return;
    }
    if (m_domain) {
        disconnectFromSpice();
    }

    m_domain = domain;
    Q_EMIT domainChanged();

    if (isComponentComplete() && m_domain) {
        if (m_domain) {
            setupSpiceSession();
        } else {
            qCDebug(KARTON_DEBUG) << "setDomain(): null domain assigned";
        }
    }
}

void DomainViewer::componentComplete()
{
    QQuickItem::componentComplete();
    if (m_domain) {
        setupSpiceSession();
    }
}

void DomainViewer::handleHostPort(int exitCode, const QString &output)
{
    if (exitCode != 0 || output.isEmpty()) {
        qCCritical(KARTON_DEBUG) << "handleHostPort: virsh domdisplay call failed";
        return;
    }

    QString trimmedOutput = output.trimmed();
    QUrl url(output.trimmed());

    if (!url.isValid()) {
        qCCritical(KARTON_DEBUG) << "Invalid SPICE URI given to virsh domdisplay";
        return;
    }
    m_host = url.host();
    m_port = url.port(); // defaults to -1 if not found
    m_spiceUri = url.toString();

    if (m_spiceUri.startsWith(QStringLiteral("spice+unix:///tmp/spice"))) {
        qCInfo(KARTON_DEBUG) << "Detected UNIX socket. Using GL scanout rendering.";
        m_renderMode = RenderMode::GlScanout;
    } else {
        qCInfo(KARTON_DEBUG) << "Detected network TCP socket. Using primary surface rendering.";
        m_renderMode = RenderMode::PrimarySurface;
    }

    if (!connectToSpice()) {
        qCCritical(KARTON_DEBUG) << "Failed to connect to SPICE";
    }
}

bool DomainViewer::setupSpiceSession()
{
    // TODO: replace virsh CLI, use libvirt API: https://libvirt.org/html/libvirt-libvirt-domain.html#VIR_MIGRATE_PARAM_GRAPHICS_URI
    // once finished, handleHostPort() will set the host and port provided by the output.
    bool commandStarted = m_commandRunner->runCommand(QStringLiteral("virsh domdisplay %1").arg(m_domain->config()->name()));
    return commandStarted;
}

bool DomainViewer::connectToSpice()
{
    qCInfo(KARTON_DEBUG) << "Connecting to SPICE...";
    qCInfo(KARTON_DEBUG) << "   SPICE URI:" << m_spiceUri;
    qCInfo(KARTON_DEBUG) << "   host: " << m_host;
    qCInfo(KARTON_DEBUG) << "   post" << m_port;

    if (!m_domain) {
        qCCritical(KARTON_DEBUG) << "connectToSpice() called but domain is null!";
        return false;
    }

    disconnectFromSpice();
    m_session = spice_session_new();

    qCInfo(KARTON_DEBUG) << "Connecting to URI! -" << m_spiceUri;
    g_object_set(m_session, "uri", m_spiceUri.toUtf8().constData(), NULL);

    g_signal_connect(m_session, "channel-new", G_CALLBACK(DomainViewer::channel_new_callback), this);

    if (!spice_session_connect(m_session)) {
        g_object_unref(m_session);
        m_session = nullptr;
        m_audio = nullptr;
        return false;
    }
    qCInfo(KARTON_DEBUG) << "Established connection! connected to " << domain()->config()->name();
    m_connected = true;

    return true;
}

void DomainViewer::disconnectFromSpice()
{
    stopAudio();

    if (m_renderer) {
        m_renderer->detach();
        m_renderer.reset();
    }

    if (m_session) {
        spice_session_disconnect(m_session);

        g_object_unref(m_session);
        m_session = nullptr;
        m_display_channel = nullptr;
        m_audio = nullptr;
        m_playback_channel = nullptr;
        m_connected = false;
    }
}

void DomainViewer::channel_new_callback(SpiceSession *session, SpiceChannel *channel, gpointer user_data)
{
    Q_UNUSED(session);

    DomainViewer *item = static_cast<DomainViewer *>(user_data);

    item->checkChannelStatus(); // uncomment for channel debug msgs
    if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
        qCInfo(KARTON_DEBUG) << "SPICE display connected";
        item->attachDisplayChannel(channel);
    } else if (SPICE_IS_INPUTS_CHANNEL(channel)) {
        qCInfo(KARTON_DEBUG) << "SPICE: Inputs connected";
        spice_channel_connect(channel);
        item->m_inputs_channel = SPICE_INPUTS_CHANNEL(channel);
    } else if (SPICE_IS_PLAYBACK_CHANNEL(channel)) {
        qCInfo(KARTON_DEBUG) << "SPICE: Audio playback connected";
        spice_channel_connect(channel);
        item->m_playback_channel = SPICE_PLAYBACK_CHANNEL(channel);

        g_signal_connect(channel, "playback-start", G_CALLBACK(playback_start_callback), item);
        g_signal_connect(channel, "playback-data", G_CALLBACK(playback_data_callback), item);
        g_signal_connect(channel, "playback-stop", G_CALLBACK(playback_stop_callback), item);
    } else {
        qCWarning(KARTON_DEBUG) << "Unrecognised SPICE channel type";
    }
}

// ========================== Display rendering  ========================

void DomainViewer::attachDisplayChannel(SpiceChannel *channel)
{
    spice_channel_connect(channel);
    m_display_channel = channel;

    switch (m_renderMode) {
    case RenderMode::GlScanout:
        m_renderer = std::make_unique<GlScanoutRenderer>();
        break;
    case RenderMode::PrimarySurface:
        m_renderer = std::make_unique<PrimarySurfaceRenderer>();
        break;
    }

    connect(m_renderer.get(), &SpiceDisplayRenderer::frameReady, this, &QQuickItem::update);
    connect(m_renderer.get(), &SpiceDisplayRenderer::frameSizeChanged, this, &DomainViewer::handleRendererFrameSizeChanged);

    m_renderer->attach(SPICE_DISPLAY_CHANNEL(channel));
}

void DomainViewer::handleRendererFrameSizeChanged()
{
    if (!m_renderer) {
        return;
    }
    const QSize size = m_renderer->frameSize();
    setImplicitWidth(size.width());
    setImplicitHeight(size.height());
}

// triggered by update()
QSGNode *DomainViewer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_renderer) {
        delete oldNode;
        return nullptr;
    }
    return m_renderer->updatePaintNode(window(), oldNode, boundingRect());
}

void DomainViewer::saveFrameToDomain()
{
    if (!window()) {
        qCDebug(KARTON_DEBUG) << "saveFrameToDomain: No window available";
        return;
    }

    if (!isVisible() || width() <= 0 || height() <= 0) {
        qCDebug(KARTON_DEBUG) << "saveFrameToDomain: Item not ready for grabbing";
        return;
    }

    // returns the image of the full window
    QImage fullImage = window()->grabWindow();

    if (!fullImage.isNull()) {
        // crop it to the actual QQI
        QRectF itemBounds = mapRectToScene(boundingRect());
        QRect cropRect = itemBounds.toRect().intersected(fullImage.rect());
        QImage finalImage = cropRect.isEmpty() ? fullImage : fullImage.copy(cropRect);

        qCDebug(KARTON_DEBUG) << "Sending QImage frame to save... - " << finalImage.size() << ", " << m_domain->config()->name();
        m_domain->savePreviewFrame(finalImage.convertToFormat(QImage::Format_RGB32));
    } else {
        qCDebug(KARTON_DEBUG) << "saveFrameToDomain: grabWindow returned null image";
    }
}
// ========================== Audio callbacks =====================

void DomainViewer::playback_start_callback(SpicePlaybackChannel *channel, gint format, gint channels, gint rate, gpointer user_data)
{
    Q_UNUSED(channel);

    DomainViewer *item = static_cast<DomainViewer *>(user_data);
    qCInfo(KARTON_DEBUG) << "Audio playback starting - Format:" << format << "Channels:" << channels << "Rate:" << rate;

    item->stopAudio();

    item->m_audioFormat.setSampleRate(rate);
    item->m_audioFormat.setChannelCount(channels);
    item->m_audioFormat.setSampleFormat(QAudioFormat::Int16);

    item->m_audioSink = new QAudioSink(item->m_audioFormat, item);
    item->m_audioDevice = item->m_audioSink->start();

    qCInfo(KARTON_DEBUG) << "Audio output started successfully";
}

void DomainViewer::playback_data_callback(SpicePlaybackChannel *channel, gpointer data, gint size, gpointer user_data)
{
    Q_UNUSED(channel);

    DomainViewer *item = static_cast<DomainViewer *>(user_data);
    if (item->m_audioDevice) {
        item->m_audioDevice->write(static_cast<const char *>(data), size);
    }
}

void DomainViewer::playback_stop_callback(SpicePlaybackChannel *channel, gpointer user_data)
{
    Q_UNUSED(channel);

    DomainViewer *item = static_cast<DomainViewer *>(user_data);
    qCInfo(KARTON_DEBUG) << "Audio playback stopping";
    item->stopAudio();
}

void DomainViewer::stopAudio()
{
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }

    m_audioDevice = nullptr;
}

// ========================== Input handling =============================

// maps qt provided scancode to pcxt
uint8_t DomainViewer::evdevToPcXt(uint32_t evdev_scancode)
{
    if (evdev_scancode >= CODE_MAP_LINUX_TO_XTKBD_LEN) {
        qCWarning(KARTON_DEBUG) << "Scancode out of range:" << evdev_scancode;
        return 0;
    }

    uint8_t result = code_map_linux_to_xtkbd[evdev_scancode] & 0xFF;
    if (result == 0) {
        qCWarning(KARTON_DEBUG) << "Unknown evdev scancode:" << evdev_scancode;
        return 0;
    }

    qCDebug(KARTON_DEBUG) << "Mapped evdev" << evdev_scancode << "to PC XT" << QString::number(result, 16);
    return result;
}

void DomainViewer::keyPressEvent(QKeyEvent *event)
{
    event->accept();
    quint32 evdev_scancode;
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) { // check if X11
        evdev_scancode = event->nativeScanCode();
    } else { // wayland probably
        evdev_scancode = event->nativeScanCode() - x11_wayland_evdev_offset;
    }

    uint8_t pcxt_scancode = DomainViewer::evdevToPcXt(evdev_scancode); // SPICE accepts PC XT: see inputs channel docs
    qCDebug(KARTON_DEBUG) << "key press: " << event->text() << evdev_scancode << pcxt_scancode;

    if (m_inputs_channel && m_connected && pcxt_scancode != 0) {
        spice_inputs_channel_key_press(m_inputs_channel, pcxt_scancode);
    }
}

void DomainViewer::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();

    quint32 evdev_scancode;
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        evdev_scancode = event->nativeScanCode();
    } else {
        evdev_scancode = event->nativeScanCode() - x11_wayland_evdev_offset;
    }

    uint8_t pcxt_scancode = DomainViewer::evdevToPcXt(evdev_scancode);

    if (m_inputs_channel && m_connected && pcxt_scancode != 0) {
        spice_inputs_channel_key_release(m_inputs_channel, pcxt_scancode);
    }
}

void DomainViewer::wheelEvent(QWheelEvent *event)
{
    event->accept();

    if (!m_inputs_channel || !m_connected) {
        return;
    }
    qreal x = event->position().x();
    qreal y = event->position().y();

    const QSize frameSize = m_renderer ? m_renderer->frameSize() : QSize();
    if (frameSize.width() > 0 && frameSize.height() > 0 && width() > 0 && height() > 0) {
        x = (x * frameSize.width()) / width();
        y = (y * frameSize.height()) / height();
    }

    spice_inputs_channel_position(m_inputs_channel, x, y, 0, 0);

    QPoint angleDelta = event->angleDelta();
    // for more info on constants see: (Inputs channel definition) https://www.spice-space.org/spice-protocol.html
    if (angleDelta.y() > 0) { // scroll up
        spice_inputs_channel_button_press(m_inputs_channel, SPICE_MOUSE_BUTTON_UP, 0);
        spice_inputs_channel_button_release(m_inputs_channel, SPICE_MOUSE_BUTTON_UP, 0);
    } else if (angleDelta.y() < 0) { // scroll down
        spice_inputs_channel_button_press(m_inputs_channel, SPICE_MOUSE_BUTTON_DOWN, 0);
        spice_inputs_channel_button_release(m_inputs_channel, SPICE_MOUSE_BUTTON_DOWN, 0);
    }

    if (angleDelta.x() > 0) { // scroll right
        // TODO: find horizontal in spice protocol
    } else if (angleDelta.x() < 0) { // scroll left
    }

    qCDebug(KARTON_DEBUG) << "wheel event at (" << x << "," << y << ") delta:" << angleDelta;
}

void DomainViewer::mouseMoveEvent(QMouseEvent *event)
{
    int button_mask = 0;
    if (event->buttons() & Qt::LeftButton)
        button_mask |= SPICE_MOUSE_BUTTON_MASK_LEFT;
    if (event->buttons() & Qt::MiddleButton)
        button_mask |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
    if (event->buttons() & Qt::RightButton)
        button_mask |= SPICE_MOUSE_BUTTON_MASK_RIGHT;

    qreal x = event->position().x();
    qreal y = event->position().y();

    const QSize frameSize = m_renderer ? m_renderer->frameSize() : QSize();
    if (width() > 0 && height() > 0) {
        x = (x * frameSize.width()) / width();
        y = (y * frameSize.height()) / height();
    }

    // note: theres a warning that it's deprecated, but newer version has some bug with drag.
    spice_inputs_position(m_inputs_channel, x, y, 0, button_mask);
    static int moveCounter = 0;
    if (++moveCounter % 10 == 0) {
        qCDebug(KARTON_DEBUG) << "Mouse drag at (" << x << "," << y << "), mask: " << m_current_button_mask;
    }

    event->accept();
}
void DomainViewer::hoverMoveEvent(QHoverEvent *event)
{
    static int hoverCounter = 0;
    if (++hoverCounter % 20 == 0) {
        qCInfo(KARTON_DEBUG) << "Mouse hover at (" << event->position().x() << "," << event->position().y() << ")";
    }
    if (m_inputs_channel && m_connected) {
        qreal x = event->position().x();
        qreal y = event->position().y();

        const QSize frameSize = m_renderer ? m_renderer->frameSize() : QSize();
        if (frameSize.width() > 0 && frameSize.height() > 0 && width() > 0 && height() > 0) {
            x = (x * frameSize.width()) / width();
            y = (y * frameSize.height()) / height();
        }

        spice_inputs_channel_position(m_inputs_channel, x, y, 0, 0);
    }
}

void DomainViewer::mousePressEvent(QMouseEvent *event)
{
    qCInfo(KARTON_DEBUG) << "Mouse click at (" << event->position().x() << "," << event->position().y() << ") button:" << event->button();
    setFocus(true);

    switch (event->button()) {
    case Qt::LeftButton:
        m_current_button_mask |= SPICE_MOUSE_BUTTON_MASK_LEFT;
        spice_inputs_channel_button_press(m_inputs_channel, SPICE_MOUSE_BUTTON_LEFT, m_current_button_mask);
        break;
    case Qt::MiddleButton:
        m_current_button_mask |= SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        spice_inputs_channel_button_press(m_inputs_channel, SPICE_MOUSE_BUTTON_MIDDLE, m_current_button_mask);
        break;
    case Qt::RightButton:
        m_current_button_mask |= SPICE_MOUSE_BUTTON_MASK_RIGHT;
        spice_inputs_channel_button_press(m_inputs_channel, SPICE_MOUSE_BUTTON_RIGHT, m_current_button_mask);
        break;
    default:
        return;
    }

    event->accept();
}

void DomainViewer::mouseReleaseEvent(QMouseEvent *event)
{
    qCInfo(KARTON_DEBUG) << "Mouse release at (" << event->position().x() << "," << event->position().y() << ") button:" << event->button();
    int button = 0;
    switch (event->button()) {
    case Qt::LeftButton:
        button = SPICE_MOUSE_BUTTON_LEFT;
        m_current_button_mask &= ~SPICE_MOUSE_BUTTON_MASK_LEFT;
        break;
    case Qt::MiddleButton:
        button = SPICE_MOUSE_BUTTON_MIDDLE;
        m_current_button_mask &= ~SPICE_MOUSE_BUTTON_MASK_MIDDLE;
        break;
    case Qt::RightButton:
        button = SPICE_MOUSE_BUTTON_RIGHT;
        m_current_button_mask &= ~SPICE_MOUSE_BUTTON_MASK_RIGHT;
        break;
    default:
        return;
    }

    spice_inputs_channel_button_release(m_inputs_channel, button, m_current_button_mask);

    event->accept();
}

void DomainViewer::checkChannelStatus()
{
    if (!m_session) {
        qCInfo(KARTON_DEBUG) << "Channel check: No active session";
        return;
    }
    if (!SPICE_IS_SESSION(m_session)) {
        qCWarning(KARTON_DEBUG) << "Session pointer is invalid!";
        m_session = nullptr;
        return;
    }

    GList *channels = spice_session_get_channels(m_session);
    qCInfo(KARTON_DEBUG) << "Channel check: Found" << (channels ? g_list_length(channels) : 0) << "channels";

    bool hasDisplayChannel = false;
    for (GList *iter = channels; iter; iter = iter->next) {
        SpiceChannel *channel = SPICE_CHANNEL(iter->data);
        gint type;
        g_object_get(channel, "channel-type", &type, NULL);
        const gchar *type_str = spice_channel_type_to_string(type);

        qCInfo(KARTON_DEBUG) << "  - Channel type:" << QString::fromUtf8(type_str);

        if (SPICE_IS_DISPLAY_CHANNEL(channel)) {
            hasDisplayChannel = true;
            qCInfo(KARTON_DEBUG) << "    Found display channel";
            gint channel_id;
            g_object_get(channel, "channel-id", &channel_id, NULL);
            qCInfo(KARTON_DEBUG) << "    Display channel ID:" << channel_id;
        }
    }

    if (!hasDisplayChannel) {
        qCWarning(KARTON_DEBUG) << "No display channel found after" << (m_connected ? "successful connection!" : "failed connection");
    }
}

