// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Derek Lin <derekhongdalin@gmail.com>

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.kde.karton


Kirigami.ScrollablePage {
    title: i18nc("noun, Virtual Machines","Virtual Machines")
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0
    
    // implicitWidth: applicationWindow().isWidescreen ? Kirigami.Units.gridUnit * 3 : applicationWindow().width
    
    Component.onCompleted: {
        Karton.errorOccurred.connect(function(errorMessage) {
            showPassiveNotification(errorMessage, "long");
        });
    }
    
    actions: [
        Kirigami.Action {
            icon.name: "list-add-symbolic"
            text: i18nc("verb, to create a new virtual machine", "Create")
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

    ListView {
        id: view
        model: VMModel
        
        currentIndex: 0
        
        delegate: Controls.ItemDelegate {
            width: ListView.view.width
            highlighted: ListView.isCurrentItem
            
            leftPadding: Kirigami.Units.gridUnit
            rightPadding: leftPadding
            topPadding: Kirigami.Units.largeSpacing
            bottomPadding: topPadding

            ListView.onIsCurrentItemChanged: {
                if (ListView.isCurrentItem) {
                    while (applicationWindow().pageStack.depth > 1) { 
                        applicationWindow().pageStack.pop();
                    }
                    applicationWindow().pageStack.push(Qt.resolvedUrl("VMPage.qml"), {domain: domain});
                }
            }

            onClicked: {
                ListView.view.currentIndex = model.index;
            }
            contentItem: RowLayout {
                id: delegateLayout
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: "computer-symbolic" 
                    // TODO: Add OS Icon -> eventually, have screencap of VM window
                    Layout.fillHeight: true
                    Layout.maximumHeight: Kirigami.Units.iconSizes.huge
                    Layout.preferredWidth: height
                }
                ColumnLayout {
                    Kirigami.Heading {
                        Layout.fillWidth: true
                        font.weight: Font.DemiBold
                        level: 3
                        text: domain.config.name
                        elide: Text.ElideRight
                    }
                    Controls.Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: domain.config.state + " | " + domain.config.shortOsId
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
}
