import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: pagePalette.base

    SystemPalette {
        id: pagePalette
        colorGroup: SystemPalette.Active
    }

    readonly property color sectionColor: pagePalette.base
    readonly property color textColor: pagePalette.text
    readonly property color mutedTextColor: pagePalette.mid
    readonly property color borderColor: pagePalette.mid
    readonly property int outerMargin: 0
    readonly property int sectionPadding: 12
    readonly property int sectionHeaderHeight: 32

    function distanceToSlider(distance) {
        return Math.log(Math.max(4, distance)) / Math.LN10
    }

    function sliderToDistance(sliderValue) {
        return Math.pow(10, sliderValue)
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + root.outerMargin * 2
        clip: true

        ColumnLayout {
            id: contentColumn
            width: flick.width - root.outerMargin * 2
            x: root.outerMargin
            y: root.outerMargin
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: cameraContent.implicitHeight + root.sectionHeaderHeight + root.sectionPadding
                color: root.sectionColor
                border.color: root.borderColor
                border.width: 1

                Label {
                    x: root.sectionPadding
                    y: 10
                    text: qsTr("Camera")
                    color: root.textColor
                    font.bold: true
                }

                ColumnLayout {
                    id: cameraContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: root.sectionPadding
                    anchors.rightMargin: root.sectionPadding
                    anchors.topMargin: root.sectionHeaderHeight
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Compass")
                            color: root.textColor
                        }

                        Slider {
                            id: cameraFacingSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 359
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.cameraFacingDegrees : 0
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraFacingDegrees = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1°").arg(Math.round(cameraFacingSlider.value))
                            color: root.textColor
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Tilt")
                            color: root.textColor
                        }

                        Slider {
                            id: cameraTiltSlider
                            Layout.fillWidth: true
                            from: -90
                            to: 90
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.cameraTiltDegrees : 0
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraTiltDegrees = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1°").arg(Math.round(cameraTiltSlider.value))
                            color: root.textColor
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Distance")
                            color: root.textColor
                        }

                        Slider {
                            id: cameraDistanceSlider
                            Layout.fillWidth: true
                            from: root.distanceToSlider(4)
                            to: root.distanceToSlider(inspectorState ? Math.max(500, inspectorState.cameraDistanceMeters * 2) : 500)
                            stepSize: 0.001
                            live: true
                            value: root.distanceToSlider(inspectorState ? inspectorState.cameraDistanceMeters : 120)
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraDistanceMeters = root.sliderToDistance(value)
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1 m").arg(Math.round(root.sliderToDistance(cameraDistanceSlider.value)))
                            color: root.textColor
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        opacity: inspectorState && inspectorState.orthographicProjection ? 0.45 : 1.0

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Focal Length")
                            color: root.textColor
                        }

                        Slider {
                            id: cameraFocalLengthSlider
                            Layout.fillWidth: true
                            enabled: inspectorState ? !inspectorState.orthographicProjection : true
                            from: 10
                            to: 80
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.cameraFocalLengthMm : 35
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.cameraFocalLengthMm = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            enabled: cameraFocalLengthSlider.enabled
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1 mm").arg(Math.round(cameraFocalLengthSlider.value))
                            color: root.textColor
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 120
                        visible: inspectorState ? inspectorState.orthographicProjection : false
                        text: qsTr("Focal length is disabled in orthographic projection.")
                        wrapMode: Text.WordWrap
                        color: root.mutedTextColor
                        font.pointSize: Math.max(8, Qt.application.font.pointSize - 1)
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: layersContent.implicitHeight + root.sectionHeaderHeight + root.sectionPadding
                color: root.sectionColor
                border.color: root.borderColor
                border.width: 1

                Label {
                    x: root.sectionPadding
                    y: 10
                    text: qsTr("Layers")
                    color: root.textColor
                    font.bold: true
                }

                Column {
                    id: layersContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: root.sectionPadding
                    anchors.rightMargin: root.sectionPadding
                    anchors.topMargin: root.sectionHeaderHeight
                    spacing: 6

                    Repeater {
                        model: layerModel

                        delegate: CheckBox {
                            required property int rowIndex
                            required property int indent
                            required property bool hasChildren
                            required property string label
                            required property int layerChecked

                            x: indent * 34
                            width: parent.width - x
                            tristate: hasChildren
                            checkState: layerChecked
                            text: label
                            palette.text: root.textColor
                            palette.buttonText: root.textColor
                            nextCheckState: function() {
                                return checkState === Qt.Checked ? Qt.Unchecked : Qt.Checked
                            }
                            onClicked: {
                                if (layerModel) {
                                    layerModel.setLayerVisible(rowIndex, checkState === Qt.Checked)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: sceneSettingsContent.implicitHeight + root.sectionHeaderHeight + root.sectionPadding
                color: root.sectionColor
                border.color: root.borderColor
                border.width: 1

                Label {
                    x: root.sectionPadding
                    y: 10
                    text: qsTr("Scene Settings")
                    color: root.textColor
                    font.bold: true
                }

                ColumnLayout {
                    id: sceneSettingsContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: root.sectionPadding
                    anchors.rightMargin: root.sectionPadding
                    anchors.topMargin: root.sectionHeaderHeight
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Model Coloring")
                            color: root.textColor
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Altitude"), qsTr("None")]
                            currentIndex: inspectorState ? inspectorState.meshColorMode : 0
                            onActivated: {
                                if (inspectorState) {
                                    inspectorState.meshColorMode = currentIndex
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Background")
                            color: root.textColor
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Black"), qsTr("White")]
                            currentIndex: inspectorState ? inspectorState.backgroundMode : 0
                            onActivated: {
                                if (inspectorState) {
                                    inspectorState.backgroundMode = currentIndex
                                }
                            }
                        }
                    }

                    CheckBox {
                        text: qsTr("Show Bounding Box")
                        palette.text: root.textColor
                        palette.buttonText: root.textColor
                        checked: inspectorState ? inspectorState.showBoundingBox : true
                        onToggled: {
                            if (inspectorState) {
                                inspectorState.showBoundingBox = checked
                            }
                        }
                    }

                    CheckBox {
                        text: qsTr("Show Head-Up Display")
                        palette.text: root.textColor
                        palette.buttonText: root.textColor
                        checked: inspectorState ? inspectorState.showHud : true
                        onToggled: {
                            if (inspectorState) {
                                inspectorState.showHud = checked
                            }
                        }
                    }

                    CheckBox {
                        text: qsTr("Show Title & Stats")
                        palette.text: root.textColor
                        palette.buttonText: root.textColor
                        checked: inspectorState ? inspectorState.showInfo : true
                        onToggled: {
                            if (inspectorState) {
                                inspectorState.showInfo = checked
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Rotation Speed")
                            color: root.textColor
                        }

                        Slider {
                            id: rotationSpeedSlider
                            Layout.fillWidth: true
                            from: 5
                            to: 90
                            stepSize: 1
                            live: true
                            value: inspectorState ? inspectorState.autoRotationSpeed : 30
                            onMoved: {
                                if (inspectorState) {
                                    inspectorState.autoRotationSpeed = value
                                }
                            }
                        }

                        Label {
                            Layout.preferredWidth: 64
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%1°/s").arg(Math.round(rotationSpeedSlider.value))
                            color: root.textColor
                        }
                    }
                }
            }
        }
    }
}
