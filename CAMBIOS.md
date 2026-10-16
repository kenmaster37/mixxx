# Cambios de este fork respecto al Mixxx original

- **Repositorio:** <https://github.com/kenmaster37/mixxx>
- **Base:** Mixxx 2.7.0-alpha, commit `0fffd3d0d0`
  (*«Merge pull request #17253 from xARSENICx/fix/latenightqml-animations»*).
- **Documentado:** 13-10-2026.
- **Detalle sesión por sesión:** [`PENDIENTE.md`](PENDIENTE.md) (bitácora interna, sin limpiar;
  está en el repositorio a propósito para no perder el porqué de cada decisión).

## Qué es este fork

Un build personal de Mixxx pensado para pinchar con un **Numark Mixtrack Pro II**, con dos
skins QML propias de estilo VirtualDJ/Serato, AutoDJ con IA, servidor OSC, un panel que sugiere
la siguiente pista según la tonalidad y un **asistente de dispositivo de audio** que detecta la
mesa al conectarla o desconectarla en caliente.

No busca ser aceptado en upstream: son cambios de proyecto (skins propias, flujo de trabajo
personal). Se mantienen separados para poder rehacer el merge sobre `main` cuando se quiera.

## Resumen por área

| Área | Cambio | Dónde |
| --- | --- | --- |
| **Skins propias** | `PulseDeck` y `NeonDeck`: skins QML completas (layout tipo VDJ/Serato, color por deck, tipografía Rajdhani, iconos Font Awesome) | `res/skins/PulseDeck/`, `res/skins/NeonDeck/` |
| **Fuentes** | Rajdhani (SIL OFL) y Font Awesome Free 6.7.2 (SIL OFL / CC BY 4.0 / MIT), con sus licencias | `res/fonts/` |
| **Carga de skins QML** | Las skins QML se cargan sin `--developer`; `NeonDeck` es la skin por defecto | `src/main.cpp`, `src/skin/skinloader.{h,cpp}` |
| **Estilo oscuro nativo** | Hoja de estilo por defecto para widgets nativos (biblioteca, menús, diálogos) | `src/skin/defaultstylesheet.{h,cpp}`, `src/mixxxmainwindow.cpp` |
| **Panel «Track Advisor»** | Sugiere la siguiente pista compatible armónicamente, bajo los samplers | `src/qml/qmllibraryproxy.{h,cpp}`, `src/skin/skincontrols.{h,cpp}`, `res/skins/*/Advisor/TrackAdvisor.qml` |
| **AutoDJ armónico** | Selección por tonalidad/BPM con degradación progresiva; tests | `src/library/autodj/harmonicmixing.{h,cpp}`, `autodjcratesdao.{h,cpp}`, `autodjfeature.{h,cpp}` |
| **AutoDJ con IA** | Planner compatible OpenAI/DeepSeek y barra de instrucción en AutoDJ; clave leída de forma asíncrona (QtKeychain) | `src/ai/aimixplanner.{h,cpp}`, `src/preferences/dialog/dlgprefai.*`, `dlgautodj.{h,cpp}` |
| **Servidor OSC** | Servidor OSC con página de preferencias | `src/controllers/osc/oscserver.{h,cpp}`, `src/preferences/dialog/dlgprefosc.*` |
| **Asistente de audio (estilo VDJ)** | Diálogo al conectar/desconectar un controlador, cambio de salida en caliente y elección recordada | `src/soundio/audiodeviceassistant.{h,cpp}`, `src/util/systemdevices.{h,cpp}` |
| **Audio: sin mesa** | Si el dispositivo configurado no está conectado, suena el dispositivo por defecto del sistema; en Windows no se cierra la app si la frecuencia no es compatible | `src/soundio/soundmanagerconfig.{h,cpp}`, `soundmanager.{h,cpp}`, `sounddevice*.{h,cpp}`, `portaudioenumerator.{h,cpp}` |
| **Pantalla completa (Linux)** | La acción de pantalla completa afecta a la ventana que muestra la skin QML | `src/qml/qmlapplication.{h,cpp}`, `src/qml/qmlapplicationproxy.h`, `src/mixxxmainwindow.cpp` |
| **Atajos de teclado** | El *fallback* de `en_US.kbd.cfg` buscaba la ruta mal: nadie tenía atajos. Y al quedar activos, escribir en la caja de la IA del AutoDJ disparaba los comandos del deck | `src/controllers/keyboard/keyboardeventfilter.{h,cpp}`, `src/qml/qmllegacylibraryitem.cpp` |
| **Controlador Mixtrack Pro II** | Scripts y mapeados sin controles inexistentes y sin el `abort()` del timer del plato | `res/controllers/Numark Mixtrack Pro.midi.xml`, `Numark-Mixtrack-{2,Pro}-scripts.js` |
| **Interfaz de los decks** | Pads de hotcue que tapaban los ajustes del deck; botones muertos; botón sándwich ilegible con el menú abierto | `res/skins/*/Deck/*`, `res/skins/*/Toolbar/Toolbar.qml`, `res/qml/Deck/BeatJump.qml` |
| **Empaquetado** | Empaquetado `.msi` y `.deb`, y alternativa de FFmpeg `-extra` para el `.deb` | `packaging/CPackConfig.cmake`, `packaging/CPackDebFfmpegExtraAlternatives.cmake` |

## 1. Skins QML propias: `PulseDeck` y `NeonDeck`

Ambas parten de `LateNightQML` y están reescritas como skins planas y oscuras, con el aspecto
de una consola moderna: color por deck (A cian / B rosa) con las ondas a juego, anillos de color
en los platos, rejilla de pads a la derecha del navegador, FX en los bordes y tipografía propia.

- `res/skins/PulseDeck/` y `res/skins/NeonDeck/`: skin completa (deck compacto/mixi/full, mixer,
  efectos, sampler, mic/aux, biblioteca, toolbar y panel Advisor).
- `res/skins/NeonDeck/Controls/IconLabel.qml`: dibuja un glifo de Font Awesome como si fuera texto
  (`LateNightTheme.iconGlyph(name)`), lo que permite colorear los iconos según el estado.
- `src/main.cpp`: se quitó la condición que exigía `--developer` para cargar skins QML.
- `src/skin/skinloader.{h,cpp}`: `skinFromDirectory()` descubre las skins QML como cualquier otra,
  `getDefaultSkinName()` devuelve `NeonDeck`.
- `src/skin/defaultstylesheet.{h,cpp}` + `src/mixxxmainwindow.cpp`: hoja de estilo propia para los
  widgets nativos (el árbol no usa el look de `LateNight`).
- `res/skins/LateNightQML/**`: ajustes menores compartidos (botones de beat, `SkinControlBootstrap`,
  toolbar y atajos del menú de aplicación).

⚠️ **Trampa de los assets:** `lateNightAsset()` arma las rutas como
`../../LateNight/<ColorScheme.name>/...`, y en `res/skins/LateNight/` sólo existen `classic` y
`palemoon`. Si un esquema declara otro `name`, **no se carga ningún SVG**. En NeonDeck se mapea
`neondeck → palemoon` en su `LateNightTheme.qml`.

## 2. Panel «Track Advisor»

Panel plegable (por defecto **apagado**) que lista las pistas de la biblioteca que mezclan
armónicamente con la que suena, bajo la rejilla de samplers; la biblioteca se desplaza en vez de
quedar tapada. Al hacer clic carga la pista en el deck que no sirve de referencia.

- `QmlLibraryProxy::findCompatibleTracks()` (nuevo, `src/qml/qmllibraryproxy.{h,cpp}`) reutiliza
  `mixxx::HarmonicMixing` y devuelve una lista de mapas lista para QML.
- Control `[Skin] show_track_advisor` (`src/skin/skincontrols.{h,cpp}`), botón **ADVISOR** en la
  barra y *Vista → Show Track Advisor* (**Ctrl+7**) en `src/widget/wmainmenubar.{h,cpp}`.
- QML: `res/skins/{NeonDeck,PulseDeck}/Advisor/TrackAdvisor.qml` y su sitio en `MainWindow.qml`.

## 3. AutoDJ armónico y AutoDJ con IA

- `src/library/autodj/harmonicmixing.{h,cpp}` (nuevo): círculo de quintas sobre el enum
  `ChromaticKey`, con la tolerancia de BPM `[Auto DJ] HarmonicBpmTolerance`; acepta también las
  ventanas de tempo mitad y doble. Si ninguna candidata cumple, degrada **primero** a compatibilidad
  de tonalidad y sólo después a aleatorio (antes se perdía la armonía con bibliotecas concentradas
  en un rango estrecho de BPM).
- `src/library/dao/autodjcratesdao.{h,cpp}`: `getRandomTrackIdHarmonic()`.
- `src/library/autodj/autodjfeature.{h,cpp}` y `dlgautodj.{h,cpp}`: barra de instrucción en lenguaje
  natural y planificador creado de forma perezosa.
- `src/ai/aimixplanner.{h,cpp}` (nuevo): cliente compatible con OpenAI/DeepSeek. **La clave de API
  no está en el repositorio**: se guarda en preferencias y se lee con QtKeychain de forma asíncrona
  (`ReadPasswordJob`), igual que hace la página `DlgPrefAi`.
- `src/preferences/dialog/dlgprefai.{h,cpp,.ui}` (nuevo) y `dlgprefautodj.*` (grupo *Harmonic
  Mixing*), registrados en `dlgpreferences.cpp`.

## 4. Servidor OSC

`src/controllers/osc/oscserver.{h,cpp}` (nuevo) se crea y arranca desde `CoreServices` (`m_pOscServer`),
con su página de preferencias `dlgprefosc.{h,cpp,.ui}`. Se usa para controlar Mixxx desde fuera
(por ejemplo, para medir CPU y para automatizar pruebas).

## 5. Asistente de dispositivo de audio (estilo VirtualDJ)

**Qué hace:** cuando se conecta o desconecta un controlador mientras Mixxx está abierto, aparece un
diálogo que **nombra el dispositivo reconocido** y deja elegir, y la salida de audio cambia en
caliente sin reiniciar. Si el dispositivo que estaba en uso desaparece, avisa y pasa al audio del
ordenador. La elección se puede recordar (`[Sound] controller_audio_device` = `mixer` | `computer`).
La misma acción está en el menú (*Vista → Audio Setup Assistant*, `Ctrl+8`) y, al usarla, **siempre
pregunta** aunque haya una elección recordada; si no hay ninguna mesa, lo dice.

- `src/soundio/audiodeviceassistant.{h,cpp}` (nuevo): vigilante cada 2 s, diálogos, elección de la
  API y de los canales (`Main` en los canales 1-2 y, con 4 o más canales, `Headphones` en 3-4).
- `src/util/systemdevices.{h,cpp}` (nuevo): lista **barata** de dispositivos del sistema (MIDI con
  `winmm` en Windows, `/proc/asound/cards` en Linux). No toca el motor de audio ni el backend de
  controladores, por eso se puede consultar cada 2 s.
- Al detectar un cambio se reenumera de verdad (`SoundManager::reinitializeDevices()`: cierra los
  streams, `Pa_Terminate`/`Pa_Initialize` y vuelve a casar la configuración), lo que **corta el audio
  un instante** de forma inevitable: PortAudio sólo enumera al arrancar. Se intentó evitarlo con
  reintentos y con preferencia forzada de WASAPI y **ambos se revirtieron** (más cortes y lag; ver
  `PENDIENTE.md` §6.15).
- El controlador se reconoce **por nombre** (el nombre MIDI comparado con el de las salidas,
  normalizado a letras y dígitos en minúscula: `MixTrack Pro II` casa con
  `Altavoces (MixTrack Pro II)`), así que sirve con cualquier marca.
- El asistente **respeta la API configurada**: si el dispositivo aparece en la API de la
  configuración, la usa; si no, recorre el orden natural de `getHostAPIList()`. No fuerza WASAPI ni
  degrada a MME.
- Cableado: `src/qml/qmlapplication.{h,cpp}` (propietario del asistente en los builds QML),
  `src/qml/qmlapplicationproxy.h` (`Mixxx.Application.checkAudioDevices()`),
  `src/mixxxmainwindow.{h,cpp}` (builds sin QML), `src/widget/wmainmenubar.{h,cpp}` y las entradas de
  menú de las tres skins (`Toolbar/Toolbar.qml`, `Toolbar/ApplicationMenuShortcuts.qml`,
  `res/qml/ApplicationMenuActions.qml`, `res/qml/MainMenuBar.qml`).
- `CMakeLists.txt`: fuentes nuevas y enlace de `winmm` (no lo hereda el ejecutable desde la
  biblioteca estática).

**Limitación conocida:** si Mixxx arranca **con la mesa desconectada** y se enchufa después, el audio
cambia pero los botones no responden hasta reiniciar Mixxx: `PortMidiEnumerator` sólo enumera
dispositivos al inicializarse. El intento de arreglarlo (un `refreshHostDevices()` con
`Pm_Terminate` + `Pm_Initialize`) **crasheaba** por objetos obsoletos del build incremental y se
revirtió; para reintentarlo hace falta `cmake --build . --clean-first` después de tocar la clase base
y sincronizar con `dlgprefcontrollers` (ver `PENDIENTE.md` §6.15).

## 6. Audio: qué pasa cuando no hay mesa

- `SoundManagerConfig::validateDevices()`: si no queda ningún dispositivo de salida conectado, añade
  la salida del **dispositivo por defecto del sistema** para que Mixxx siga sonando en vez de quedarse
  mudo, marcando el añadido como temporal para que **no se guarde** en `soundconfig.xml` y para que la
  elección del usuario se conserve y se recupere al reconectar.
- `SoundManagerConfig::addOutput()`/`addInput()` quitan esa marca temporal al añadir un dispositivo,
  de modo que lo que elige el usuario (o el asistente) **sí** se guarda. Sin esto, la elección se
  perdía.
- **Negociación de frecuencia:** `SoundDevice::isSampleRateSupported()` (en PortAudio con
  `Pa_IsFormatSupported`) y `SoundManagerConfig::checkSampleRate()` prueban la frecuencia configurada,
  luego la del dispositivo y luego la lista de la API. En Windows, pedir una frecuencia que el
  endpoint no acepta hacía que Mixxx mostrara un error y se cerrara.
- `SoundManager::reinitializeDevices()`, `getDefaultOutputDevice()`, `getDevice()`,
  `getSubstitutedDevices()`, y `SoundDeviceEnumerator::getDefaultOutputDevice(api)`.

## 7. Pantalla completa en Linux

La acción de pantalla completa apuntaba a la ventana `QWidget` oculta de `MixxxMainWindow`, mientras
que las skins QML se dibujan en la ventana que crea `QQmlApplicationEngine`, por lo que no hacía nada.
Ahora `QmlApplication` guarda su ventana y `QmlApplicationProxy::setWindowFullScreen()` la aplica;
`MixxxMainWindow::slotViewFullScreen()` la usa primero.

## 8. Atajos de teclado

`src/controllers/keyboard/keyboardeventfilter.cpp`: el *fallback* a `en_US.kbd.cfg` pasaba
`resourcePath` (que termina en `res/`) en lugar de `resourcePath + "keyboard/"`, así que buscaba
`res/en_US.kbd.cfg` y terminaba con `starting without shortcuts`: **el usuario no tenía ningún atajo**.
Arreglo de una línea; verificado con Ctrl+P.

### 8.1 Escribir en el panel AutoDJ disparaba los atajos

Al quedar activos los atajos apareció un efecto secundario: en la caja de instrucciones de la IA
(panel AutoDJ) **no se podía escribir**, y cada letra movía el crossfader, arrancaba la reproducción
o disparaba hotcues. Los 26 mapeos de una sola letra de `en_US.kbd.cfg` (`h`/`g` crossfader, `d` play,
`f` cue, `z`-`v`/`m` hotcues, `q`/`w`/`e`/`u`/`i`/`o` loops, `a`/`s`/`j`/`k` beatjump, `p` cargar
pista, `t`/`y` PFL, `b`/`n` parámetro, `r` editar, `l` play) chocaban con la escritura.

Causa: el árbol de widgets de la biblioteca se **pinta** dentro de un item de QML
(`mixxx::qml::QmlLegacyLibraryItem`) y vive **fuera de pantalla** (`WA_DontShowOnScreen`), con los
clics y las teclas reenviados a mano. El `KeyboardEventFilter` está instalado en toda la aplicación
(`app->installEventFilter(...)`), así que ve las teclas antes que nadie, y su guarda para no pisar la
escritura era `isEditableTextInput(QGuiApplication::focusObject())`, que **no puede ver** un widget que
no está en la jerarquía de ventanas: devuelve la ventana QML. Con la caja enfocada, el filtro seguía
tratando las letras como atajos y además **se comía la tecla**.

Arreglo, siguiendo el patrón de *callback* que ya usa el proyecto:

- `KeyboardEventFilter::isEditableTextInput()` pasa a ser método estático público.
- `KeyboardEventFilter::registerEmbeddedTextInputFocusQuery(std::function<bool()>)`: quien pinta
  widgets fuera de pantalla registra una consulta, y la guarda del filtro pasa a
  `isEditableTextInput(focusObject()) || (s_embeddedTextInputFocusQuery && s_embeddedTextInputFocusQuery())`.
- `QmlLegacyLibraryItem` la registra en su constructor (y la limpia en el destructor) devolviendo
  `isEditableTextInput(m_pFocusedWidget)`, que es el widget que `updateEmbeddedFocus()` asigna en cada
  clic. Así el estado se lee en vivo y no hay nada que se desincronice.

Con eso, escribir en la caja funciona y los atajos siguen activos al volver el foco a la tabla de
pistas (un clic en la tabla deja el viewport, que no es un campo de texto). Verificado por el usuario
en pantalla.

### 8.2 La barra espaciadora activaba «BIG LIBRARY» al escribir

Quedó un cabo suelto del apartado anterior: ya se podía escribir, pero **el espacio activaba
«BIG LIBRARY»**, porque el espacio *es* el atajo de *Maximize Library* (decisión de upstream, está en
`res/qml/ApplicationMenuActions.qml` y en `res/keyboard/en_US.kbd.cfg`).

El filtro de teclado no era el único camino que había que tapar:

- los `Shortcut` de QML (`Toolbar/ApplicationMenuShortcuts.qml`) los resuelve Qt **antes** de entregar
  la tecla al item, así que no pasan por `KeyboardEventFilter`;
- y las `Action` de `ApplicationMenuActions.qml` son de **QtQuick**, que **registra el atajo de la
  propia Action** por su cuenta (el skin instancia `MainMenuBar`, así que están en un menú). Bloquear
  sólo el `Shortcut` no bastaba —se probó y seguía activándose—.

Solución, con una única fuente de verdad:

- `KeyboardEventFilter::isTextInputFocused()`, que usa el propio filtro y que se expone a QML como la
  propiedad `Mixxx.Application.textInputFocused` (con `NOTIFY` desde `focusObjectChanged` y desde un
  aviso que manda `QmlLegacyLibraryItem` cuando cambia el foco de sus widgets pintados).
- El bloqueo en `configuredMenuShortcut()`, el único punto donde se definen los 32 atajos del menú:
  mientras se escribe devuelve `""`, y eso desarma a la vez el atajo de la Action y el `Shortcut` de
  QML. Al salir el foco de la caja, el binding se reevalúa y los atajos vuelven.

## 9. Controlador Numark Mixtrack Pro II

- `res/controllers/Numark Mixtrack Pro.midi.xml`: los grupos `[Flanger]` y `[ChannelN],flanger` no
  existen en Mixxx 2.x; los knobs y LED de FX ahora apuntan a
  `[EffectRack1_EffectUnitN_Effect1]`. El script usa `fxSlotForDeck(deck)`.
- `res/controllers/Numark-Mixtrack-Pro-scripts.js`: el timer llamaba a `jogWheelStopScratch()` **sin
  el deck**, y `scratchDisable(undefined)` acababa en `groupForDeck(-1)`, que falla una aserción
  (`DEBUG_ASSERTIONS_FATAL`) y **cerraba Mixxx**. Se pasa el deck y se añade una guarda en los dos
  scripts del Mixtrack.
- Nota de uso: el plato sólo hace *scratch* con el deck sonando si su modo vinyl/scratch está
  encendido (nota `0x48`/`0x50`, con LED); apagado es *nudge*, como un CDJ.
- Los tests `src/test/{neondeckskin,pulsedeck_skin,harmonicmixing,aimixplanner}_test.cpp` cubren la
  carga de las skins y la lógica de selección.

## 10. Interfaz de los decks y del menú

- Los números de los pads de hotcue tapaban los ajustes del deck (estrellas, slip, quantize, curpos,
  eject, repeat, keylock): `deckSettingsTopOffset` en `res/skins/*/Deck/OverviewRow.qml` (y el panel de
  fondo a juego) desplaza el bloque cuando los pads están visibles.
- Botones muertos: `beatloop_size_double/halve` y `beatjump_size_halve/double` no existen (los reales
  son `loop_double`/`loop_halve`), así que el *spin box* de tamaño no avanzaba.
  Ver `res/qml/Deck/BeatJump.qml` y los `TransportLoopBeatjump*` de las tres skins.
- El botón sándwich del menú se ponía gris sin que se vieran las líneas: el icono era el SVG claro
  `btn__menu.svg` sobre el fondo gris del menú abierto. Ahora se dibuja con la fuente de iconos y se
  colorea según el estado (`PulseDeck` y `NeonDeck`; en `LateNightQML` sigue igual porque ese tema no
  tiene la fuente de iconos).

## 11. Empaquetado

- `packaging/CPackConfig.cmake`: build y empaquetado del `.msi` (MSVC + Ninja + WiX) y del `.deb`.
  El `wix.exe` global es un *tool* de .NET y necesita `DOTNET_ROOT` apuntando al SDK.
- `packaging/CPackDebFfmpegExtraAlternatives.cmake` (nuevo): permite las bibliotecas de FFmpeg
  `-extra` como alternativas en las dependencias del `.deb`.

Los scripts de build locales (`build_windows_release.cmd`, `CMakeSettings.json`) están en el
`.gitignore` del proyecto, así que **no se incluyen**: cada uno arma su propio comando con los
datos de la sección 12.

## 12. Cómo compilar y verificar

```powershell
# Windows (MSVC + Ninja + vcpkg de buildenv), desde una consola con vcvars64
cmake --build C:\ruta\build\x64__legacy --config RelWithDebInfo
# y para el instalador:
cpack -G WIX -C RelWithDebInfo
```

```bash
# Linux
cmake --build build --config RelWithDebInfo --parallel
cpack -G DEB
```

⚠️ **Nunca interrumpir una compilación** y, tras cambiar un *header* que forma parte de una clase,
recompilar limpio (`--clean-first`) si aparecen crashes que cambian de sitio: un `.obj` viejo con el
tamaño antiguo de la clase provoca corrupción de memoria (en `PENDIENTE.md` §2.1 está el caso real).

⚠️ **Al empaquetar el `.msi`, llamar `vcvars64.bat` en una línea aparte.** vcpkg copia las DLLs
caminando el grafo de imports del ejecutable y necesita `dumpbin` en el `PATH`. Si se encadena todo
en una sola línea de `cmd` —

```bat
rem MAL: cmd expande %PATH% antes de que vcvars64 corra, y el set pisa el PATH
cmd /c "call vcvars64.bat >nul && set "PATH=<cmake>;<ninja>;%PATH%" && cpack -G WIX"
```

— el `set` escribe el `PATH` **anterior** a vcvars y borra el directorio de herramientas de MSVC
(donde viven `dumpbin.exe` y `cl.exe`). La compilación no falla (CMake cachea las rutas absolutas de
`cl.exe` dentro de `build.ninja`), pero el script de vcpkg sí busca `dumpbin` en el `PATH` y avisa
sólo con un `resolve : Neither dumpbin, llvm-objdump nor objdump could be found`; CPack termina
igual y **genera un MSI sin ninguna DLL**, que se instala bien y falla al abrir con
`0xC0000135` (falta una DLL). Síntoma para reconocerlo: el MSI pesa mucho menos de lo normal
(~74 MB en vez de ~115 MB). Conviene poner `where dumpbin || exit /b 1` antes de `cpack`.
Detalle completo en `PENDIENTE.md` §6.17.

## 13. Notas

- **Estilo, no arte copiado:** el objetivo visual es el *estilo* de Serato/VirtualDJ. Todos los
  recursos son originales o con licencia libre (Rajdhani y Font Awesome Free, SIL OFL / CC BY 4.0 /
  MIT, con sus ficheros de licencia en `res/fonts/`). No se copia ningún fichero gráfico ni logotipo
  de VirtualDJ/Serato.
- **Claves de API:** no hay ninguna en el repositorio. La del AutoDJ con IA se guarda en las
  preferencias del usuario (bóveda del sistema vía QtKeychain).
- **`PENDIENTE.md`** es la bitácora de trabajo: contiene rutas locales, capturas de log y el porqué de
  cada decisión. Sirve de respaldo, no como documentación para terceros.

---

## 12. Generacion de stems (pestana Stems)

Mixxx ya sabia **reproducir** stems, pero no crearlos. Ahora tambien los genera:

- **Worker externo** `res/stems/stems_worker.py` (+ `stem_mp4.py`): separa con
  `demucs-onnx` (ONNX Runtime, sin PyTorch) y escribe el contenedor Native
  Instruments: 5 pistas (la mezcla y los cuatro stems) y el manifiesto JSON en
  `moov/udta/stem`, con los `stco`/`co64` parcheados porque el `moov` crece. El
  progreso se publica como lineas (`STATUS`, `PROGRESS`, `RESULT`, `DONE`,
  `ERROR`) y el subcomando `probe` informa del entorno para poder avisar antes de
  empezar.
- **Servicio** `src/stems/stemgenerator.{h,cpp}`: cola de un trabajo a la vez,
  pospuesta mientras hay musica sonando, lanzada con QProcess, con las DLL de
  CUDA de los wheels de Python anadidas al PATH y salida junto a la pista (por
  defecto) o en `<config>/stems/<hash>/`. La cola se muestra con
  `src/stems/stemqueuemodel.{h,cpp}`.
- **Interfaz**: pestana **Stems** en Preferencias (`res/qml/Settings/Stems.qml`)
  con el aviso de hardware y la cola con progreso, y entradas
  *Stems > Generate stems / Regenerate stems* en el menu contextual de la
  biblioteca (WTrackMenu).

Aviso que muestra la pestana: sin GPU la separacion se hace en CPU, consume mucho
mas y tarda unas cinco veces mas. Medido en el equipo de desarrollo (pista de
243 s, `htdemucs` fp32): **22.4 s con RTX 3060 (10.9x tiempo real)** frente a
**102.4 s en CPU (2.4x)**, con la suma de stems a -30.9 dB del original en ambos
casos.
---

# 14. Deck de PulseDeck tomado de la skin experimental (15-10)

- **Bug de arranque de las skins** (`Toolbar.qml` en PulseDeck, NeonDeck y LateNightQML): un
  `function onEnabledChanged()` dentro de un `Connections` choca con la propiedad `enabled` que
  `Connections` ya declara, y el compilador de QML rechaza el fichero entero ("Duplicate method
  name"), asi que **la skin no cargaba**. Sustituido por un `Binding` que espeja
  `stemGenerator.enabled` en el boton de stems.
- **Controles de tonalidad solapados**: el hueco de `KeyControlsPlaceholder` era elastico
  (76-111 px) y el `RowLayout` comprimia a sus hijos cuando la fila no cabia. Ahora el hueco es
  fijo (76 px) en los tres `FullDeck.qml` y en los tres `TransportLoopBeatjumpCompact.qml`, y los
  controles de vinilo (158 px de ancho fijo) tienen fila propia en PulseDeck o se ocultan
  mientras no caben en el resto de skins y en las filas compactas.
- **Deck de PulseDeck** (`res/skins/PulseDeck/Deck/FullDeck.qml`): es la composicion de
  `LateNightQML` (plato grande con las filas de titulo al lado, transporte debajo, pitch en el
  borde) manteniendo los adornos de PulseDeck (tira de acento del color del deck, color de deck
  en BPM y llave, iconos, insignia A/B/C/D) y el espejado de los decks B/D, anadido al deck
  copiado como `mirrorControls`. `fullDeckHeight` pasa de 230 a 206 px, que es lo que mide.
- **Boton de sync**: dice **SYNC** en vez de dibujar un glifo (`RatePlaceholder.qml` y
  `RateCompact.qml`), como VirtualDJ.
- **Worker de stems**: `StemGenerator::resolvedWorkerScript()` busca ahora tambien junto al
  ejecutable, en `<app>/../share/mixxx/stems/` y en el directorio de configuracion: Mixxx solo
  conoce la ruta de recursos si se le pasa `--resource-path`, y sin ella resolvia
  `stems_worker.py` relativo al directorio actual (de ahi el "No se encuentra .../stems_worker.py"
  al comprobar el entorno).
- **`wsl_rebuild_deb.sh`** en la raiz del repo: compila y empaqueta el `.deb` dentro de WSL
  reutilizando `/home/mixxx/mixxx/build` y copia el paquete a `dist\`.
- **Iconos del panel Track Advisor**: la skin experimental (`LateNightQML`) no tenia el mapa de
  glifos de Font Awesome en su tema, asi que el icono ✨ de la cabecera y el boton ⟳ de recargar
  **no se dibujaban** (sus `iconName` no resolvian). Se anadio el mapa (64 glifos) y la familia
  `iconFontFamily` a su tema. Ademas, el boton de recargar tiene ahora ancho minimo fijo en las
  tres skins, para que una cabecera estrecha no lo comprima hasta hacerlo desaparecer.
- **Columna Stems de la biblioteca**: quedaba al final porque la colocacion se marcaba como hecha
  aunque no hubiera podido moverse. Ahora la marca es una **version** (`stems_column_placed = 2`):
  los perfiles que pasaron por la regla anterior se colocan **una vez mas** y a partir de ahi se
  respeta donde la arrastre el usuario.
- **Panel STEMS de la barra**: pulsar el boton con el panel abierto **no lo cerraba** (volvia a
  abrirlo). Ahora usa el mismo ayudante que los desplegables de ajustes, con la marca
  `wasPopupOpenOnPress` en el boton para no reabrirlo con el clic que acaba de cerrarlo.
- **Entorno de Python de los stems**: se anaden `res/stems/requirements.txt` y
  `res/stems/setup_venv.py`, que crean el entorno en `<config>/stems/venv` (la carpeta que Mixxx
  consulta por defecto) e instalan ONNX Runtime y el separador. Antes no habia forma de instalarlo
  desde el repo.