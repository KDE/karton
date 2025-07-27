// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.kde.karton


Kirigami.ScrollablePage {
    title: i18nc("noun, title of listpage","Karton Virtual Machine Manager")
    Component.onCompleted: {
        Karton.errorOccurred.connect(function(errorMessage) {
            showPassiveNotification(errorMessage, "long");
        });
    }
    actions: [
        Kirigami.Action {
            icon.name: "list-add-symbolic"
            text: i18nc("verb, to add a new virtual machine","Add")
            onTriggered: source => {
                addDomainDialog.open();
            }
        }
    ]

    InstallationDialog {
        id: addDomainDialog
    }

    Component {
        id: vmViewerWindowComponent
        VMViewerWindow {}
    }

    Kirigami.CardsListView {
        id: view
        model: VMModel

        delegate: Kirigami.AbstractCard {
            contentItem: Item {
                implicitWidth: delegateLayout.implicitWidth
                implicitHeight: delegateLayout.implicitHeight
                RowLayout {
                    id: delegateLayout
                    anchors {
                        left: parent.left
                        top: parent.top
                        right: parent.right
                    }
                    Kirigami.Icon {
                        source: "computer-symbolic" 
                        // TODO: Add OS Icon -> eventually, have screencap of VM window
                        Layout.fillHeight: true
                        Layout.maximumHeight: Kirigami.Units.iconSizes.huge
                        Layout.preferredWidth: height
                    }
                    ColumnLayout {
                        Kirigami.Heading {
                            level: 2
                            text: domain.config.name
                        }
                        Kirigami.Separator {
                            Layout.fillWidth: true
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "UUID: " + domain.config.uuid
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "State: " + domain.config.state
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Memory (GB): " + domain.config.maxRam
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Memory Usage (GB): " + domain.config.ramUsage
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "CPU Cores: " + domain.config.cpus
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Virtual Disk: " + domain.config.virtualDiskPath
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "ISO Disk: " + domain.config.isoDiskPath
                        }

                    }
                    ColumnLayout{
                        Controls.Button {
                            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
                            Layout.columnSpan: 1
                            text: i18nc("verb, start a VM", "Start")
                            icon.name: "media-playback-start"
                            onClicked: {
                                Karton.startDomain(domain)
                                showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Starting VM: %1!", domain.config.name));
                            }
                        }
                        Controls.Button {
                            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
                            Layout.columnSpan: 1
                            text: i18nc("verb, stop a VM", "Stop")
                            icon.name: "system-shutdown"
                            onClicked: {
                                Karton.stopDomain(domain)
                                showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Stopping VM: %1!", domain.config.name));
                            }
                        }
                        Controls.Button {
                            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
                            Layout.columnSpan: 1
                            text: i18nc("verb, stop a VM immediately", "Force Stop")
                            // icon.name: "process-stop"
                            onClicked: {
                                Karton.forceStopDomain(domain)
                                showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Force-stopping VM: %1!", domain.config.name));
                            }
                        }
                        Controls.Button {
                            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
                            Layout.columnSpan: 1
                            text: i18nc("verb, open viewer for VM", "View VM")
                            icon.name: "computer-laptop-symbolic"
                            onClicked: {
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
                        Controls.Button {
                            Layout.alignment: Qt.AlignRight|Qt.AlignVCenter
                            Layout.columnSpan: 1
                            text: i18nc("verb, delete a VM", "Delete")
                            icon.name: "delete"
                            onClicked: {
                                if (domain.config.state === "running") {
                                    showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Error: %1 is still running!", domain.config.name));
                                    return;
                                }
                                deleteConfirmationDialog.domain = domain;
                                deleteConfirmationDialog.open();
                            }
                        }
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)

            visible: view.count === 0

            text: i18nc("@title, greet user to Karton", "Welcome to Karton!")
            explanation: i18n("Create a new virtual machine to proceed.")
            icon.name: "computer-symbolic"
        }
    }

    Kirigami.Dialog {
        id: deleteConfirmationDialog
        title: i18nc("Confirm deleting %1 (virtual machine name)", "Delete '%1'?", 
            deleteConfirmationDialog.domain ? deleteConfirmationDialog.domain.config.name : "")

        padding: Kirigami.Units.largeSpacing
        preferredWidth: root.width - Kirigami.Units.gridUnit * 30
        preferredHeight: root.height - Kirigami.Units.gridUnit * 30
        
        showCloseButton: false
        standardButtons: Kirigami.Dialog.NoButton
        flatFooterButtons: false

        property var domain: null

        Controls.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18nc("%1 is the virtual machine name",
`You are about to remove the virtual machine, '%1'. 
Would you like to remove the disk image as well? 
This action cannot be undone.`, 
            deleteConfirmationDialog.domain.config.name)
        }

        customFooterActions: [
            Kirigami.Action {
                text: i18nc("verb, close confirmation dialog", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: {
                    deleteConfirmationDialog.domain = null;
                    deleteConfirmationDialog.close();
                }
            },
            Kirigami.Action {
                text: i18nc("action, delete VM but keep disk image", "Keep File")
                icon.name: "edit-delete-remove"
                
                onTriggered: {
                    if (deleteConfirmationDialog.domain) {
                        Karton.deleteDomain(deleteConfirmationDialog.domain, false);
                        showPassiveNotification(i18nc("%1 is the virtual machine name", "Undefining %1!", deleteConfirmationDialog.domain.config.name));
                        deleteConfirmationDialog.domain = null;
                        deleteConfirmationDialog.close();
                    }
                }
            },
            Kirigami.Action {
                text: i18nc("action, delete VM and disk image", "Delete Disk")
                icon.name: "delete"
                
                onTriggered: {
                    if (deleteConfirmationDialog.domain) {
                        Karton.deleteDomain(deleteConfirmationDialog.domain, true);
                        showPassiveNotification(i18nc("%1 is the virtual machine name", "Undefining %1!", deleteConfirmationDialog.domain.config.name));
                        deleteConfirmationDialog.domain = null;
                        deleteConfirmationDialog.close();
                    }
                }
            }
        ]
    }
}
