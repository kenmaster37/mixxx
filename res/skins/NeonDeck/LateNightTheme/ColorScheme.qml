pragma Singleton
import QtQuick
import Mixxx 1.0 as Mixxx
import "ColorSchemes"

QtObject {
    readonly property string configScheme: Mixxx.Config.configScheme
    readonly property QtObject activeScheme: {
        const scheme = String(configScheme).toLowerCase();
        if (scheme === "classic") {
            return classicScheme;
        }
        if (scheme === "palemoon") {
            return paleMoonScheme;
        }
        // This skin is built around the NeonDeck palette, so it is also the
        // default when the configuration does not name a scheme, which is the
        // case on a fresh install.
        return neonDeckScheme;
    }

    readonly property color accentColor: activeScheme.accentColor
    readonly property string displayName: activeScheme.displayName
    readonly property string name: activeScheme.name

    readonly property QtObject paleMoonScheme: PaleMoon {}
    readonly property QtObject classicScheme: Classic {}
    readonly property QtObject neonDeckScheme: NeonDeck {}
}
