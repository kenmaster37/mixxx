import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "." as Setting
import "../Theme"

Category {
    id: root

    property bool dirty: false
    property bool stemsEnabled: true
    property bool writeNextToTrack: true
    property bool pauseWhilePlaying: true
    property int codecIndex: 0
    property int modelIndex: 0
    property int providerIndex: 0
    property string pythonPath: ""
    property string cacheDirectory: ""
    readonly property var codecs: ["aac", "mp3", "alac"]
    readonly property var models: ["htdemucs", "htdemucs_ft"]
    readonly property var providers: ["auto", "cuda", "cpu"]

    label: "Stems"

    function markDirty() {
        root.dirty = true;
    }

    // The installer for the engine that carries its own interpreter. It belongs
    // to StemGenerator and keeps its own state, so the choices below are pushed
    // into it and read back from it instead of being duplicated here.
    readonly property var engine: Mixxx.Core.stemGenerator.engine

    function engineVariantSize() {
        const list = engine.variants;
        for (let i = 0; i < list.length; ++i) {
            if (list[i].key === engine.variant) {
                return list[i].sizeText;
            }
        }
        return "";
    }

    function engineModelSize() {
        const list = engine.models;
        for (let i = 0; i < list.length; ++i) {
            if (list[i].key === engine.model) {
                return list[i].sizeText;
            }
        }
        return "";
    }

    function engineItemStateText(state) {
        if (state === "downloading") {
            return "downloading";
        }
        if (state === "done") {
            return "ready";
        }
        if (state === "failed") {
            return "failed";
        }
        return "";
    }

    function engineStateText() {
        if (engine.platformNotice !== "") {
            return engine.platformNotice;
        }
        if (engine.busy) {
            return engine.statusText;
        }
        if (engine.state === "failed") {
            return engine.errorText;
        }
        if (engine.state === "outdated") {
            return "The installed engine is not the one this Mixxx expects. Installing it again brings it up to date.";
        }
        if (engine.state === "ready") {
            return engine.statusText;
        }
        return "Not installed. Separating a track needs this engine, or a Python of your own with the libraries installed (see below).";
    }

    // RatioChoice loses a binding on its `selected` as soon as it is clicked, so
    // the values are written here and this runs whenever the installer changes
    // them on its own, which is what happens when the graphics card answers
    // after the window is already open.
    function syncEngineInputs() {
        engineVariantInput.selected = engine.variant === "nvidia" ? "NVIDIA" : "CPU";
        engineModelInput.selected = engine.model === "htdemucs" ? "fp32" : "fp16";
    }

    function load() {
        stemsEnabled = Mixxx.Core.stemGenerator.enabled;
        writeNextToTrack = Mixxx.Core.stemGenerator.writeNextToTrack;
        pauseWhilePlaying = Mixxx.Core.stemGenerator.pauseDuringPlayback;
        pythonPath = Mixxx.Core.stemGenerator.pythonPath;
        cacheDirectory = Mixxx.Core.stemGenerator.cacheDirectory;
        codecIndex = Math.max(0, codecs.indexOf(Mixxx.Core.stemGenerator.codec));
        modelIndex = Math.max(0, models.indexOf(Mixxx.Core.stemGenerator.model));
        providerIndex = Math.max(0, providers.indexOf(Mixxx.Core.stemGenerator.provider));
        Mixxx.Core.stemGenerator.probe();
        Mixxx.Core.stemGenerator.engine.refresh();
        syncEngineInputs();
    }

    function save() {
        Mixxx.Core.stemGenerator.enabled = stemsEnabled;
        Mixxx.Core.stemGenerator.writeNextToTrack = writeNextToTrack;
        Mixxx.Core.stemGenerator.pauseDuringPlayback = pauseWhilePlaying;
        Mixxx.Core.stemGenerator.pythonPath = pythonPath;
        Mixxx.Core.stemGenerator.cacheDirectory = cacheDirectory;
        Mixxx.Core.stemGenerator.codec = codecs[codecIndex];
        Mixxx.Core.stemGenerator.model = models[modelIndex];
        Mixxx.Core.stemGenerator.provider = providers[providerIndex];
        root.dirty = false;
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 10

        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.pixelSize: 14
            font.weight: Font.DemiBold
            text: "Stem generation"
        }
        Text {
            Layout.fillWidth: true
            color: Theme.midGray
            text: "Creates a Native Instruments style .stem.mp4 file with four stems (drums, bass, other, vocals) so the track can be played and remixed stem by stem. Add tracks from the library context menu: right click a selection and choose Stems > Generate stems. The jobs run one at a time and, unless disabled below, are postponed while a deck is playing."
            wrapMode: Text.WordWrap
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.darkGray3
        }

        // --- graphical hardware warning -------------------------------------
        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.pixelSize: 14
            font.weight: Font.DemiBold
            text: "Hardware"
        }
        Text {
            Layout.fillWidth: true
            color: Mixxx.Core.stemGenerator.gpuAvailable ? "#7bd88f" : "#ffb347"
            text: {
                if (Mixxx.Core.stemGenerator.probing)
                    return "Checking the environment...";
                if (Mixxx.Core.stemGenerator.gpuAvailable)
                    return "GPU detected" + (Mixxx.Core.stemGenerator.gpuName !== "" ? ": " + Mixxx.Core.stemGenerator.gpuName : "") + ". Separation runs on CUDA at roughly 10x real time, about 20 seconds for a four minute track.";
                return "No CUDA capable GPU found, so the separation would run on the CPU: it uses a lot of CPU time and takes much longer, roughly 2.4x real time, that is about 100 seconds for a four minute track (some five times slower than with a GPU). Running it while mixing may cause dropouts, so it is postponed while a deck is playing.";
            }
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            color: "#ffb347"
            text: Mixxx.Core.stemGenerator.probeErrors.length > 0 ? "Setup required:\n" + Mixxx.Core.stemGenerator.probeErrors.join("\n") : ""
            visible: Mixxx.Core.stemGenerator.probeErrors.length > 0
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            color: Theme.midGray
            text: "The worker needs a Python interpreter with demucs-onnx and onnxruntime-gpu installed, plus ffmpeg. Example:\n    python -m venv <folder>\\venv\n    <folder>\\venv\\Scripts\\pip install demucs-onnx onnxruntime-gpu\n    <folder>\\venv\\Scripts\\pip install nvidia-cublas nvidia-cuda-runtime nvidia-cuda-nvrtc nvidia-cufft nvidia-nvjitlink nvidia-cudnn-cu13\n    then set that interpreter below."
            visible: Mixxx.Core.stemGenerator.probeErrors.length > 0
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.darkGray3
        }

        // --- engine -----------------------------------------------------------
        Text {
            Layout.fillWidth: true
            color: Theme.white
            font.pixelSize: 14
            font.weight: Font.DemiBold
            text: "Engine"
        }
        Text {
            Layout.fillWidth: true
            color: Theme.midGray
            text: "The engine carries its own interpreter, its own libraries and its own model, so separating a track does not ask for anything to be installed on this computer. It is downloaded once into the folder below and can be removed at any time; nothing outside that folder is touched."
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            color: Mixxx.Core.stemGenerator.engine.gpuSuggested ? "#7bd88f" : Theme.midGray
            text: Mixxx.Core.stemGenerator.engine.gpuSuggested
                  ? "Graphics card found: " + Mixxx.Core.stemGenerator.engine.gpuName + ", so the CUDA build is offered by default."
                  : "No NVIDIA graphics card found, so the processor is what this computer can use."
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Processor"
            }
            Setting.RatioChoice {
                id: engineVariantInput

                options: ["CPU", "NVIDIA"]

                onSelectedChanged: {
                    const key = engineVariantInput.selected === "NVIDIA" ? "nvidia" : "cpu";
                    if (root.engine.variant !== key) {
                        root.engine.variant = key;
                    }
                }
            }
            Text {
                Layout.leftMargin: 12
                color: Theme.midGray
                text: root.engineVariantSize()
            }
            Item {
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Model quality"
            }
            Setting.RatioChoice {
                id: engineModelInput

                options: ["fp16", "fp32"]

                onSelectedChanged: {
                    const key = engineModelInput.selected === "fp32" ? "htdemucs" : "htdemucs_fp16weights";
                    if (root.engine.model !== key) {
                        root.engine.model = key;
                    }
                }
            }
            Text {
                Layout.leftMargin: 12
                color: Theme.midGray
                text: root.engineModelSize()
            }
            Item {
                Layout.fillWidth: true
            }
        }
        Text {
            Layout.fillWidth: true
            color: Theme.white
            text: "To download: " + Mixxx.Core.stemGenerator.engine.items.length + " files, "
                  + Mixxx.Core.stemGenerator.engine.totalSizeText + "."
            visible: Mixxx.Core.stemGenerator.engine.items.length > 0
            wrapMode: Text.WordWrap
        }
        ListView {
            id: engineList

            Layout.fillWidth: true
            Layout.preferredHeight: 150
            clip: true
            model: Mixxx.Core.stemGenerator.engine.items
            spacing: 2

            delegate: Rectangle {
                required property int index
                required property string label
                required property string sizeText
                required property string state

                color: index % 2 === 0 ? Theme.darkGray4 : Theme.darkGray2
                height: 24
                width: engineList.width

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        color: Theme.white
                        elide: Text.ElideRight
                        text: label
                    }
                    Text {
                        color: Theme.midGray
                        text: sizeText
                    }
                    Text {
                        color: state === "failed" ? "#ff8a80" : Theme.midGray
                        text: root.engineItemStateText(state)
                    }
                }
            }
        }
        ProgressBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 8
            from: 0
            to: 100
            value: Mixxx.Core.stemGenerator.engine.progress
            visible: Mixxx.Core.stemGenerator.engine.busy
        }
        Text {
            Layout.fillWidth: true
            color: Mixxx.Core.stemGenerator.engine.state === "failed" ? "#ff8a80" : Theme.white
            text: root.engineStateText()
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FormButton {
                text: Mixxx.Core.stemGenerator.engine.installed
                      ? "Reinstall or update the engine" : "Install the engine"
                enabled: !Mixxx.Core.stemGenerator.engine.busy
                         && Mixxx.Core.stemGenerator.engine.platformNotice === ""

                onClicked: Mixxx.Core.stemGenerator.engine.install()
            }
            FormButton {
                text: "Cancel"
                enabled: Mixxx.Core.stemGenerator.engine.busy

                onClicked: Mixxx.Core.stemGenerator.engine.cancel()
            }
            FormButton {
                text: "Remove the engine"
                enabled: !Mixxx.Core.stemGenerator.engine.busy
                         && Mixxx.Core.stemGenerator.engine.installed

                onClicked: Mixxx.Core.stemGenerator.engine.removeEngine()
            }
            FormButton {
                text: "Open the engine folder"

                onClicked: Mixxx.Core.stemGenerator.engine.openEngineFolder()
            }
            Item {
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.darkGray3
        }

        RowLayout {
            Layout.fillWidth: true

            Setting.RatioChoice {
                id: enabledInput

                options: ["on", "off"]

                onSelectedChanged: root.markDirty()
            }
            Text {
                color: Theme.white
                text: "Enable stem generation"
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Interpreter"
            }
            TextField {
                id: pythonPathInput

                Layout.fillWidth: true
                placeholderText: "python"
                text: root.pythonPath

                onTextEdited: {
                    root.pythonPath = text;
                    root.markDirty();
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Stem folder"
            }
            TextField {
                id: cacheDirectoryInput

                Layout.fillWidth: true
                placeholderText: "%1/stems".arg(Mixxx.Application.settingsDirectoryUrl)
                text: root.cacheDirectory

                onTextEdited: {
                    root.cacheDirectory = text;
                    root.markDirty();
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Setting.RatioChoice {
                id: nextToTrackInput

                options: ["on", "off"]

                onSelectedChanged: root.markDirty()
            }
            Text {
                Layout.fillWidth: true
                color: Theme.white
                text: "Write the stem file next to the track (otherwise it goes to the stem folder above)"
                wrapMode: Text.WordWrap
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Setting.RatioChoice {
                id: pauseInput

                options: ["on", "off"]

                onSelectedChanged: root.markDirty()
            }
            Text {
                Layout.fillWidth: true
                color: Theme.white
                text: "Pause generation while a deck is playing"
                wrapMode: Text.WordWrap
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Quality"
            }
            Skin.ComboBox {
                id: modelInput

                Layout.preferredWidth: 220
                model: ["htdemucs (fast)", "htdemucs_ft (best)"]

                onCurrentIndexChanged: root.markDirty()
            }
            Text {
                Layout.fillWidth: true
                color: Theme.midGray
                text: "The fast model is about 30% quicker and slightly less accurate."
                wrapMode: Text.WordWrap
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Format"
            }
            Skin.ComboBox {
                id: codecInput

                Layout.preferredWidth: 220
                model: ["aac (smallest)", "mp3 (most compatible)", "alac (lossless, big)"]

                onCurrentIndexChanged: root.markDirty()
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.preferredWidth: 220
                color: Theme.white
                text: "Processing device"
            }
            Skin.ComboBox {
                id: providerInput

                Layout.preferredWidth: 220
                model: ["auto", "cuda (GPU)", "cpu"]

                onCurrentIndexChanged: root.markDirty()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.darkGray3
        }

        // --- queue -----------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.pixelSize: 14
                font.weight: Font.DemiBold
                text: "Queue"
            }
            Text {
                color: Theme.midGray
                text: Mixxx.Core.stemGenerator.queueStatus
            }
        }
        ListView {
            id: queueList

            Layout.fillWidth: true
            Layout.preferredHeight: 200
            clip: true
            model: Mixxx.Core.stemGenerator.queue
            spacing: 2

            delegate: Rectangle {
                required property int index
                required property string title
                required property string status
                required property int progress
                required property int state
                required property string error

                color: index % 2 === 0 ? Theme.darkGray4 : Theme.darkGray2
                height: 34
                width: queueList.width

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        color: state === 3 ? "#ff8a80" : Theme.white
                        elide: Text.ElideRight
                        text: title
                    }
                    Text {
                        color: Theme.midGray
                        elide: Text.ElideRight
                        text: error !== "" ? error : status
                    }
                    ProgressBar {
                        Layout.preferredWidth: 120

                        from: 0
                        to: 100
                        value: progress
                        visible: state === 1 || state === 2
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FormButton {
                text: "Apply"

                onClicked: root.save()
            }
            FormButton {
                text: "Probe setup"

                onClicked: Mixxx.Core.stemGenerator.probe()
            }
            FormButton {
                text: "Open stem folder"

                onClicked: Mixxx.Core.stemGenerator.openCacheFolder()
            }
            FormButton {
                text: "Rescan library"

                onClicked: Mixxx.Core.stemGenerator.rescanLibrary()
            }
            FormButton {
                text: "Clear finished"

                onClicked: Mixxx.Core.stemGenerator.clearFinished()
            }
            FormButton {
                text: "Cancel all"

                onClicked: Mixxx.Core.stemGenerator.cancel()
            }
        }
    }

    Connections {
        target: Mixxx.Core.stemGenerator.engine

        function onSelectionChanged() {
            root.syncEngineInputs();
        }
    }
}
