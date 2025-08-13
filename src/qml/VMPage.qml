// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard 1.0 as FormCard
import org.kde.karton

Kirigami.ScrollablePage {
    title: domain.config.name
    property Domain domain: null
    readonly property bool isRunning: domain.config.state === "running"
    
    topPadding: 0
    bottomPadding: Kirigami.Units.gridUnit
    leftPadding: 0
    rightPadding: 0
    
    actions: [
        Kirigami.Action {
            icon.name: isRunning ? "system-shutdown" : "media-playback-start" 
            text: isRunning ? i18nc("verb, stop a VM", "Stop") : i18nc("verb, start a VM", "Start")
            onTriggered: {
                if (isRunning) {
                    Karton.stopDomain(domain);
                    showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Stopping VM: %1!", domain.config.name));
                } else {
                    Karton.startDomain(domain)
                    showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Starting VM: %1!", domain.config.name));
                }
            }
        },
        Kirigami.Action {
            text: i18nc("verb, stop a VM immediately", "Force Stop")
            icon.name: "process-stop"
            visible: isRunning
            onTriggered: {
                Karton.forceStopDomain(domain)
                showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Force-stopping VM: %1!", domain.config.name));
            }
        },
        Kirigami.Action {
            text: i18nc("verb, delete a VM", "Delete")
            icon.name: "delete"
            visible: !isRunning
            onTriggered: {
                if (domain.config.state === "running") {
                    showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Error: %1 is still running!", domain.config.name));
                    return;
                }
                deleteConfirmationDialog.domain = domain;
                deleteConfirmationDialog.open();
            }
        },
        Kirigami.Action {
            id: viewVMAction
            text: i18nc("verb, open viewer for VM", "View VM")
            icon.name: "computer-laptop-symbolic"
            visible: isRunning
            onTriggered: {
                let component = Qt.createComponent("VMViewerWindow.qml")
                if (component.status === Component.Ready) {
                    let window = component.createObject(null, {domain: domain})
                    if (window) {
                        window.show()
                        showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Opening viewer for: %1", domain.config.name))
                    }
                }
            }
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing
        
        Rectangle {
            Layout.fillWidth: true
            color: "black"
            Layout.preferredHeight: Kirigami.Units.gridUnit * 16

            Image {
                id: previewImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: "file://" + domain.previewPath

                Connections {
                    target: domain
                    function onPreviewChanged() {
                        previewImage.source = "file://" + domain.previewPath + "?" + Date.now();
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                color: {
                    if (previewMouseArea.containsMouse) {
                        return Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.5);
                    } else {
                        return Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.6);
                    }
                }
                Behavior on color {
                    ColorAnimation {
                        duration: Kirigami.Units.shortDuration
                    }
                }
            }

            MouseArea {
                id: previewMouseArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: viewVMAction.trigger()
            }
        }
        FormCard.FormCard {
            maximumWidth: parent.width - Kirigami.Units.gridUnit * 2

            FormCard.FormTextDelegate {
                text: i18n("UUID")
                description: domain.config.uuid
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormTextDelegate {
                text: i18n("State")
                description: domain.config.state
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormTextDelegate {
                text: i18n("Maximum Memory (GB)")
                description: domain.config.maxRam
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormTextDelegate {
                text: i18n("Memory Usage (GB)")
                description: domain.config.ramUsage
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormTextDelegate {
                text: i18n("CPUs")
                description: domain.config.cpus
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormTextDelegate {
                text: i18n("Virtual Disk Path")
                description: domain.config.virtualDiskPath
            }
            FormCard.FormDelegateSeparator {}
            FormCard.FormTextDelegate {
                text: i18n("ISO Disk Path")
                description: domain.config.isoDiskPath
                
            }
        }
    }
}