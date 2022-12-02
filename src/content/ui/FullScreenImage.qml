// SPDX-FileCopyrightText: 2019 Black Hat <bhat@encom.eu.org>
// SPDX-License-Identifier: GPL-3.0-only

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1

import org.kde.kirigami 2.15 as Kirigami
import org.kde.kmasto 1.0

QQC2.Popup {
    id: root

    required property var model
    property alias currentIndex: view.currentIndex

    parent: QQC2.Overlay.overlay
    closePolicy: QQC2.Popup.CloseOnEscape
    width: parent.width
    height: parent.height
    modal: true
    padding: 0
    background: null

    Keys.onBackPressed: root.close()

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        QQC2.Control {
            Layout.fillWidth: true

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    id: imageLabel
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing

                    text: root.model[view.currentIndex].description
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                QQC2.ToolButton {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2

                    text: i18n("Zoom in")
                    Accessible.name: text
                    icon.name: "zoom-in"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: {
                        view.currentItem.image.scaleFactor = view.currentItem.image.scaleFactor + 0.25
                        if (view.currentItem.image.scaleFactor > 3) {
                            view.currentItem.image.scaleFactor = 3
                        }
                    }

                    QQC2.ToolTip {
                        text: parent.text
                    }
                }
                QQC2.ToolButton {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2

                    text: i18n("Zoom out")
                    Accessible.name: text
                    icon.name: "zoom-out"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: {
                        view.currentItem.image.scaleFactor = view.currentItem.image.scaleFactor - 0.25
                        if (view.currentItem.image.scaleFactor < 0.25) {
                            view.currentItem.image.scaleFactor = 0.25
                        }
                    }

                    QQC2.ToolTip {
                        text: parent.text
                    }
                }
                QQC2.ToolButton {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2

                    text: i18n("Rotate left")
                    Accessible.name: text
                    icon.name: "image-rotate-left-symbolic"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: view.currentItem.image.rotationAngle = view.currentItem.image.rotationAngle - 90

                    QQC2.ToolTip {
                        text: parent.text
                    }
                }
                QQC2.ToolButton {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2

                    text: i18n("Rotate right")
                    Accessible.name: text
                    icon.name: "image-rotate-right-symbolic"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: view.currentItem.image.rotationAngle = view.currentItem.image.rotationAngle + 90

                    QQC2.ToolTip {
                        text: parent.text
                    }
                }
                QQC2.ToolButton {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2

                    text: i18n("Save as")
                    Accessible.name: text
                    icon.name: "document-save"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: {
                        const dialog = saveAsDialog.createObject(QQC2.ApplicationWindow.overlay, {
                            url: view.currentItem.image.source,
                        })
                        dialog.open();
                        dialog.currentFile = dialog.folder + "/" + FileHelper.fileName(view.currentItem.image.source);
                    }

                    QQC2.ToolTip {
                        text: parent.text
                    }
                }
                QQC2.ToolButton {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2

                    text: i18n("Close")
                    Accessible.name: text
                    icon.name: "dialog-close"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: {
                        root.close()
                    }

                    QQC2.ToolTip {
                        text: parent.text
                    }
                }
            }

            background: Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
            }

            Kirigami.Separator {
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: 1
            }
        }

        ListView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            snapMode: ListView.SnapOneItem 
            highlightRangeMode: ListView.StrictlyEnforceRange
            highlightMoveDuration: 0
            focus: true
            keyNavigationEnabled: true
            keyNavigationWraps: true
            model: root.model
            orientation: ListView.Horizontal
            clip: true
            delegate: Item {
                id: imageContainer
                width: ListView.view.width
                height: ListView.view.height

                property alias image: imageItem

                AnimatedImage {
                    id: imageItem

                    property var scaleFactor: 1
                    property int rotationAngle: 0
                    property var rotationInsensitiveWidth: Math.min(root.imageWidth > 0 ? root.imageWidth : sourceSize.width, imageContainer.width - Kirigami.Units.largeSpacing * 2)
                    property var rotationInsensitiveHeight: Math.min(root.imageHeight > 0 ? root.imageHeight : sourceSize.height, imageContainer.height - Kirigami.Units.largeSpacing * 2)

                    anchors.centerIn: parent
                    width: rotationAngle % 180 === 0 ? rotationInsensitiveWidth : rotationInsensitiveHeight
                    height: rotationAngle % 180 === 0 ? rotationInsensitiveHeight : rotationInsensitiveWidth
                    fillMode: Image.PreserveAspectFit
                    clip: true
                    source: modelData.url

                    Behavior on width {
                        NumberAnimation {duration: Kirigami.Units.longDuration; easing.type: Easing.InOutCubic}
                    }
                    Behavior on height {
                        NumberAnimation {duration: Kirigami.Units.longDuration; easing.type: Easing.InOutCubic}
                    }

                    transform: [
                        Rotation {
                            origin.x: image.width / 2
                            origin.y: image.height / 2
                            angle: image.rotationAngle

                            Behavior on angle {
                                RotationAnimation {duration: Kirigami.Units.longDuration; easing.type: Easing.InOutCubic}
                            }
                        },
                        Scale {
                            origin.x: image.width / 2
                            origin.y: image.height / 2
                            xScale: image.scaleFactor
                            yScale: image.scaleFactor

                            Behavior on xScale {
                                NumberAnimation {duration: Kirigami.Units.longDuration; easing.type: Easing.InOutCubic}
                            }
                            Behavior on yScale {
                                NumberAnimation {duration: Kirigami.Units.longDuration; easing.type: Easing.InOutCubic}
                            }
                        }
                    ]
                }
            }

            QQC2.RoundButton {
                anchors {
                    left: parent.left
                    leftMargin: Kirigami.Units.largeSpacing
                    verticalCenter: parent.verticalCenter
                }
                width: Kirigami.Units.gridUnit * 2
                height: width
                icon.name: "arrow-left"
                visible: !Kirigami.Settings.isMobile && view.currentIndex > 0
                Keys.forwardTo: view
                Accessible.name: i18n("Previous image")
                onClicked: view.currentIndex -= 1
            }

            QQC2.RoundButton {
                anchors {
                    right: parent.right
                    rightMargin: Kirigami.Units.largeSpacing
                    verticalCenter: parent.verticalCenter
                }
                width: Kirigami.Units.gridUnit * 2
                height: width
                icon.name: "arrow-right"
                visible: !Kirigami.Settings.isMobile && view.currentIndex < view.count - 1
                Keys.forwardTo: view
                Accessible.name: i18n("Next image")
                onClicked: view.currentIndex += 1
            }
        }
    }

    Component {
        id: saveAsDialog
        FileDialog {
            property var url
            fileMode: FileDialog.SaveFile
            folder: StandardPaths.writableLocation(StandardPaths.DownloadLocation)
            onAccepted: {
                if (!currentFile) {
                    return;
                }
                console.log(url, currentFile, AccountManager.selectedAccount)
                FileHelper.downloadFile(AccountManager.selectedAccount, url, currentFile)
            }
        }
    }

    onClosed: {
        view.currentItem.image.scaleFactor = 1
        view.currentItem.image.rotationAngle = 0
    }
}
