// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.karton

Kirigami.ApplicationWindow {
    id: viewerWindow
    required property Domain domain
    property bool initialSizeApplied: false
    
    title: domain ? i18nc("%1 is the name of the virtual machine", "VM Viewer - %1", domain.config.name) : i18n("VM Viewer")
    
    width: Kirigami.Units.gridUnit * 53
    height: Kirigami.Units.gridUnit * 36

    onClosing: {
        domainViewer.saveFrameToDomain();
        domainViewer.disconnectFromSpice();
    }

    function applyInitialSize() {
        if (initialSizeApplied
            || viewerWindow.visibility !== Window.Windowed
            || domainViewer.implicitWidth <= 0
            || domainViewer.implicitHeight <= 0) {
            return
        }
        initialSizeApplied = true

        const dpr = domainViewer.dprHelper.devicePixelRatio
        viewerWindow.width = Math.min(Screen.desktopAvailableWidth, domainViewer.implicitWidth / dpr)
        viewerWindow.height = Math.min(Screen.desktopAvailableHeight,
                                       domainViewer.implicitHeight / dpr + pageStack.globalToolBar.height)
    }

    Connections {
        target: domainViewer
        function onImplicitWidthChanged() {
            viewerWindow.applyInitialSize()
        }
        function onImplicitHeightChanged() {
            viewerWindow.applyInitialSize()
        }
    }

    pageStack.initialPage: Kirigami.Page {
        title: viewerWindow.title
        padding: 0 

        actions: [
            Kirigami.Action {
                text: viewerWindow.visibility === Window.FullScreen ? i18n("Exit Full Screen") : i18n("Full Screen")
                icon.name: viewerWindow.visibility === Window.FullScreen ? "view-restore" : "view-fullscreen"
                checkable: true
                checked: viewerWindow.visibility === Window.FullScreen
                onTriggered: {
                    if (viewerWindow.visibility === Window.FullScreen) {
                        viewerWindow.showNormal()
                    } else {
                        viewerWindow.showFullScreen()
                    }
                }
            }
        ]

        Rectangle {
            id: viewerArea

            anchors.fill: parent
            color: "black"

            DomainViewer {
                id: domainViewer

                anchors.centerIn: parent

                property DevicePixelRatioHelper dprHelper: DevicePixelRatioHelper {
                    window: domainViewer.Window.window
                }

                readonly property real nativeWidth: implicitWidth > 0 ? implicitWidth / dprHelper.devicePixelRatio : 0
                readonly property real nativeHeight: implicitHeight > 0 ? implicitHeight / dprHelper.devicePixelRatio : 0
                readonly property real fitScale: nativeWidth > 0 && nativeHeight > 0
                    ? Math.min(viewerArea.width / nativeWidth, viewerArea.height / nativeHeight)
                    : 1.0

                // the container size, not the fitted size, or the guest never fills the window
                availableArea: Qt.size(viewerArea.width, viewerArea.height)

                // sized rather than scaled, the mouse mapping divides by width()
                width: nativeWidth > 0 ? Math.round(nativeWidth * fitScale) : Kirigami.Units.gridUnit * 56.55
                height: nativeHeight > 0 ? Math.round(nativeHeight * fitScale) : Kirigami.Units.gridUnit * 36

                domain: viewerWindow.domain

                focus: true
                activeFocusOnTab: true
                onActiveFocusChanged: {
                    console.log("DomainViewer focus changed to:", activeFocus)
                }
                onFocusChanged: {
                    console.log("DomainViewer focus property changed to:", focus)
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: (mouse) => {
                        console.log("MouseArea click. giving focus to domainviewer")
                        parent.forceActiveFocus()
                        mouse.accepted = false
                    }
                }
            }
        }
    }
}