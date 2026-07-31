// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import QtQuick.Dialogs as Dialogs
import org.kde.kirigamiaddons.formcard 1.0 as FormCard
import org.kde.karton

Kirigami.Dialog {
    title: i18n("Add New Virtual Machine")
    id: addDomainDialog
    padding: Kirigami.Units.largeSpacing
    property bool showError: false
    modal: true

    function getShortOsId(path) {
        return OsinfoConfig.getShortIdFromId(OsinfoConfig.getOsIdFromDisk(path));
    }

    onOpened: {
        nameField.forceActiveFocus()
    }

    customFooterActions: [
        Kirigami.Action {
            text: i18nc("verb, creation button for a new VM", "Create")
            icon.name: "dialog-ok"
            onTriggered: {
                if (nameField.text.trim() === ""
                    || diskImageField.text.trim() === ""
                    || !OsinfoConfig.getOsVariants().includes(osField.editText.trim())) {
                    showError = true;
                    return;
                }
                const domainConfig = {
                    name: nameField.text.trim(),
                    shortOsId: osField.editText.trim(),
                    isoDiskPath: diskImageField.text,
                    memoryGB: memorySpinBox.value,
                    storageGB: storageSpinBox.value,
                    cpus: cpuSpinBox.value,
                    enableAccel3d: enableAccel3dCheckBox.checked
                };
                Karton.createDomain(domainConfig);
                showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Created VM: %1", nameField.text));
                addDomainDialog.close();
            }
        }
    ]

    preferredWidth: parent.width - Kirigami.Units.gridUnit * 10
    preferredHeight: parent.height - Kirigami.Units.gridUnit * 10

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit * 2
            Layout.rightMargin: Kirigami.Units.gridUnit * 2
            text: i18nc("Error message", "Please fill in all required fields")
            type: Kirigami.MessageType.Error
            visible: addDomainDialog.showError
        }

        FormCard.FormCard {
            Layout.fillWidth: true

            FormCard.FormTextFieldDelegate {
                id: nameField
                label: i18nc("@label:textbox", "VM Name:")
                placeholderText: i18n("Enter VM Name")
                Layout.fillWidth: true
                validator: RegularExpressionValidator {
                    regularExpression: /^[^\s]+$/
                }
            }

            Dialogs.FileDialog {
                id: fileDialog
                title: i18nc("@label:filedialog", "Choose a disk image")
                nameFilters: [i18nc("name filter for the file dialog",
                                    "Disk images (*.qcow2 *.raw *.img *.iso *.vdi *.vmdk)")]
                onAccepted: {
                    diskImageField.text = fileDialog.selectedFile.toString().replace("file://", "");
                    let shortOsId = getShortOsId(diskImageField.text);
                    if (shortOsId === "") {
                        osTextField.placeholderText = i18n("Could not identify the OS. Please enter an OS Variant.");
                        osField.editText = "";
                        osTextField.text = osField.editText;
                    } else {
                        osField.editText = shortOsId;
                        osTextField.text = osField.editText;
                    }
                }
            }

            FormCard.FormDelegateSeparator {}
            FormCard.AbstractFormDelegate {
                background: null
                contentItem: RowLayout {
                    Layout.fillWidth: true

                    Controls.TextField {
                        id: diskImageField
                        Layout.fillWidth: true
                        placeholderText: i18n("Select a disk image")
                        readOnly: true
                    }

                    Controls.Button {
                        text: i18nc("verb, look for a file in explorer", "Browse")
                        onClicked: {
                            fileDialog.open();
                        }
                    }
                }
            }
            FormCard.AbstractFormDelegate {
                background: null // removes hover selection
                hoverEnabled: false
                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Controls.Label {
                        text: i18n("OS Variant:")
                    }

                    Controls.ComboBox {
                        id: osField
                        Layout.fillWidth: true
                        editable: true
                        currentIndex: -1

                        contentItem: Controls.TextField {
                            id: osTextField
                            placeholderText: i18n("Select or enter an OS variant")
                            background: Item {}
                            onTextEdited: suggestionsPopup.updateSuggestions()
                            onActiveFocusChanged: {
                                if (activeFocus) {
                                    suggestionsPopup.updateSuggestions();
                                } else {
                                    suggestionsPopup.close();
                                }
                            }
                        }

                        // triggers on text edits in OS variant selection
                        Controls.Popup {
                            id: suggestionsPopup
                            parent: osField
                            y: osField.height
                            width: osField.width
                            padding: 0
                            implicitHeight: Math.min(listView.contentHeight, Kirigami.Units.gridUnit * 10)

                            function updateSuggestions() {
                                const filterText = osTextField.text.toLowerCase();
                                listView.model = OsinfoConfig.getOsVariants().filter((osVariant) => osVariant.toLowerCase().includes(filterText));
                                listView.model.length > 0 ? open() : close();
                            }

                            contentItem: ListView {
                                id: listView
                                implicitHeight: contentHeight
                                clip: true
                                delegate: Controls.ItemDelegate {
                                    width: ListView.view.width
                                    text: modelData
                                    onClicked: {
                                        osTextField.text = modelData;
                                        suggestionsPopup.close();
                                        osTextField.forceActiveFocus();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        FormCard.FormCard {
            Layout.bottomMargin: Kirigami.Units.largeSpacing
            FormCard.FormSpinBoxDelegate {
                id: memorySpinBox
                label: i18nc("@label:spinbox, RAM", "Memory (GB)")
                value: 4
                from: 1
                to: 64
                Layout.fillWidth: true
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                id: storageSpinBox
                label: i18nc("@label:spinbox", "Disk Storage (GB)")
                from: 1
                to: 2048
                value: 4
                Layout.fillWidth: true
            }

            FormCard.FormDelegateSeparator {}

            FormCard.FormSpinBoxDelegate {
                id: cpuSpinBox
                label: i18nc("@label:spinbox, number of cpus", "CPUs")
                from: 1
                to: 16
                value: 2
                Layout.fillWidth: true
            }

            // FormCard.FormDelegateSeparator {}

            FormCard.FormCheckDelegate {
                id: enableAccel3dCheckBox
                text: i18nc("@option:check", "Enable hardware acceleration")
                checked: true
                Layout.fillWidth: true
            }
        }
    }
}