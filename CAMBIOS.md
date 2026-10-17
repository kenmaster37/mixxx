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

---

# 15. Motor de stems propio, instalable desde Preferencias

Generar stems exigia tener Python y las librerias puestas a mano. Ahora la pagina
**Stems** ofrece **descargar un motor completo** que lleva dentro su propio
interprete, ONNX Runtime, el separador, `ffmpeg` y el modelo, de modo que no hace
falta nada instalado en el equipo.

- **La pagina viva es C++, no QML**: `src/preferences/dialog/dlgprefstems.cpp` (Qt Widgets),
  registrada en `dlgpreferences.cpp`. El fichero `res/qml/Settings/Stems.qml` **no se muestra**
  (el QML va incrustado en el ejecutable y esa pagina es la de C++). Por eso "acorde al skin"
  significa usar los mismos widgets que el resto de la pagina (`QGroupBox`, `QFormLayout`,
  `QListWidget`, `QProgressBar`, `QPushButton`), no `Theme`/QML.
- **Instalador** `src/stems/stemengineinstaller.{h,cpp}` y `src/stems/ziparchive.{h,cpp}`: lee el
  manifiesto `res/stems/engine.json` (variantes CPU/CUDA y modelos con sus tamanos), comprueba la
  red **antes** de empezar host por host, descarga con reintentos **desde cero** (nunca reanuda un
  fichero cortado), verifica el `sha256`, descomprime los ZIP con zlib en rebanadas de 50 ms para
  no congelar la interfaz, escribe el `_pth` del interprete embebido (`../app`,
  `../site-packages`), copia el worker y el sustituto de `huggingface_hub`, y deja
  `installed.json` con version, variante y modelo.
- **Variante sugerida**: una sonda de GPU **asincrona** (nunca bloquea el arranque) deja puesta la
  variante CUDA cuando hay tarjeta NVIDIA, salvo que el usuario elija otra o ya haya motor
  instalado.
- **El generador prefiere el motor**: `StemGenerator::resolvedPython()`, `resolvedFfmpeg()` y
  `resolvedWorkerScript()` miran primero dentro de la carpeta del motor, y el `PATH` del trabajo
  incluye los `bin` de NVIDIA que vienen en los wheels.
- **Rutas de recursos**: se usa `m_pConfig->getResourcePath()` (la ruta ya resuelta, que en un
  arbol de desarrollo sale de `CMakeCache.txt`), no `CmdlineArgs::getResourcePath()`, que solo
  tiene valor si se pasa `--resource-path`. Con lo anterior, en un arbol de desarrollo la pagina
  no encontraba ni el manifiesto ni el worker.
- **Al terminar la instalacion** la pagina vuelve a sondear el entorno, para que la caja
  *Environment* y el estado del generador reflejen el motor recien instalado sin tener que cerrar
  y abrir Preferencias.
- **Aislamiento**: el paquete se llama, se instala y se registra aparte (`Mixxx Stem Engine` con
  GUID propio; `.deb` `mixxx-stem-engine` en `/opt/mixxx-stem-engine`), de modo que probarlo **no
  sustituye** un Mixxx instalado. Scripts de esta fase: `build-msi.cmd`, `package-msi.cmd`,
  `wsl-build-deb.sh` y `compile-check.cmd`.
- **Trampa del build que costo un rato**: `dlgprefstems.cpp` incluye su `moc_dlgprefstems.cpp` a
  mano y Ninja no siempre registra esa dependencia. Si se anaden miembros a `DlgPrefStems`, el
  objeto de `dlgpreferences.cpp` se queda con el **tamano viejo de la clase**: `new DlgPrefStems`
  reserva de menos y el constructor escribe fuera, con fallo de acceso en `Qt6Core.dll` al abrir
  Preferencias. Hay que borrar los `.obj` afectados (los que incluyen la cabecera) y recompilar.
- **Comprobado**: instalando desde Mixxx la variante CPU (237,9 MB, 16 ficheros) la carpeta del
  motor queda en 576,9 MB y su interprete embebido importa `onnxruntime`, `numpy`, `soundfile`,
  `soxr` e `imageio_ffmpeg` (el `ffmpeg` de 87 MB viaja dentro del wheel) sin ningun Python en el
  equipo.
- **Pendiente**: en Linux la pagina avisa y no ofrece instalar, porque el manifiesto es
  `win_amd64`; hace falta un manifiesto con los `.tar.gz` de python-build-standalone, otro
  `ffmpeg` y `chmod +x` al descomprimir.

## 16. El fallo al escribir el contenedor (WinError 2)

Sintoma reportado: la separacion avanza, llega a *encoding and writing the container* y termina
con `writing the container failed: [WinError 2] El sistema no puede encontrar el archivo
especificado`. No se generaba ningun `.stem.mp4`.

- **Causa**: `stem_mp4.read_metadata()` leia las etiquetas con **`ffprobe`**, un programa que el
  motor **no** trae (en `imageio_ffmpeg` solo viaja el codificador). Buscaba un `ffprobe` junto al
  `ffmpeg`, luego en el `PATH` y, si no lo encontraba, se quedaba con el nombre pelado `ffprobe`
  y lo lanzaba: eso es un `[WinError 2]`, y ocurria **despues** de la separacion, en el ultimo
  paso del trabajo. En el equipo de desarrollo no se veia porque hay un `ffprobe` del sistema
  (winget) en el `PATH`; en un portatil sin ffmpeg instalado, siempre.
- **Reproducido y comprobado**: con el `PATH` sin ffmpeg y la ruta de ffmpeg rota a proposito, el
  worker instalado da exactamente el error del usuario (salida 6); el mismo comando con el codigo
  arreglado termina en `DONE` y deja un `.stem.mp4` de 1,9 MB cuyo propio verificador acepta
  (5 pistas, manifiesto y tablas de offsets correctas).
- **Arreglo, en tres capas**: `read_metadata()` pide las etiquetas al propio `ffmpeg`
  (`-f ffmetadata`), que es el unico programa que el motor garantiza, y nunca deja caer un trabajo
  por las etiquetas; el worker resuelve el codificador con `resolve_ffmpeg()` (si lo que le llega
  no es un programa arrancable, se lo pide a `imageio_ffmpeg`, que sabe donde esta el suyo) y lo
  comprueba **antes** de separar, para no perder minutos de trabajo por una ruta; y
  `StemGenerator::resolvedFfmpeg()` ya no propaga una ruta inexistente.
- **El primer sospechoso no era el culpable**: `StemEngineInstaller::ffmpegFor()` devolvia el
  primer fichero de `binaries` que no fuese `.txt`, y ahi `README.md` va antes que
  `ffmpeg-win-x86_64-v7.1.exe`. Se corrijo igual (busca por el prefijo `ffmpeg`), pero un `.md`
  no da error 2 sino **193** (`no es una aplicacion Win32 valida`), asi que no era el fallo
  reportado.
- **El worker ahora viaja con la aplicacion**: `resolvedWorkerScript()` prefiere la copia de
  `share/mixxx/stems/` antes que la que quedo dentro del motor. Antes, un arreglo del worker solo
  llegaba al usuario volviendo a descargar 240 MB de motor; ahora basta con instalar el Mixxx
  nuevo. Para que el worker funcione desde cualquiera de las dos carpetas, la aplicacion le pasa
  `STEM_ENGINE_MODELS` y el sustituto de `huggingface_hub` sabe resolver la carpeta `models` por
  la variable, por su propia ubicacion o por la del interprete.
- **Paquete de pruebas**: el MSI se empaqueta con `CPACK_PACKAGE_VERSION=2.7.1` (el proyecto es
  2.7.0) porque con el mismo GUID de actualizacion y la misma version Windows rechaza instalar
  encima con el error 1638.
- **El `.deb` de pruebas ahora se puede lanzar y desinstalar**: como se instala bajo `/opt`, no
  habia nada en el `PATH` ni en el menu, y parecia que "no se instalaba" (`mixxx` no era un
  comando) ni se podia quitar (`apt remove mixxx` no es el nombre del paquete). El paquete aislado
  lleva `packaging/isolated-deb/{postinst,postrm}` (pasados con `CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA`
  desde `wsl-build-deb.sh`): el `postinst` crea `/usr/local/bin/mixxx-stem-engine` y una entrada de
  menu propia —`Mixxx (motor de stems, pruebas)`, con `Exec` e `Icon` absolutos—, y el `postrm` la
  retira. El paquete oficial no usa nada de esto.
- **Detalle del `sed` del `postinst`**: los patrones no van anclados al final de la linea
  (`^Name=Mixxx.*`) porque el `.desktop` del repositorio sale con retorno de carro en Windows y un
  `$` anclado no casa con nada.

## 17. Biblioteca y panel lateral borrosos con escalado fraccionario

Sintoma reportado: en un portatil de 1080p con la pantalla escalada (el otro equipo, a 100 %, se ve
bien) **la tipografia de la biblioteca y del panel lateral** (arbol de fuentes, pistas, AutoDJ) se
ve borrosa, mientras la piel QML y las ondas se ven nitidas.

- **Causa**: en las pieles QML la biblioteca es un arbol de **Qt Widgets** que no se dibuja en
  pantalla, sino que se **pinta** dentro de un item de QML (`QmlLegacyLibraryItem`, con
  `WA_DontShowOnScreen`). `renderOffscreen()` creaba el `QPixmap` de **un pixel por pixel logico**
  (`QPixmap(size)`, sin `devicePixelRatio`) y `paint()` lo dibujaba tal cual. Con la ventana al 150 %
  el escenario amplia ese mapa 1,5 veces: la biblioteca se ve como una imagen magnificada, y solo
  ella, porque el resto de la ventana lo dibuja el motor de QML a resolucion nativa. A 100 % no hay
  ampliacion y por eso el otro equipo se ve bien.
- **Arreglo**: el mapa se crea del tamano del **dispositivo** (`ceil(lado * devicePixelRatio)`) con
  su `devicePixelRatio` puesto, de modo que el pintor dibuja los widgets a la resolucion de la
  pantalla y `paint()` lo pone pixel por pixel del dispositivo, sin magnificar. La disposicion de los
  widgets sigue en pixeles logicos (los clics siguen cuadrando porque `QQuickItem` entrega las
  posiciones en esas mismas unidades). Se anade `itemChange(ItemDevicePixelRatioHasChanged)` para
  volver a pintar al mover la ventana a una pantalla con otra escala.
- **Comprobado**: con `QT_SCALE_FACTOR=1.5`, mismo perfil, instantaneas del mismo recorte del arbol
  ampliadas 4 veces: antes los trazos salen dobles y con flecos de color (mapa ampliado), despues
  limpios. Con el escalado a 100 % el resultado es identico al de antes del cambio.

## 18. El motor en Linux

Hasta aqui la pagina de Stems avisaba en Linux y no ofrecia instalar, porque el manifiesto era
`win_amd64`. Ahora hay una lista de ficheros por plataforma.

- **Manifiesto nuevo** `res/stems/engine-linux.json`, generado con `tools/make_manifest.py`
  (la misma herramienta del laboratorio, con tres cambios):
  - acepta **varias etiquetas `--platform`**, porque pip no expande una familia de `manylinux` en
    las demas y estos paquetes no coinciden en ninguna: numpy publica `manylinux_2_28`,
    `imageio-ffmpeg` sigue en `manylinux2014` y `soxr` en `manylinux_2_27`;
  - `--runtime-strip-prefix python`: el interprete de python-build-standalone viene dentro de un
    unico directorio `python/`, y el motor lo quiere en `runtime/bin/python3`;
  - `--no-nvidia`: la variante CUDA de Linux resuelve a `onnxruntime-gpu` 1.26 con ruedas cu12
    mientras la de CPU va a 1.31, y como los dos instalan el mismo paquete `onnxruntime` se
    pisarian los ficheros entre si. Se deja fuera a proposito, en vez de ofrecer un motor a medias.
  - Contenido: interprete `cpython 3.11.17` (46,7 MB), 14 wheels y los mismos modelos. Instalacion
    de 438,1 MB con el modelo fp32 y 287,3 MB con el fp16.
- **El instalador** (`stemengineinstaller.{h,cpp}`):
  - elige el manifiesto por plataforma (`engine.json` en Windows, `engine-linux.json` en el resto,
    con vuelta al primero si no esta);
  - no se queda con una variante que la lista no tenga: sin esto, la sugerencia automatica de CUDA
    dejaba la seleccion apuntando a una variante inexistente en Linux;
  - le pasa al interprete `PYTHONPATH` con la carpeta `site-packages` del motor. En Windows eso lo
    hace el `._pth` que escribe la instalacion; en Linux no existe tal fichero;
  - da el **permiso de ejecucion** al `ffmpeg` de `imageio_ffmpeg`, que viaja dentro de un ZIP y por
    tanto sin el bit de ejecucion;
  - **cancelar** ya no se confunde con fallar: la extraccion interrumpida no llama a `finish()` y la
    instalacion no queda marcada como hecha.
- **Lector de tar** `src/stems/tararchive.{h,cpp}`: gzip sobre zlib y tar con las extensiones que
  usan las herramientas de empaquetado (ustar, nombre y enlace largos de GNU, cabeceras PAX),
  **enlaces simbolicos y duros**, modo de cada fichero y recorte de un prefijo. Nada se escribe
  fuera del destino, igual que en el lector de ZIP. La extraccion informa del avance en bytes
  comprimidos y se puede interrumpir desde la llamada, que es lo que usa la cancelacion.
- **Pruebas** `src/test/tararchive_test.cpp`, dentro de la suite del proyecto y sin ficheros de
  apoyo (el test construye el tar y lo comprime el mismo): recorte de prefijo, permiso de
  ejecucion, enlace relativo, nombre largo, rechazo de rutas que salen del destino, extraccion sin
  prefijo y cancelacion. **Las cuatro pasan en Windows y en Linux** (`mixxx-test
  --gtest_filter=TarArchiveTest.*`), que es la unica forma de comprobar los enlaces y los permisos.
  Dos avisos que costaron una vuelta: `QFile::readAll()` sin `open()` devuelve vacio, y en Qt 6
  `symLinkTarget()` devuelve la ruta **absoluta** resuelta, asi que el test lee el enlace con
  `readlink()` para comprobar que se guardo relativo.
- **Validacion de verdad** `tools/validate_linux_engine.py`: baja el interprete, los wheels y el
  modelo desde el propio manifiesto, coloca el worker y el sustituto de `huggingface_hub` como lo
  hace el instalador, da el permiso al `ffmpeg`, y separa una pista de prueba de 12 s para escribir
  un `.stem.mp4`. Es lo que responde a la pregunta que el manifiesto no puede: si el motor arranca
  en Linux. **Comprobado en WSL (Ubuntu 24.04)**: interprete 3.11.17, 14 wheels, `ffmpeg-linux-x86_64-v7.0.2`
  del propio wheel, separacion de 12 s en 19,9 s de CPU y un `.stem.mp4` de 1,9 MB que su
  verificador acepta (5 pistas, manifiesto y tablas de offsets).
- **Comprobado por el usuario**: la generacion de stems funciona **en Linux** instalando el motor desde
  Preferencias y **en Windows**, o sea que el camino completo del motor en Linux (manifiesto,
  descarga, `tar.gz` con enlaces, permisos y ejecucion) esta validado tambien desde la aplicacion,
  no solo con el script de validacion.
- **Lo que sigue sin explicacion**: unos iconos en negro en la seccion Mic/Aux en Linux. Se ha
  descartado que falten los SVG (estan en el paquete), que haya discrepancias de mayusculas en las
  rutas (128 referencias revisadas, 0 discrepancias) y que algun paquete declarado no exista (71 de
  71 correctos). Falta el dato de la maquina afectada.


## 19. El paquete de Windows que no arrancaba (objetos obsoletos, otra vez)

El MSI 2.7.3 **moria al arrancar en Windows**: se veia la pantalla de inicio y desaparecia. En
Linux el mismo codigo iba bien, que fue la primera pista (en Linux GCC lleva ficheros de
dependencias; con MSVC y Ninja no siempre).

- **Diagnostico**: el registro de eventos de Windows dio el modulo que falla —
  `Qt6Core.dll`, `0xc0000005` — que con un desplazamiento tan bajo es un puntero corrompido, no un
  fallo de Qt. La prueba que lo separo: el Mixxx instalado en `C:\Program Files` (una compilacion
  anterior, misma musica, mismo escaneo) **arrancaba bien**, asi que el fallo era de esta
  compilacion.
- **Causa**: `stemgenerator.cpp.obj` era **anterior** a `stemengineinstaller.h`. Esa cabecera habia
  crecido (`m_unpackCancelled`, `manifestHasVariant`, `stripPrefix`) y ese fichero es justo el que
  construye el instalador, asi que reservaba el tamano viejo de la clase y el constructor escribia
  fuera de la memoria reservada. La corrupcion aparecia mas tarde, al escanear la biblioteca, y por
  eso el sintoma era "se queda en el logo".
- **Por que Ninja no lo vio**: los fuentes se estaban editando **mientras** corria una compilacion,
  asi que las marcas de tiempo quedaron en un orden que hizo creer a Ninja que el objeto estaba al
  dia. Es la misma trampa que el apartado 15 documenta para `dlgprefstems.cpp`, con otra causa.
- **Arreglo**: `find_stale_objs.ps1` (busca objetos mas viejos que las cabeceras cambiadas y que las
  mencionan), borrar los 47 que salieron y recompilar. **Comprobado arrancando el MSI extraido**:
  sobrevive al escaneo (100 s con la ventana principal abierta y 1 GB de memoria) donde el 2.7.3
  moria. La comprobacion se repitio con el paquete final (2.7.5) y el detector se pasa **antes** de
  empaquetar, como manda la regla de abajo.
- **Nota sobre los nombres**: CPack nombra el fichero con la version del *proyecto*, asi que el MSI
  sale siempre como `mixxx-2.7-alpha-...-amd64.msi`. `package-msi.cmd` lo renombra al copiarlo
  (`mixxx-2.7.5-stem-engine-amd64.msi`) para que en la carpeta de entrega haya una sola copia y se
  sepa cual es. El `.deb` conserva su nombre y su version (`2.7.0-alpha`), asi que para instalar
  encima de uno anterior hace falta `apt install --reinstall ./...deb`.
- **Regla para las proximas tandas**: pasar `find_stale_objs.ps1` **antes de empaquetar**, y no
  editar fuentes con una compilacion en marcha.


## 20. El motor en Linux con GPU (variante CUDA)

El manifiesto de Linux nacio con una sola variante (`cpu`). En esta tanda se le ha anadido la de
GPU y se ha comprobado **de verdad** sobre una RTX 3060.

- **Que lleva la variante**: los mismos ficheros comunes y los mismos modelos, pero con
  `onnxruntime-gpu 1.26` en lugar del `onnxruntime` normal, mas las ruedas de CUDA que necesita
  (`nvidia-cublas-cu12`, `nvidia-cudnn-cu12` y compania). En total 1.894,5 MB frente a los 263,5 MB
  de la de CPU: por eso la de GPU se ofrece aparte y no por defecto.
- **Dos trampas del generador de manifiestos** (`tools/make_manifest.py`):
  - `onnxruntime` y `onnxruntime-gpu` **no se pueden compartir** entre variantes: cada una necesita
    la suya. Para eso esta `VARIANT_SPECIFIC_WHEELS`.
  - Los extras de pip llevan comas, asi que la opcion `--nvidia-requirements` los pide separados
    por `;` y es la herramienta la que los parte.
- **Lo que faltaba en el codigo**: las ruedas de NVIDIA no estan en `site-packages` a secas, sino
  en `site-packages/nvidia/*/lib`, y el cargador de Linux no mira ahi. `StemGenerator` ahora anade
  esas carpetas a `LD_LIBRARY_PATH` (solo fuera de Windows) al montar el entorno del worker, junto
  a `PYTHONPATH` y `STEM_ENGINE_MODELS`.
- **Comprobado en WSL (Ubuntu 24.04) con la RTX 3060** (`validate_linux_engine.py --variant=nvidia`):
  el sondeo lista `[TensorrtExecutionProvider, CUDAExecutionProvider, CPUExecutionProvider]` con
  `has_cuda: true`, y la separacion de los mismos 12 s de audio baja de **19,0 s (CPU) a 12,8 s
  (GPU)** dejando un `.stem.mp4` que el verificador acepta.
- **Trampa al validar**: el manifiesto habla de variantes (`cpu`, `nvidia`), pero el worker habla de
  proveedores (`cpu`, `cuda`, `auto`). Pasar `--provider nvidia` falla con `invalid choice`; el
  script de validacion traduce la variante antes de llamarlo.
- **Lo que llevan los paquetes**: el `.deb` se rehizo despues de este trabajo, asi que su manifiesto
  trae las dos variantes y su binario el cambio de `LD_LIBRARY_PATH` (comprobado: el binario
  empaquetado es el compilado, 594 MB con simbolos -> 25 MB `stripped`, y el manifiesto de dentro
  tiene el mismo hash que el de `res/stems/`). El MSI se reempaqueto como **2.7.5** para que Windows
  lleve exactamente el mismo codigo, aunque el cambio de librerias solo actua fuera de Windows
  (`#ifndef Q_OS_WIN`): en Windows esas carpetas van en `PATH`, que es lo que ya hacia.


## 21. El icono del muelle (Plank) y el lanzador que no valia

En Linux la aplicacion arranca y se ve, pero en Plank (el muelle) salia con el icono generico, y el
lanzador no aparecia donde debia. Las dos causas se midieron, no se supusieron.

- **Causa 1: el fichero `.desktop` generado no era valido**. El original (`res/linux/org.mixxx.Mixxx.desktop`)
  venia con finales de linea de Windows y el `sed` del `postinst` solo reescribia las lineas que
  cambiaba (`Name`, `Exec`, `Icon`, `StartupWMClass`), asi que `[Desktop Entry]`, `Type=Application` y
  `Categories` conservaban el retorno de carro. `desktop-file-validate` lo rechaza con *"file contains
  at least one line ending with a carriage return, while lines should only be separated by a line feed
  character"* (53 bytes CR en total), y un lanzador invalido se ignora: ni entrada de menu ni
  asociacion con la ventana, y de ahi el icono generico. **Arreglo**: `tr -d '\r'` sobre el fichero
  entero antes de reescribirlo, que ademas deja de depender de donde venga el original.
- **Causa 2: `StartupWMClass` no era el de verdad**. La entrada original declara
  `org.mixxx.mixxx`, que es el `app_id` de Wayland (`QGuiApplication::setDesktopFileName`), pero en
  X11 Mixxx reporta otra cosa. Medido con un servidor X virtual y `xprop`:
  `WM_CLASS(STRING) = "mixxx", "Mixxx"`. **Arreglo**: `StartupWMClass=mixxx`, que en las
  comparaciones sin distinguir mayusculas cubre las dos partes (`mixxx` es la instancia y `Mixxx` la
  clase).
- **Y el icono, por nombre**: apuntaba a una ruta absoluta de un SVG fuera del tema. Ahora los PNG
  que ya trae el paquete se copian al tema de iconos como `mixxx-stem-engine` (`32x32` a `512x512`)
  y la entrada usa `Icon=mixxx-stem-engine`: nombre propio, asi que se resuelve como cualquier
  aplicacion y **no toca** los iconos `mixxx` del paquete oficial. El `postrm` los borra, y el
  `purge` se comprobo de nuevo: no queda ni el lanzador, ni la entrada, ni los iconos.
- **Como se comprobo todo esto** (en WSL, sin pantalla): instalar el `.deb`, `desktop-file-validate`
  (ahora sin errores, 0 retornos de carro), listar los iconos en
  `/usr/share/icons/hicolor/*/apps/`, y medir `WM_CLASS` arrancando Mixxx bajo `Xvfb` y leyendo la
  ventana con `xprop`. Los scripts de comprobacion quedan en la carpeta de la sesion
  (`check_icon_desktop.sh`, `check_icon_theme.sh`).
- **Lo que hay que hacer tras actualizar**: reinstalar el `.deb` y reiniciar Plank (o cerrar sesion y
  volver a entrar), porque el muelle lee los lanzadores al arrancar.
- **Una lectura que era falsa alarma**: en la onda se ve un recuadro con `CUE` y, debajo, un `1`
  rosa. No dice `CUT`: es la marca `cue_point` de la piel (`LateNightQML` y compania la dibujan con
  el texto `CUE`) junto con la insignia del hotcue 1, y aparece en **cada** onda de la pista (la de
  arriba, la principal y las de los dos platos), que es lo que hace parecer que se repite.


## 22. MIC/AUX y ranuras de efectos vacias en Linux (componentes atados a su contexto)

En Linux, la seccion MIC/AUX salia como cajas vacias (los `MIC 1..4` / `AUX 1..4` no aparecian) y las
ranuras de efectos tampoco dibujaban sus parametros. En Windows se veia bien. El registro de la
aplicacion lo dijo sin ambiguedad:

```
PulseDeck/MicAux/MicUnit.qml: Cannot instantiate bound component outside its creation context
PulseDeck/MicAux/AuxUnit.qml: ...
PulseDeck/Effects/EffectSlot.qml: ...
```

- **Causa**: esos ficheros llevan `pragma ComponentBehavior: Bound` (el modo moderno de QML) y a la vez
  entregan sus bloques `Component {}` a un `Loader.sourceComponent`. Qt prohibe justo esa
  combinacion: un componente *atado* a su contexto no se puede instanciar fuera de el, y el `Loader`
  lo intenta. El item no se crea, el `Loader` queda vacio y la unidad se dibuja como una caja.
  En Windows no se veia porque la version de Qt del paquete de dependencias no aplica esa restriccion.
- **Arreglo**: quitar el `pragma` de esos ficheros (`MicUnit.qml`, `AuxUnit.qml` y `EffectSlot.qml`
  en las tres pieles QML: `PulseDeck`, `LateNightQML` y `NeonDeck`). Es un cambio de una linea por
  fichero: vuelve al comportamiento de siempre, que es el que el codigo espera, y no toca nada mas.
- **Comprobado en Linux, con MIC/AUX y efectos activados** (`[Skin] show_microphones=1`,
  `show_effectrack=1`): antes del arreglo, tres avisos por unidad; despues, **cero** avisos
  `outside its creation context`, y una captura de la ventana muestra `MIC 1..4` y `AUX 1..4` con sus
  botones, igual que en Windows.
- **Como se reproduce sin pantalla**: `Xvfb`, arrancar el binario con `QT_QPA_PLATFORM=xcb`, y para
  la captura cerrar el asistente de primer arranque con `xdotool key Escape` (si no, lo que se
  fotografia es el dialogo de "elige tu carpeta de musica", que tapa la ventana principal).
- **Pendiente en el registro**: los avisos de `qml/ApplicationMenu*.qml` (`TypeError: Cannot read
  property 'length' of undefined`, `Unable to assign [undefined]`), que apuntan a los menus de la
  aplicacion. Son ruido aparte y no impiden arrancar; quedan para la siguiente tanda junto con el
  detalle de la marca `CUE` que parpadea.


## 23. La marca CUE que parpadeaba sobre la onda (sin cue puesto)

Sobre la onda aparecia un recuadro naranja `CUE` (con su banderin) y, al lado, una insignia rosa con
un `1`. Se veia aunque el plato no tuviera ningun cue puesto, y **parpadeaba**.

- **Lo que es**: dos marcas distintas de la piel. El recuadro `CUE` es
  `Mixxx.WaveformMark { control: "cue_point" }` y la insignia `1` es el hotcue 1 (en estas pistas,
  importado de Serato). Al tamano real, `CUE` + `1` se lee como "CUT 1".
- **El parpadeo, medido sobre el video del usuario**: extrayendo los 8,66 s a 10 fotogramas por
  segundo (90 en total) y recortando la zona de la marca, el recuadro aparece **solo en 4
  fotogramas** (0,8 s, 7,2 s y 8,3 s). O sea que no es una impresion: la marca se dibuja y se borra.
  Coincide con que se dibuja en el mismo sitio que el marcador de reproduccion, que se repinta encima
  cada fotograma.
- **Por que sale sin cue puesto**: en Mixxx 2.7 el control `cue_point` siempre tiene posicion (0 si no
  se ha puesto), asi que la marca existe siempre.
- **Arreglo**: se quita la marca `cue_point` de las ondas de las dos pieles del fork (`PulseDeck` y
  `NeonDeck`), con un comentario que explica el motivo. `LateNightQML` se deja como esta, porque es la
  de referencia.
- **Como se diagnostico un video sin poder verlo**: `ffprobe` para la duracion, `ffmpeg` con
  `fps=10,crop=...,tile=10x9` para tener los 90 fotogramas del recuadro en una sola imagen y ver en
  cuales aparece. Es la forma rapida de detectar algo intermitente.


## 24. Las imagenes no se instalaban, y los menus a prueba de un singleton raro

Dos cosas pequeñas que salieron del registro de Linux y de comparar arboles.

- **`res/images` no se instalaba en ningun paquete**. El proyecto solo instala
  `res/images/icons/` (para los iconos del menu del sistema) y el resto de las imagenes las da por
  hechas desde los recursos de Qt. Pero las pieles y los componentes QML se cargan **desde disco**, y
  sus referencias relativas (`../images/keywheel/keywheel.svg`) apuntan a `share/mixxx/images`, que no
  existia: de ahi el `Cannot open: .../images/keywheel/keywheel.svg` y el hueco en el dialogo del
  circulo de tonalidades. **Arreglo**: una regla `install(DIRECTORY res/images ...)` junto a la de
  las pieles, que son 136 ficheros y 2,1 MB. Comprobado que el `.deb` llevaba **cero** ficheros de
  `images/` antes del cambio. Sorpresa util: puede tapar huecos parecidos en otras ventanas.
- **`Mixxx.Application` en los menus**. En el equipo del usuario, con su version de Qt,
  `Mixxx.Application` no resuelve al proxy de Mixxx sino a un `QQuickApplication`, asi que
  `menuShortcut` y `textInputFocused` llegan indefinidos y **todos** los `Action` de los menus se
  quedan sin atajo. En el Linux de compilacion (Qt 6.4.2) no se reproduce: el `.deb` lleva el QML
  correcto y arranca sin un solo aviso, asi que es cosa del entorno. **Arreglo defensivo**: en
  `res/qml/ApplicationMenuActions.qml` el proxy se lee una sola vez en una propiedad y las dos
  llamadas van con comprobacion, de modo que si no esta se usa el atajo por defecto en lugar de
  romper el menu entero. Falta confirmar la version de Qt del equipo afectado
  (`qmake6 -query QT_VERSION`) para saber si hay que hacer algo mas.


## 25. El icono de verdad era la cache, el parpadeo era el bucle de dibujo, y el stem duplicado era mio

Tres cosas cerradas con comprobacion, ya sobre el equipo del usuario.

- **El icono (menu, muelle y lanzador) era la cache del tema**. Los ficheros estaban bien
  instalados y el `.desktop` validaba, pero `/usr/share/icons/hicolor/icon-theme.cache` no los
  indexaba, y tanto el menu como Plank leen la cache primero: de ahi la rueda generica en los tres
  sitios a la vez. **Arreglo**: el `postinst` regenera la cache con `gtk-update-icon-cache -f -t`
  (y el `postrm` tambien, al desinstalar), y se instala tambien el SVG escalable. Comprobado:
  `strings icon-theme.cache | grep mixxx-stem-engine` da 1 tras instalar y 0 tras `purge`, sin
  dejar ficheros. Confirmado por el usuario: "se arreglo lo del icono".
- **El parpadeo de la onda era el bucle de dibujo, no la grafica**. El registro se llenaba de
  `VisualPlayPosition::calcOffsetAtNextVSync "[Channel1]" outdated position request (offset <
  minOffset)`: el dibujo pide la posicion del siguiente fotograma y esa posicion ya paso, asi que
  Mixxx la recorta y la onda se queda quieta un fotograma y salta al siguiente. Con
  `QSG_RENDER_LOOP=basic` las lineas bajaron de cientos a 58, y con el modo de vsync ya forzado a
  temporizador (que es lo que hace `startVSync` con pieles QML) quedaron en 2. **Arreglo**:
  `main.cpp` fija `QSG_RENDER_LOOP=basic` antes de crear la aplicacion, solo en Linux y solo si el
  usuario no lo ha fijado el mismo. Detalle importante para el futuro: en pieles QML la clave
  `[Waveform] VSync` del `mixxx.cfg` **se ignora** (`startVSync` fuerza `ST_TIMER`), asi que aquello
  no era lo que funcionaba.
- **El stem duplicado lo provocaba el propio generador**. Al terminar, `StemGenerator` llamaba a
  `rescanLibrary()`, y el escaneo añadia el `.stem.mp4` como una pista mas: la misma cancion
  aparecia dos veces. Comprobado con el usuario que **la pista original ya reproduce los stems**
  (drum, vocals, etc.) y que su columna dice `STEMS`, asi que la llamada automatica sobra.
  **Arreglo**: se quita la llamada; el fichero queda al lado del original y la coleccion lo ve en el
  siguiente escaneo. El metodo `rescanLibrary()` **se conserva** porque la pagina Stems de
  Preferencias lo usa en un boton (para forzar el escaneo cuando se quiera).





