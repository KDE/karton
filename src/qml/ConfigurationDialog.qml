// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.delegates as Delegates
import org.kde.kirigamiaddons.formcard 1.0 as FormCard
import org.kde.karton

Kirigami.Dialog {
    title: domain ? i18nc("%1 is the name of the virtual machine", "Configure '%1'", domain.config.name) : ""
    id: configurationDialog
    padding: 0
    modal: true

    property Domain domain: null

    preferredWidth: Kirigami.Units.gridUnit * 32
    preferredHeight: Kirigami.Units.gridUnit * 22

    onOpened: {
        categoryList.currentIndex = 0;
        nameField.text = domain.config.name;
        memorySpinBox.value = domain.config.maxRam;
        cpuSpinBox.value = domain.config.cpus;
        storageSpinBox.value = domain.config.maxDiskStorage;
        nameField.forceActiveFocus();
    }

    customFooterActions: [
        Kirigami.Action {
            text: i18nc("verb, save VM configuration changes", "Save")
            icon.name: "dialog-ok"
            onTriggered: {
                if (nameField.text.trim() === "") {
                    return;
                }
                const domainConfig = {
                    name: nameField.text.trim(),
                    memoryGB: memorySpinBox.value,
                    storageGB: storageSpinBox.value,
                    cpus: cpuSpinBox.value
                };
                if (Karton.editDomain(configurationDialog.domain, domainConfig)) {
                    showPassiveNotification(i18nc("%1 is the name of the virtual machine", "Updated configuration for VM: %1", domainConfig.name));
                    configurationDialog.close();
                }
            }
        }
    ]

    RowLayout {
        spacing: 0

        ListView {
            id: categoryList
            Layout.preferredWidth: Kirigami.Units.gridUnit * 8
            Layout.fillHeight: true
            currentIndex: 0

            model: [
                {label: i18nc("@title:group configuration category", "General"), icon: "settings-configure"},
                {label: i18nc("@title:group configuration category", "Hardware"), icon: "cpu"},
                {label: i18nc("@title:group configuration category", "Storage"), icon: "drive-harddisk-symbolic"}
            ]

            delegate: Delegates.RoundedItemDelegate {
                width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                text: modelData.label
                icon.name: modelData.icon
                highlighted: ListView.isCurrentItem
                onClicked: categoryList.currentIndex = index
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: categoryList.currentIndex

            ColumnLayout {
                spacing: 0

                FormCard.FormCard {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing

                    FormCard.FormTextFieldDelegate {
                        id: nameField
                        label: i18nc("@label:textbox", "VM Name:")
                        Layout.fillWidth: true
                        validator: RegularExpressionValidator {
                            regularExpression: /^[^\s]+$/
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            ColumnLayout {
                spacing: 0

                FormCard.FormCard {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing

                    FormCard.FormSpinBoxDelegate {
                        id: memorySpinBox
                        label: i18nc("@label:spinbox, RAM", "Memory (GB)")
                        from: 1
                        to: 64
                        Layout.fillWidth: true
                    }

                    FormCard.FormDelegateSeparator {}

                    FormCard.FormSpinBoxDelegate {
                        id: cpuSpinBox
                        label: i18nc("@label:spinbox, number of cpus", "CPUs")
                        from: 1
                        to: 16
                        Layout.fillWidth: true
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            ColumnLayout {
                spacing: 0

                FormCard.FormCard {
                    Layout.fillWidth: true
                    Layout.margins: Kirigami.Units.largeSpacing

                    FormCard.FormSpinBoxDelegate {
                        id: storageSpinBox
                        label: i18nc("@label:spinbox", "Disk Storage (GB)")
                        from: 1
                        to: 2048
                        Layout.fillWidth: true
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
