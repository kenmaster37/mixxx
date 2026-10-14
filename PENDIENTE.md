# Traspaso — Mixxx tipo VirtualDJ/Serato (pendientes)

Documento de continuidad. Todo lo aquí escrito está **verificado**, no es suposición.

---

## 1. Estado actual (funcionando)

| Elemento | Estado |
|---|---|
| App instalada en `C:\Program Files\Mixxx` | ✅ Arranca |
| Skin QML `NeonDeck` | ✅ Carga **sin** `--developer` y con todos sus SVG |
| Servidor OSC | ✅ Activo, confirmado por log |
| IA (DeepSeek/OpenAI) + AutoDJ armónico | ✅ En el binario |
| `.deb` de Linux | ❌ **Desactualizado**, hay que rehacerlo |
| Aspecto tipo VDJ/Serato | 🟡 **En curso** (sección 5): color por deck, anillos, pads junto al navegador, FX en los bordes, iconos Font Awesome, tipografía Rajdhani y BPM en color de deck — hechos y verificados con captura. Falta el layout por etapas (5.8) |

**Repositorio:** `c:\Users\giova\Desktop\mixxx\mixxx`
Mixxx 2.7.0-alpha · rama `main` · commit base `0fffd3d0d0`

**Los cambios NO están commiteados.** Todo vive en el árbol de trabajo. Primer paso
recomendado en la sesión nueva: commitear para no perderlo.

---

## 2. NO volver a depurar esto (dos causas raíz ya resueltas)

### 2.1 El cierre al arrancar NO era un bug de código

Síntoma: la app se cerraba durante el arranque (`0xC0000005` / corrupción de heap),
detectada en un sitio distinto cada ejecución.

Causa real: **base de dependencias de Ninja dañada** por builds interrumpidos.
`library.cpp.obj` quedó congelado a las 19:33 mientras `autodjfeature.h` cambiaba a
las 21:12. Como `Library::Library` construye `AutoDJFeature` vía `make_parented`,
asignaba memoria con el **tamaño antiguo** de la clase → corrupción de heap.

Stack que se veía:
`ntdll!RtlSizeHeap → sqlite3 → qsqlite → SettingsDAO::getValue →
TrackModel::getModelSetting → BaseSqlTableModel::setSort →
PlaylistTableModel::selectPlaylist → AutoDJProcessor::AutoDJProcessor →
AutoDJFeature::AutoDJFeature → make_parented → Library::Library → CoreServices::initialize`

**Reglas para que no vuelva a pasar:**
- **Nunca interrumpir una compilación.**
- Si tras cambiar un **header** aparecen crashes raros o que se mueven de sitio:
  hacer **recompilación limpia** antes de investigar código.
- Comando de comprobación rápida de sospecha: comparar fechas de
  `build\x64__legacy\CMakeFiles\**\<fichero>.cpp.obj` con los headers que incluye.
  Si un `.obj` es más antiguo que un header que incluye → es basura.

### 2.2 El aspecto "amateur" NO era el diseño

Causa real: el esquema `NeonDeck.qml` declaraba `name: "neondeck"`, y
`lateNightAsset()` construye las rutas como
`../../LateNight/<ColorScheme.name>/<carpeta>/<fichero>`.
Pero en `res/skins/LateNight/` **sólo existen `classic` y `palemoon`**. No hay
carpeta `neondeck` → **no se cargaba ni un solo SVG**.

Arreglado en `res/skins/NeonDeck/LateNightTheme/LateNightTheme.qml`: se mapea el
esquema a la carpeta real (`neondeck` → `palemoon`).

⚠️ **Tener en cuenta al tocar temas:** si se cambia el `name` de un esquema, hay
que garantizar que exista la carpeta de assets correspondiente.

---

## 3. Cambios ya hechos (para no repetirlos)

### C++ (modificados)
| Fichero | Qué se hizo |
|---|---|
| `src/main.cpp` | Carga skins QML **sin** `--developer` (se quitó la condición `args.getDeveloper()`) |
| `src/skin/skinloader.cpp` | `skinFromDirectory()` descubre `QmlSkin` como cualquier skin; `getDefaultSkinName()` devuelve `"NeonDeck"`; `isDeveloperOnlyQmlSkin()` ahora devuelve `false` (quedó vestigial) |
| `src/coreservices.cpp/.h` | Crea y arranca `OscServer` (miembro `m_pOscServer` **insertado en medio** de la clase) |
| `src/library/autodj/autodjfeature.cpp/.h` | Wiring de IA: `slotAiMixRequested/Succeeded/Failed`, `buildAiCandidateTracks()`, `getHarmonicSelectionConstraints()`, planner creado **perezosamente** |
| `src/library/autodj/dlgautodj.cpp/.h` | Fila de instrucción IA (`setupAiMixBar()`) + señal `aiMixRequested` |
| `src/library/dao/autodjcratesdao.cpp/.h` | `getRandomTrackIdHarmonic()` |
| `src/preferences/dialog/dlgpreferences.cpp` | Registra las páginas `DlgPrefOsc` y `DlgPrefAi` |
| `src/preferences/dialog/dlgprefautodj.*` | Grupo "Harmonic Mixing" |
| `src/mixxxmainwindow.cpp` | Usa `mixxx::skin::loadDefaultStyleSheet()` |
| `src/qml/qmlapplication.cpp` | `#include` sin usar eliminado; fija `QApplication::setFont()` a Rajdhani para los widgets nativos (biblioteca, menús, diálogos) |
| `CMakeLists.txt` | Registra las fuentes nuevas (11 inserciones) |

### C++ (nuevos)
- `src/ai/aimixplanner.{h,cpp}` — cliente compatible OpenAI/DeepSeek
- `src/controllers/osc/oscserver.{h,cpp}` — servidor OSC
- `src/library/autodj/harmonicmixing.{h,cpp}`
- `src/preferences/dialog/dlgprefai.{h,cpp,.ui}` y `dlgprefosc.{h,cpp,.ui}`
- `src/skin/defaultstylesheet.{h,cpp}`
- `src/test/aimixplanner_test.cpp`, `harmonicmixing_test.cpp`, `neondeckskin_test.cpp`

### Recursos
- `res/skins/NeonDeck/` (101 ficheros) — skin QML basado en LateNightQML
- `res/skins/NeonDeck/LateNightTheme/LateNightTheme.qml` — mapeo de assets (sección 2.2)
- `res/skins/NeonDeck/Samplers/SamplerPadGrid.qml` — **nuevo**: rejilla de pads (5.3)
- `res/skins/NeonDeck/Controls/IconLabel.qml` — **nuevo**: dibuja un glifo de Font
  Awesome (5.6)
- `res/fonts/Rajdhani-Light/Regular/Medium/SemiBold/Bold.ttf` +
  `res/fonts/Rajdhani.LICENSE.txt` — **nuevos**: tipografía del skin (SIL OFL, 5.5).
- `res/fonts/FontAwesomeFree-Solid.ttf` + `res/fonts/FontAwesomeFree.LICENSE.txt` —
  **nuevos**: iconos (Font Awesome Free 6.7.2, SIL OFL / CC BY 4.0 / MIT, 5.6).
  `FontUtils::initializeFonts` registra todo lo que haya en `res/fonts` que no sea `.txt`.

---

## 4. Cómo compilar y verificar

### Compilar (Windows → .msi)
```
c:\Users\giova\Desktop\mixxx\mixxx\build_windows_release.cmd
```
Hace todo: localiza MSVC con `vswhere`, `vcvars64`, usa CMake+Ninja de VS,
configura `build\x64__legacy`, compila y empaqueta con `cpack -G WIX`.
Tarda del orden de 10–20 min. **No interrumpir** (ver sección 2.1).

Las dependencias (1.19 GB) ya están en caché en
`c:\Users\giova\Desktop\mixxx\mixxx\buildenv\` — no se re-descargan.

Salida: `build\x64__legacy\mixxx-2.7-alpha-427-g0fffd3d0d0-modified-amd64.msi`

### Ver cambios de skin/QML SIN compilar
El skin y el QML son **datos**, no código. Para iterar el aspecto:
```
"C:\Program Files\Mixxx\mixxx.exe" --resource-path "c:\Users\giova\Desktop\mixxx\mixxx\res"
```
(`--developer` sólo hace falta con binarios anteriores al cambio de `main.cpp`.)

### Capturar la ventana para verificar
En PowerShell:
```powershell
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinCap {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
$p = Get-Process mixxx | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$h = $p.MainWindowHandle
$r = New-Object WinCap+RECT
[WinCap]::GetWindowRect($h, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap(($r.R-$r.L), ($r.B-$r.T))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc(); [WinCap]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc)
$bmp.Save("C:\Users\giova\Desktop\mixxx\shot.png", [System.Drawing.Imaging.ImageFormat]::Png)
```
`PrintWindow` con flag `2` (PW_RENDERFULLCONTENT) captura **sólo** esa ventana.

### Depurar un crash
- Volcados WER: `%LOCALAPPDATA%\CrashDumps\`
- Depurador: `C:\Program Files\WindowsApps\Microsoft.WinDbg_1.2610.1001.0_x64__8wekyb3d8bbwe\amd64\cdb.exe`
- Símbolos: `C:\Program Files\Mixxx\mixxx.pdb`
```
cdb.exe -z <dump> -y "srv*C:\symbols*https://msdl.microsoft.com/download/symbols;C:\Program Files\Mixxx" -c "!analyze -v; q"
```

### Trampas del entorno (ya me costaron tiempo)
- **PowerShell 5.1 `Remove-Item -Recurse -Force` sobre una junction borra el CONTENIDO DEL DESTINO.** Esto llegó a vaciar `C:\Program Files\Mixxx`. Usar `cmd /c rmdir "<junction>"`.
- `Start-Process -ArgumentList '--developer'` **descarta el argumento en silencio**. Pasar array: `@('--developer','--resource-path','"ruta con espacios"')`.
- `QT_USE_QSTRINGBUILDER` está activo: encadenar `"a" + b + "c"` da un *proxy*, que no convierte a `QJsonValue`. Asignar antes a `QString`.
- `QJsonValue` no tiene constructor de `int` → usar `static_cast<double>(int)`.
- Todo header con `Q_OBJECT` necesita `#include "moc_<nombre>.cpp"` en su `.cpp`.
- Al editar ficheros, **cuidado con la codificación**: se coló mojibake
  (`Schürmann` → `SchÃ¼rmann`) en `CMakeLists.txt`. Ya está corregido. Escribir siempre UTF-8.

---

### 4.1 Compilado y verificado (2026-10-12)

`build_windows_release.cmd` ejecutado a las 22:14–22:16 → **MSI nuevo**:
`build\x64__legacy\mixxx-2.7-alpha-427-g0fffd3d0d0-modified-amd64.msi` (119,9 MB).
Compiló `src/qml/qmlapplication.cpp` sin avisos. Ejecutando el binario recién
compilado (`build\x64__legacy\mixxx.exe --resource-path …\res`) con **música real**
(`Pistas (215)`): arranca sin asserts, la biblioteca sale ya con Rajdhani (el
`QApplication::setFont` de C++ funciona) y los decks cargan pistas.

**Ojo:** el `.msi` y el `.deb` que había en `Desktop\mixxx\` son anteriores a todo lo
de la sección 5. Instalar el nuevo `.msi` es lo que despliega el aspecto nuevo.

---

## 5. PENDIENTE — Aspecto tipo VDJ / Serato

**Decisión tomada: apuntar al estilo Serato/VirtualDJ.** Es plano y oscuro, con colores por
deck y waveforms grandes: se puede reconstruir con calidad y de forma legal, porque
es *estilo*, no arte copiado. (El usuario pidió explícitamente "que se vea mejor, como
VirtualDJ"; el objetivo de referencia es esa consola.)

### LÍMITE LEGAL (no negociable)
- ✅ Iconos **originales** inspirados en el estilo, en calidad profesional.
- ✅ Recursos con licencia abierta (MIT / CC0 / SIL OFL): Lucide, Tabler, Phosphor,
  fuentes Inter / Barlow Condensed / Archivo / **Rajdhani (usada)**.
- ❌ **No** copiar los ficheros gráficos reales de VirtualDJ/Serato, ni su logo ni
  su logotipo. Es su expresión protegida.

### 5.1 Color por deck (A cian / B rosa) + waveforms a juego
**Lo más reconocible del estilo.**

Medido: el skin **no tiene concepto de color por deck**. Toda la paleta depende de un
**único** `accentColor` global, en `LateNightTheme.qml:8`:
```qml
readonly property color accentColor: ColorScheme.accentColor
```
Hacerlo requiere:
1. Añadir `deckAColor` / `deckBColor` al tema (`res/skins/NeonDeck/LateNightTheme/LateNightTheme.qml`).
2. Cablearlos en los componentes del deck (`res/skins/NeonDeck/Deck/*.qml`), faders y platos.
3. **Color del waveform:** ⚠️ probablemente **no** es sólo QML — el color lo determina
   el renderizador de formas de onda de Mixxx. Verificar antes de prometer que no
   necesita compilación (yo ya me equivoqué afirmando eso).

**HECHO y verificado con captura (2026-10-12).** El punto 3 quedó resuelto: el color
de la señal **sí** es QML en este skin (`highColor/lowColor/midColor` de
`Waveforms/LateNightWaveformDisplay.qml` y `Deck/OverviewRow.qml`), no hace falta compilar.
Cómo quedó:
- Tema: `deckAColor: "#00e5ff"` (cian, canales impares) y `deckBColor: "#ff2d55"`
  (rojo, pares — se cambió de rosa a rojo para parecerse a VirtualDJ), más `usePerDeckColors` (`ColorScheme.name === "neondeck"`) y las
  funciones `deckNumberForGroup`, `deckColorForGroup` y `waveformSignalColorForGroup`.
- Classic y PaleMoon **conservan** su paleta original: `usePerDeckColors` es falso y
  `waveformSignalColorForGroup(group, legacyColor)` devuelve el color heredado.
- Cableado: `Deck/Deck.qml` (barra de acento de 2 px en el borde superior del deck,
  cubre Mini/Compact/Full y los 4 decks de una vez), `Waveforms/LateNightWaveformDisplay.qml`
  y `Deck/OverviewRow.qml` (señal del waveform), `Mixer/MixerChannel2Deck.qml` y
  `MixerChannel4Deck.qml` (`valueLineColor` del fader de volumen).

### 5.2 Anillos de color en los platos
**HECHO.** En `Deck/SpinnyCoverSlot.qml`: un `Rectangle` redondo con `border.width: 2`
y el color del deck, visible sólo con `usePerDeckColors`.

### 5.3 Rejilla de pads a la derecha del navegador
**HECHO.** Nuevo componente `Samplers/SamplerPadGrid.qml`: rejilla de 2 columnas
(4 u 8 pads según `[App] num_samplers`, con cabecera "Samplers") usando `SamplerMini`.
`SamplerMini` se rehizo como **pad**: glifo grande centrado (con relleno en color activo),
nombre del sample debajo y una fila fina con BPM/eject/orientación; radio 4. Es lo más
parecido a los pads de VirtualDJ/Serato.
En `MainWindow.qml` la antigua banda `samplersSection` (fila a todo el ancho debajo de
los decks) pasó a ser una **columna a la derecha de la biblioteca**; la biblioteca se
ancla con `anchors.right: samplersSection.left`. Ojo con el álgebra del panel:
`requiredPaneHeight` ya **no** suma `samplersSection.height` (ahora es una columna que
ocupa toda la altura de la biblioteca, no una banda).

### 5.4 FX en los bordes exteriores
**HECHO (versión banda).** En `Effects/EffectsRack.qml` cada unidad tiene ahora un
ancho acotado y se pega a los bordes:
`unitWidth: Math.min(420, Math.round(root.width * 0.32))`, `leftUnitEnd: unitWidth`,
`rightUnitStart: width - unitWidth`. Queda un hueco central vacío en esa banda.
**Pendiente si se quiere el look VDJ estricto:** los paneles de FX dentro de la fila de
decks (columnas a los lados de los platos). `EffectUnit` está diseñado como banda
horizontal ancha; pasarlo a columna vertical exige rehacer sus internos (los parámetros
usan `x`/`width` absolutos, no un layout).

### 5.5 Tipografía
**HECHO.** Decisión del usuario: **Rajdhani** (SIL OFL), no Barlow y **no** la fuente
"Virtual DJ" de DaFont (`Vdj.ttf`): es de mapa de bits a 8 px y no trae licencia formal
de redistribución, así que no se puede incluir en el paquete.
- Ficheros en `res/fonts/`: `Rajdhani-Light/Regular/Medium/SemiBold/Bold.ttf` +
  `Rajdhani.LICENSE.txt` (los `.txt` los ignora `FontUtils::initializeFonts`).
  Bajados de `raw.githubusercontent.com/google/fonts/main/ofl/rajdhani/` (familia única
  "Rajdhani" con estilos Light→Bold, así `font.weight`/`font.bold` funcionan de verdad;
  las instancias estáticas de `fonts.gstatic.com` en cambio registran familias sueltas
  tipo "Rajdhani SemiBold", que no se pueden seleccionar por peso).
- Tema: `LateNightTheme.fontFamily: "Rajdhani"`. Las 70 apariciones de
  `font.family: "Open Sans"` del skin se sustituyeron por `LateNightTheme.fontFamily`.
- Herencia: `MainWindow.qml` hace en `Component.onCompleted`
  `applicationWindow.font.family = LateNightTheme.fontFamily`.
  ⚠️ **No** importar `LateNightTheme` desde `main.qml`: adelanta la creación del
  singleton, `Mixxx.Config` se crea antes de que exista `s_pUserSettings` y la app
  aborta con `DEBUG ASSERT: "s_pUserSettings" in QmlConfigProxy::create()`.
  Tampoco vale `applicationWindow.font.family:` como binding declarativo en
  `MainWindow.qml` (la propiedad requerida todavía es null al evaluarlo).
- Widgets nativos (biblioteca, menús, diálogos): `src/qml/qmlapplication.cpp` fija
  `QApplication::setFont()` con la misma familia justo después de
  `m_pCoreServices->initialize(app)`. **Requiere compilar.**

### 5.6 Iconos con Font Awesome (en curso)
**Decisión del usuario:** usar los iconos de https://fontawesome.com en vez de
dibujarlos a mano.

**Licencia (importante):** sólo vale **Font Awesome Free** — fuentes bajo SIL OFL 1.1,
iconos bajo CC BY 4.0 y código bajo MIT, así que se puede redistribuir **con
atribución**. Font Awesome Pro es de pago y **no** se puede usar.
Atribución incluida en `res/fonts/FontAwesomeFree.LICENSE.txt`.

Cómo quedó montado:
- `res/fonts/FontAwesomeFree-Solid.ttf` (fa-solid-900, v6.7.2). Registra la familia
  **"Font Awesome 6 Free"**; el tema la expone como `LateNightTheme.iconFontFamily`.
- `LateNightTheme.iconGlyphs` es un mapa nombre→glifo con nombres idénticos a los de la
  web de Font Awesome (por ejemplo `"play": "\uf04b"`), más `iconGlyph(name)`, que
  devuelve cadena vacía si el nombre no existe (así un error tipográfico no dibuja un
  cuadro). Los códigos se sacaron de
  `raw.githubusercontent.com/FortAwesome/Font-Awesome/6.7.2/metadata/icons.json`.
- `Controls/IconLabel.qml` (nuevo) dibuja el glifo: `iconName`, `iconColor`, `iconSize`.
- `Deck/LateNightIconButton.qml` (y por herencia `LateNightControlButton`) acepta
  `iconName`, `iconColor` y `iconSize`; si `iconName` está puesto, oculta la `Image`
  del SVG y dibuja el glifo. `iconSize: 0` ajusta el glifo al botón. También hay
  `latchOverlayIconName` para el icono del overlay de pulsación larga.

**HECHA (verificada con capturas).** Cobertura: **70** usos de `iconName` en el skin.
Convertidos: play/pausa del deck (`LateNightPlayButton`), reverse, cue, loop (`repeat`),
reloop (`arrows-rotate`), beatjump (`angles-left`/`angles-right`), ancla de loop
(`star`/`crosshairs`), slip (`bolt`), quantize (`magnet`), eject, repeat, keylock
(`lock`), key match (`key`), key up/down, beatgrid completo (crosshairs, chevrons,
undo, lock/unlock), sync (`arrows-rotate`, incluido el overlay de pulsación larga),
samplers (repeat/eject/keylock/pfl/sync), mic (`microphone`), aux (`volume-high`),
"add" (`plus`), ducking (`bolt`/`sliders`/`power-off`), kill de EQ (`ban`) y cue
especiales intro/outro (`caret-left`/`caret-right`).
Se convirtieron además a **texto** lo que en VirtualDJ es texto: `CUE`, `IN`, `OUT`
(usando la propiedad `label` de `LateNightIconButton`).

Detalles de tamaño (petición del usuario: "buen tamaño, que no sobresalgan"):
- `LateNightIconButton` ajusta el glifo a **0,72 × min(ancho, alto) disponibles**
  cuando `iconSize` es 0, dejando margen respecto al bisel.
- `EqKillButton` usa `iconSize: 13`; los pads de sampler, glifos grandes centrados.
- Los glifos de FA no ocupan el mismo alto dentro del em (`plus`, `minus`, `eject` se
  ven más pequeños que `angles-right`); es propio de la fuente, no un recorte.

**A propósito siguen siendo SVG** (no tienen equivalente claro o son cifras):
`LateNightHotcueButton` y `SamplerHotcueButton` (pads numerados 1-8) y los badges de
"sync leader" (`leaderIcon()` / `syncLeaderIconSource()` en `RatePlaceholder`,
`RateCompact`).

⚠️ Al añadir una fuente nueva en `res/fonts` hay que **reempaquetar** (cpack) e
instalar; el `.msi` no la lleva hasta entonces.

---

### 5.7 Tema oscuro en menús y diálogos
**HECHO (2026-10-13, 00:35).** En modo QML los menús y diálogos son **widgets nativos de
Qt**: no siguen al skin.
- ⚠️ Un **stylesheet global desde `QmlApplication` rompe el arranque QML** (comprobado
  dos veces). No reintentar por ahí.
- Lo que **sí** funciona: en `QmlApplication` (ruta de skins QML) poner
  `QApplication::setStyle(QStyleFactory::create("Fusion"))` + una **`QPalette` oscura**.
  Fusion respeta la paleta; el estilo nativo `windowsvista` (que fija `main.cpp:281`)
  pinta con los colores del sistema y la ignora, que es por lo que los diálogos salían
  claros. La `QPalette` oscura está en `src\qml\qmlapplication.cpp`, junto al
  `setFont("Rajdhani")` que ya había.
- Verificado: *Preferencias* (Ctrl+P) ahora sale oscura (texto claro, campos oscuros,
  acento azul `#0096f3`). Captura: `shots\win1_Preferencias.png`.
- Los desplegables de la barra del skin (BIG LIBRARY, MIXER, …) son popups **QML**, ya
  oscuros por diseño; `default.qss` sigue aplicándose sólo en la ruta de widgets legacy.

---

### 5.8 Layout tipo VirtualDJ (plan por etapas)

El usuario pidió "que se vea muy similar" a VirtualDJ. **Igual no se puede** (es su
diseño y su arte, y su tipografía no es redistribuible), pero sí acercarse. Lo que ya
está alineado: waveform global arriba con A/B, mixer en el centro, browser abajo con el
árbol a la izquierda, pads de sampler en rejilla de 2 columnas a la derecha del
navegador, BPM gigante en color de deck, `CUE`/`IN`/`OUT` como texto, EQ kill tipo
"ban" y pads cuadrados con glifo + nombre.

**Etapas propuestas, de más a menos impacto:**
1. **Columnas PADS/FX a los lados de cada deck** (lo que da la silueta VDJ). Requiere:
   sacar el rack de FX de la banda inferior y ponerlo como columna en los bordes de la
   fila de decks; los decks se estrechan. ⚠️ `EffectUnit` está diseñado como banda
   horizontal: en modo *colapsado* y estrecho sus ranuras quedan de ~14 px y se rompe.
   Su modo *expandido* (2 ranuras apiladas + master a la derecha) sí es vertical, así
   que la columna debe forzar ese modo (`expanded`/`forceExpanded`). Además FullDeck
   (620 px) se queda sin sitio: habría que mover transporte/loop a la columna, como
   hace VDJ.
2. **Cabecera de deck VDJ**: badge con la letra del deck (A/B/C/D) en color, badge de
   tonalidad (Camelot) junto al BPM gigante, y título/artista a la izquierda.
   **HECHO.** `LateNightTheme.deckLetterForGroup(group)` devuelve A/B/C/D; la letra se
   dibuja como badge cuadrado del color del deck a la izquierda de la fila de título en
   `Deck/TitleTimeRows.qml`. La tonalidad (`Deck/KeyControlsPlaceholder.qml`) pasa a
   color de deck y en negrita. El BPM ya iba en color de deck (5.5), y es la pieza que
   más se parece a la foto.
3. **Barra superior compacta**: los toggles actuales son texto
   (`BIG LIBRARY`, `WAVEFORMS`, `4 DECKS`, `MIXER`, `EFFECTS`, `SAMPLERS`, `MIC/AUX`);
   en VDJ son iconos pequeños + etiqueta corta.
   **HECHO.** `LateNightToolbarButton` (componente inline en `Toolbar/Toolbar.qml`)
   acepta `iconName` y dibuja el glifo antes del texto; los 7 toggles llevan icono
   (`expand`, `timeline`, `table-columns`, `sliders`, `wand-magic-sparkles`,
   `compact-disc`, `microphone`) y se ensancharon para que el texto no se corte.
   De paso se quitaron los dos `family: "Open Sans"` que quedaban a fuego en
   `Toolbar.qml` (ahora `LateNightTheme.fontFamily`).
4. **Plato con "órbita"**: anillo interior + punto central de color (el anillo exterior
   ya está, 5.2).
   **HECHO.** `Deck/SpinnyCoverSlot.qml` dibuja tres anillos en color de deck:
   exterior (2 px), órbita interior (1 px al 45 % y al 74 % del tamaño) y un hub de 2 px
   al 22 %. ⚠️ El hub empezó siendo un disco relleno y **tapaba la carátula**: dejarlo
   como anillo.
5. **Estilo plano**: radios de esquina mayores y menos biseles (`Controls/Panel` tiene
   `radius: 1`; los pads ya van a 4).

Nada de esto rompe la ruta de widgets ni las otras gamas de color: igual que 5.1, todo
va detrás de `LateNightTheme.usePerDeckColors` o en componentes exclusivos del skin.

---

### 5.9 La carpeta "Mixxx (VDJ 8 Skin)" — NO es un skin de Mixxx

Está en `Desktop\mixxx\Mixxx (VDJ 8 Skin)` y en `Downloads\Mixxx (VDJ 8 Skin)`.
Contiene: `Mixxx.png` (3,6 MB, la imagen de fondo), `Logo.png`, `Brianne_s_hand.ttf` y
`Mixxx.xml` (40 KB). Autor **FatmanVladVDJ, (c)2025, sin licencia de redistribución**
→ **no se puede empaquetar** (y menos en un árbol GPL).

**Verificado que no carga:**
1. El cargador legacy busca el manifiesto como `skin.xml`
   (`kSkinManifestFileName` en `legacyskin.cpp`) y nombra el skin con el **nombre de la
   carpeta**, así que hay que copiarla y renombrar el XML.
2. Con `Mixxx.xml` → `skin.xml`, el parser falla:
   `Invalid node name in skin: Skin` (línea 1). La raíz debe ser `<skin>` en minúsculas.
3. Bajando la raíz a minúsculas, sigue fallando nodo a nodo:
   `Invalid node name in skin: slider / textzone / button / visual / browser …`
   (`legacyskinparser.cpp:667`). Los nombres válidos del formato legacy son
   **CamelCase** (`WidgetGroup`, `WidgetStack`, `Knob`, `Spinny`, `Library`,
   `EffectSelector`, …) y este XML usa `<button>`, `<slider>`, `<textzone>`, `<visual>`,
   `<browser>`, `<lists>`, `<columns>`, `<search>`, `<pannel>`, `<scratch>`,
   `<songpos>`, `<rhythmzone>`. **Es el formato de otro motor**, no el de Mixxx.

**Conclusión:** es arte + un XML de otro programa. Convertirlo es reescribir el layout
entero (mismo trabajo que el skin propio). Además describe un VDJ **distinto** al de la
captura del usuario: aquí el browser va a todo el ancho abajo (y=1325 de 2160, el 38 %)
y **no hay columna de pads a la derecha**; cada deck lleva waveform + jog + rejillas de
pads debajo, con el panel central (mixer) centrado. Para el look de la captura, nuestro
NeonDeck ya está más cerca.

**Lo que sí aporta** (referencia de proporciones, medidas reales a 4K):
- browser = 38 % inferior de la ventana; fila de decks ≈ 40 %; panel central centrado
  (`<pannel id="Audio">` x 1500–2341 de 3840).
- jog (`<scratch>`) de 286 px = 7,4 % del ancho; waveform a su lado.
- paleta del browser: fondo `#111111`, selección `#b40000`, columnas `#2E2E2E`.
- deck 1 `colorPlayed="#0096F3"`, deck 2 naranja.

**Estado local:** se copió a `%LOCALAPPDATA%\Mixxx\skins\VDJ8\` (con `skin.xml` y la raíz
en minúsculas) para probarla, **no cargaba** (UI vacía si se seleccionaba) y **se borró**.
La config quedó restaurada a `ResizableSkin NeonDeck`. El original del usuario sigue
intacto en `Desktop\mixxx\Mixxx (VDJ 8 Skin)` y en `Downloads\Mixxx (VDJ 8 Skin)`.
Nota útil: el build **sí** conserva la ruta de widgets y `LegacySkinParser` sigue
funcionando (se ve en el log), así que un skin legacy *de verdad* se podría usar.

---

### 5.10 Skin nuevo "PulseDeck" (2026-10-12) — el layout tipo VDJ de verdad

**Por qué:** doblar `LateNightQML` (NeonDeck) para que la fila de decks cupiera en
1360 px no cerraba: los decks miden 620 px de diseño mínimo cada uno, la fila
(A + mixer + B) pide 1518 px y en la ventana del usuario (1360) ya faltan 158 px antes
de meter nada nuevo. El usuario eligió **empezar un skin nuevo diseñado para 1360×768**
reutilizando todo lo que ya funciona (color por deck, Rajdhani, iconos FA, rejilla de
pads, modo vertical de FX). **NeonDeck sigue intacto y funcionando**, los dos skins
conviven.

**Qué se hizo** (`res\skins\PulseDeck\`, 104 ficheros = copia de NeonDeck + cambios):

- `Deck\FullDeck.qml` — recomposición completa del deck en 3 bloques:
  - **columna izquierda de 176 px** (pads y controles, como en la captura):
    fila de asignación de FX + key + vinilo (20 px) → **banco de 8 pads** que se estira
    (86×37 cada uno, `padColumns: 2` → 2 columnas × 4 filas) → `Flow` con intro/outro y
    beatjump (52 px).
  - **columna central** (lo que sobre, ~270 px a 1360): `TitleTimeRows` (46 px:
    título/artista a la izquierda, BPM/tono/tiempo a la derecha) → **tira fina de
    resumen de onda** (18 px, `OverviewRow`) → **jog centrado** con `anchors.centerIn`
    (el cover art se dibuja dentro, con los anillos del color del deck) → **fila de
    transporte + loop debajo del jog** (52 px: CUE, reverse, play, loop, tamaño de loop,
    IN/OUT, reloop) **igual que en la foto**.
  - **columna derecha**: `RatePlaceholder` (90 px: pitch + botones).
- `Deck\HotcuePadRow.qml` (**nuevo**) — banco de pads `GridLayout` con `padColumns`;
  los pads se estiran al espacio disponible (no son cuadrados de 26 px).
- `Deck\TransportLoopBeatjumpPlaceholders.qml` — el contenedor pasó de `RowLayout` a
  **`Flow`**, así los grupos (transporte / hotcues / intro-outro / loop / beatjump)
  **refluyen solos a columna** cuando se le da un ancho estrecho. Se añadió
  `property bool showTransport` (para poder ocultar el bloque de transporte, que en
  PulseDeck va bajo el jog) y `flowHeight` (alto del bloque, por defecto el de siempre).
  Los espaciadores que usaban `Layout.preferredWidth` (ignorado dentro de un `Flow`)
  pasaron a `width`.
- `Deck\Deck.qml` — `mirrorControls: LateNightTheme.deckNumberForGroup(root.group) % 2 === 0`:
  **los decks B/D espejan su columna de controles al borde exterior** (como VDJ).
  El espejo se hace con `LayoutMirroring.enabled` **sólo** en el `RowLayout` del deck
  (`childrenInherit` queda en `false`), así no se voltea la numeración de los pads.
- Paleta y estilo (en la copia del tema, `LateNightTheme\LateNightTheme.qml`):
  - `deckAColor` = **`#0096f3`** (azul VDJ 8 exacto, del XML del usuario),
    `deckBColor` = **`#ff3b30`** (rojo, como la onda del deck B de la captura).
  - Fondo neutro VDJ en vez del azul marino: ventana `#0d0d0d`, paneles `#171717`,
    tira de título del deck `#111111`, separadores `#2e2e2e`, textos `#f2f2f2`/`#8c8c8c`.
  - Transparencias: botones inactivos con alfa (`#70101010`, `#80131313`) sobre el panel.
  - `fullDeckHeight` = **230** (antes 206), para que quepan título + pads + jog + transporte.
  - `ColorSchemes\NeonDeck.qml`: `accentColor` = `#0096f3` (igual que el azul del deck A;
    el nombre interno de la paleta sigue siendo "neondeck" a propósito, ver pendientes).
- `Controls\Panel.qml` — **degradado vertical sutil** (`Qt.lighter(color, 1.14)` arriba →
  `Qt.darker(color, 1.35)` abajo) para dar el volumen/brillo tipo VDJ. El color sigue
  viniendo del tema, así que cualquier skin que herede el panel lo respeta.

**Config usada para probarlo** (`%LOCALAPPDATA%\Mixxx\mixxx.cfg`, formato `clave valor`):
```
ResizableSkin PulseDeck
Scheme NeonDeck
show_stem_controls 0        # estaba en 1: los paneles de stems tapaban la onda
show_beatgrid_controls 0    # la columna de beatgrid ocupaba 120 px de la banda
```
Con los dos `show_*` en 0 la banda muestra **dos ondas a todo el ancho** (una por deck,
apiladas) como en la captura. Se pueden volver a activar desde la barra (menú Waveforms).

**Verificado** (`shots\pulse-06.png`, con música): onda azul a todo el ancho, cover art
dentro del jog con anillo azul, pads 1/2/3 encendidos con el color del hotcue, BPM y
letra del deck en color, deck B espejado. `NeonDeck` sin tocar.

**Pendiente de este skin:**
1. ~~Los FX dentro de la columna del deck~~ — **HECHO (2026-10-12 23:40).**
   Nuevo `Deck\FxColumn.qml`: las **3 filas de la unidad a la que está ruteado el deck**,
   en modo compacto (`EffectSlot` con `expanded: false`), que es
   `[on/off 26][knob meta 35][selector de efecto]` — como las filas de VDJ
   (nombre + control + on/off), **teñidas con el color del deck**. La unidad se detecta
   con 4 `ControlProxy` sobre `[EffectRack1_EffectUnitN],group_<grupo>_enable` (la primera
   activa) y, si el deck no está ruteado a ninguna, usa la 1 para los decks A/C y la 4
   para B/D (que son las que el rack enseña en cada lado). No hizo falta tocar
   `EffectUnit`: se reutiliza tal cual.
   Además, **los 8 pads volvieron a una fila encima del jog** (como en la captura) con
   `HotcuePadRow { padColumns: 8 }`, y la banda de FX se oculta con
   `[Skin] show_effectrack 0` → la estructura queda **ondas / decks / browser** como la
   foto y el browser gana ~45 px. La vista completa de parámetros sigue disponible con el
   desplegable **EFFECTS** de la barra (vuelve a poner `show_effectrack 1`).
   Reparto del deck ahora: columna izquierda = asignación FX + key (20) + **FX (104)** +
   beatjump/intro-outro (52); centro = título (46) + tira de onda (18) + **pads (26)** +
   jog + transporte/loop (52); derecha = pitch (90).
   Verificado: `shots\fxcol-01.png` (con banda) y `shots\fxcol-02.png` (sin banda).
   **Cerrado también** el pendiente de la banda: con `show_effectrack 0` y
   `show_microphones 1` la banda muestra **sólo** el rack de micro/aux
   (`shots\micaux-only.png`), así que el traslado de `micAuxSection` dentro de
   `effectsSection` funciona.
2. Renombrar la paleta "neondeck" → "pulsedeck" (hay que tocar `ColorScheme.qml`,
   `usePerDeckColors` y, sobre todo, el mapa `assetScheme` que manda "neondeck" →
   "palemoon", o el skin se queda **sin arte**).
3. `skin_preview_*.png` propios y, si se quiere, `SkinLoader::getDefaultSkinName()`
   a `PulseDeck` (**requiere recompilar**; hoy el valor por defecto del código es
   NeonDeck y la config del usuario apunta a PulseDeck).
4. Detalles del mixer y del browser (tamaños/orden de botones) para acercarlos a la captura.
   Lo que aún no tiene equivalente en Mixxx es el panel **STEMS** por deck de la captura
   (Mixxx sí tiene controles de stems, pero en la banda de waveforms, no por deck).

**Scripts de verificación de esta sesión** (en
`c:\Users\giova\.copilot\session-state\e7306c0d-...\files\`):
- `shot.ps1` — captura con `PrintWindow`. **Ojo:** hay una ventana-tool de 160×28 con el
  mismo título, así que filtra por área > 200000 y reintenta; antes hace
  `ShowWindow(h, 9)` + `SetWindowPos(h, 0,0,1360,730)`.
- `dblclick.ps1` — doble clic real (SetCursorPos + mouse_event) para cargar pistas:
  hay que pulsar en la **columna Título** y **por debajo de la cabecera**, si no el clic
  cae en la cabecera y sólo **ordena** la lista.

---

### 5.11 Tema "glass" (2026-10-12) — menos saturado, más profesional

Petición del usuario tras comparar la captura con la de VDJ: "iconos o textura de
botones, efectos glass o translúcidos, algo que se vea más profesional, no tan chillón o
básico". En la captura de VDJ casi todo son **superficies grises finas con bordes de
1 px** y el color aparece sólo en detalles (anillo del jog, barra de progreso, pads, y el
botón PLAY sólido); lo nuestro eran bloques grandes con rellenos saturados.

Lo que se hizo (todo QML, en la copia de PulseDeck):

- **`Controls\GlassSurface.qml` (nuevo)**: superficie de cristal reutilizable = tinte
  translúcido + **borde de 1 px** + **reflejo superior de 1 px** + sombra inferior. Se
  coloca encima de la superficie con el mismo radio y margen.
- **`Deck\LateNightIconButton.qml`** (base de *casi* todos los botones, pads y hotcues):
  los estados **activo/pulsado ya no son bloques saturados** sino un **tinte translúcido
  del color de acento** (32 % activo / 48 % pulsado, `activeFillOpacity` /
  `activePressedFillOpacity` en el tema) con **borde del acento al 75 %**. El degradado del
  relleno es más suave (1.08/1.14 en vez de 1.16/1.25) y lleva el `GlassSurface` encima.
  Si un botón no tiene color de acento, se comporta como antes (no se rompe nada).
- **`Controls\Panel.qml`**: degradado más sutil (1.07 arriba / 1.18 abajo) y los bordes
  del panel pasaron a **hairlines translúcidos**: claro arriba (`#26ffffff`), laterales al
  8 % y sombra abajo (`#40000000`). Como `deckPanelBorder*` se usa en ~11 ficheros
  (separadores, bordes de carcasa), el efecto se propaga solo.
- **Tema**: `withAlpha(color, alpha)` + `glassBorderColor`, `glassHighlightColor`,
  `glassShadeColor`, `glassSurfaceColor`.

Verificado: `shots\glass-01.png` (vacío) y `shots\glass-02.png` (con música). Comparativa
lado a lado con la foto de VDJ en `shots\compare.png`.

**Ojo con las ondas** (no es un fallo del tema): el renderer *Filtered* usa
`waveformSignalColor`, que ya es del color del deck → onda **monocroma del color del
deck**, como VDJ (`pulse-07.png`). Pero si la pista tiene **stems** analizados, Mixxx
dibuja los carriles de stems (bandas verde/azul, como en `glass-02.png` con "Love Is
Gone" de David Guetta). Es una función de Mixxx, no del skin.

**Pendiente de este apartado:** el navegador/biblioteca (altura de fila, color de
selección, tipografía) **no se estiliza desde QML**: lo pinta la hoja de estilo por defecto
para skins QML en C++ (`src\skin\defaultstylesheet.h`) → compactarlo es cambio de C++ +
recompilar (familia de 5.7). Los pads de sampler se ven vacíos hasta que se carguen
samples (contenido, no skin): en la captura de VDJ llevan icono + nombre porque trae
samples de demo.

Aviso heredado que sale en el log al arrancar (no lo rompe nada, no es de este cambio):

```
QmlControlProxy: Requested CO "[Channel1]" , "beatloop_size_halve" does not exist, resetting...
QmlControlProxy: Requested CO "[Skin]" , "timing_shift_buttons" does not exist, resetting...
```

`timing_shift_buttons` lo usa la barra (LateNight lo tiene igual), y
`beatloop_size_halve/_double` sólo están en `Deck\TransportLoopBeatjumpCompact.qml`
(deck **compacto**, que PulseDeck no usa): si algún día se usa ese modo, el +/- del
tamaño de loop no hará nada porque esos controles no existen en este Mixxx. Los nombres
correctos son `loop_halve`/`loop_double`, que son los que usa el deck Full.

---

### 5.12 Tipografía y mixer (2026-10-12, tras "pule el mixer y la tipografía")

Comparado el zoom de la cabecera del deck y del mixer contra la captura de VDJ, la
diferencia era de **jerarquía**: en VDJ el título va **blanco en negrita**, el artista y
los tiempos en **gris**, y el color aparece sólo en BPM, clave e insignia; en el mixer,
los arcos de los knobs son **grises** y la línea del fader es neutra. Lo nuestro tenía
título, artista, tiempos, BPM y clave **todos del color del deck**, y arcos naranja/verde.

Cambios:

- **Escala tipográfica en el tema**: `fontSizeTiny/Small/Body/Subtitle/Title/Readout`
  (9/11/13/15/17/24) y `labelLetterSpacing` (0,6), para que todo use los mismos tamaños.
- **`Deck\LateNightTrackPropertyText.qml`**: nueva `fontWeight` (antes estaba fijado a
  `Font.Normal`).
- **`Deck\TitleTimeRows.qml`**: título `textColor` + `Font.DemiBold` + 17 px; artista
  `textColorMuted` + 15 px; tiempo/duration en gris; la **insignia de la letra del deck**
  ahora es translúcida (28 %) con borde y letra del color del deck (antes era un bloque
  sólido). Se eliminó la propiedad `loadedDeckTextColor`, que ya no se usa.
- **`Deck\RatePlaceholder.qml`**: BPM sigue en negrita y del color del deck (24 px); el
  % de pitch y el rango pasan a gris (`rateTextColor` = `textColorMuted`).
- **`Mixer\QuickEffectSelector.qml`**: 11 px, `Font.AllUppercase` y tracking → se lee
  "FILTRO" como en VDJ.
- **Arcos del mixer neutros** (antes naranja/verde/rojo): `mixerArcEqColor` `#8f8f8f`,
  `mixerArcGainColor` `#a6a6a6`, `mixerArcGainLowColor` `#7d7d7d`,
  `mixerArcMainBalanceColor` `#9c9c9c`, `mixerArcQuickEffectColor` `#8f8f8f`,
  `mixerQuickEffectSelectorTextColor` `#a8a8a8`, `mixerSliderBarColor` `#8a8a8a`.
- **`Mixer\MixerChannel2Deck.qml` / `MixerChannel4Deck.qml`**: la línea del fader usa el
  color del deck **al 60 %** (acento) en vez de una barra saturada.
- **`Mixer\EqKillButton.qml`**: icono 11 px en gris apagado y fondo translúcido
  (`#66111113`), antes 13 px en blanco sobre `#111113`.

⚠️ **Trampa (me costó un arranque fallido):** al reordenar propiedades dejé
`useSecondaryDeckText` **duplicada** en `TitleTimeRows.qml` y Mixxx no arrancaba:
`TitleTimeRows.qml:19:28: Duplicate property name` → `Failed to load QML file`. El log
(`%LOCALAPPDATA%\Mixxx\mixxx.log`) lo dice en la última línea antes de
`shutdown complete with code 1`.

Nota de automatización: cargar pistas con `dblclick.ps1` **falla mientras la biblioteca
está analizando** (los hilos de análisis retrasan el segundo clic y Qt ya no lo cuenta
como doble clic). Solución: llamar al script **dos veces** sobre el mismo punto
(`typo-03.png` se hizo así) o esperar a que termine el análisis.

Verificado con `shots\typo-03.png` (dos decks con música): título blanco, artista gris,
BPM/clave en color del deck, insignia translúcida, FX con nombre ("Eco…"), mixer con
arcos grises y "FILTRO" en mayúsculas. Comparativa actualizada en
`shots\compare-typografia.png`.

---

### 5.13 Navegador/biblioteca (2026-10-12) — se afina por QSS, no por C++

El estilo de la biblioteca embebida en un skin QML **no es C++**: `QmlLegacyLibraryItem`
(`src\qml\qmllegacylibraryitem.cpp`) elige la hoja QSS según el esquema de la config
(`[Config] Scheme`): si es "Classic" carga `res\skins\LateNight\style_classic.qss`, si no
`res\skins\LateNight\style_palemoon.qss` (124 KB, **compartida** con el skin legacy
LateNight en modo palemoon), y antepone `res\skins\default.qss`.

Cambios hechos en `res\skins\LateNight\style_palemoon.qss`:
- Fondo de la biblioteca `#0f0f0f` → **`#111111`** (el del navegador de VDJ),
  rayado alterno `#0a0a0a` → `#0d0d0d`.
- Selección: el teal `#2c454f` → **`#1d3b4d`** (azul tenue a juego con el acento del deck),
  en las 6 apariciones (tabla, sidebar, BPM/Played).

Verificado en `shots\browser-01.png` (fondo más oscuro y fila seleccionada azulada).

**Datos útiles para seguir afinando el navegador:**
- Altura de fila: es **preferencia**, no skin: `[Library] RowHeight` (por defecto 20,
  `Library::kDefaultRowHeightPx` en `src\library\library.cpp:60`), editable en
  *Preferences → Library → Row height*, y expuesta al QML como
  `Mixxx.Config.libraryRowHeight` (`qmlconfigproxy.cpp:838`).
- El color de fondo base del widget está en C++ (`kLegacyLibraryBackgroundColor`,
  `#1e1e1e`) pero lo pisa el QSS.
- Ojo: `style_palemoon.qss` es compartida → un cambio ahí también afecta al skin legacy
  LateNight en esquema palemoon.

---

### 5.14 Ondas en dos tonos del color del deck (2026-10-13) — y paleta gris descartada

- **Ondas**: el usuario tenía `[Waveform] WaveformType 12` (RGB) → ondas azul/morado/verde.
  VDJ dibuja cada onda en **tonos del color de su deck**. En
  `Waveforms\LateNightWaveformDisplay.qml` el skin fuerza el renderer **Filtered** cuando
  `LateNightTheme.deckColorWaveforms` es `true` (= `usePerDeckColors`), con
  `waveformLowColorForGroup` (deck color `Qt.darker 2.4`, cuerpo), `waveformMidColorForGroup`
  (`Qt.darker 1.25`) y `waveformHighColorForGroup` (`Qt.tint` blanco 65 %, picos).
  Resultado: deck A en azules y deck B en rojos, como la captura (`shots\restaurado-ondas.png`).
  Para volver a respetar el tipo elegido en Preferencias: `deckColorWaveforms: false`.
- **Paleta gris carbón (descartada)**: se probó la paleta medida en la captura de VDJ
  (paneles `#36373b`, botones hundidos `#232326`, barra `#343539`). **Al usuario no le
  gustó** ("estaba mejor el color anterior") y se **revirtió** a los 40 valores previos.
  No volver a proponerla.

---

### 5.15 Por qué en Linux Mint "no se veía como Windows" (2026-10-13)

El usuario reportó que el `.deb` recién instalado no se parecía a Windows. **Eran dos
causas independientes, las dos por la misma razón: en Windows la configuración tiene dos
líneas que en un Mint recién instalado no existen.**

1. **Skin por defecto.** `SkinLoader::getDefaultSkinName()` devolvía `"NeonDeck"`, así que
   una instalación nueva arrancaba con el skin viejo (el heredado de LateNightQML), no con
   el diseño VDJ de `PulseDeck`. En Windows funcionaba sólo porque su
   `%LOCALAPPDATA%\Mixxx\mixxx.cfg` tiene `ResizableSkin PulseDeck` puesto a mano.
   **Arreglo:** `getDefaultSkinName()` → `"PulseDeck"` (`src\skin\skinloader.cpp`).
2. **Esquema de color por defecto — el peor de los dos.** Sin `Scheme` en la configuración,
   `QmlConfigProxy` usa `QStringLiteral("PaleMoon")` y `ColorScheme.qml` cae en
   `paleMoonScheme`. Como `LateNightTheme` deriva de ahí:
   - `usePerDeckColors` (= `ColorScheme.name == "neondeck"`) → **falso** ⇒ sin color por
     deck, sin insignia A/B, sin BPM/clave en color.
   - `deckColorWaveforms` → **falso** ⇒ sin ondas en dos tonos.
   - `isPaleMoon` → **verdadero** ⇒ el skin usaba la rama de arte/estilos de PaleMoon.

   **Arreglo (contenido al skin, sin tocar C++):** en
   `res\skins\PulseDeck\LateNightTheme\ColorScheme.qml` el `activeScheme` ahora resuelve
   `"classic"` → Classic, `"palemoon"` → PaleMoon, y **todo lo demás (incluido vacío) →
   NeonDeck**, que es la paleta alrededor de la cual está diseñado el skin. Se mantiene la
   selección explícita de PaleMoon, así que el ajuste de la barra sigue funcionando.

**No era el skin lo único que podía fallar:** se revisó también el código con
`Q_OS_LINUX`/`Q_OS_WIN` en la ruta visual y sólo hay uno
(`qmllegacylibraryitem.cpp:372`, disparador de menú contextual: press vs release), que no
afecta al aspecto. El build de Linux tiene `QML:BOOL=ON` y `STEM:BOOL=ON`, correcto.

**Tests nuevos** (`src\test\pulsedeck_skin_test.cpp`, registrado en `CMakeLists.txt` junto
a `neondeckskin_test.cpp`): 5 pruebas que fijan el skin por defecto, comprueban que
`PulseDeck` es un skin QML válido, que anuncia el esquema `NeonDeck` y que existe el
fichero `ColorSchemes\NeonDeck.qml` al que se cae. Los 8 tests de skins pasan.

⚠️ **Consecuencia:** hay que recompilar y reempaquetar **los dos** paquetes; el `.deb`
antiguo seguirá viéndose "como NeonDeck/PaleMoon" aunque tenga las fuentes.

---

## 6. PENDIENTE — Otros

### 6.1 Convertir la lectura de la API key a asíncrona
**HECHO.** `AiMixPlanner::requestSetList()` ya no usa la lectura bloqueante:
- Nueva `AiMixPlanner::readApiKeyAsync(providerId, onLoaded)` (usa `ReadPasswordJob`
  con `setAutoDelete(true)`, igual que `DlgPrefAi`).
- `sendRequest(settings, instruction, candidateTracks, apiKey)` es el antiguo tramo de
  red, ahora separado.
- Mientras se lee la clave, `m_apiKeyRequestPending` mantiene `isBusy()` en true, así
  que no se pueden solapar dos peticiones; `abort()` lo baja y la respuesta se descarta.
- Sin QtKeychain (`__QTKEYCHAIN__` no definido) se sigue devolviendo el mismo error
  síncrono de antes.

### 6.2 Limpieza

**HECHO** salvo lo indicado:
- ✅ Borrado `build\x64__legacy_stale\` (4,17 GB) con `cmd /c rmdir /s /q`.
- ✅ Borrado `%LOCALAPPDATA%\Mixxx\skins\ProDeck\` (109 ficheros huérfanos) con
  `cmd /c rmdir /s /q`.
- ✅ Quitado el `#include "skin/defaultstylesheet.h"` sin usar de
  `src/qml/qmlapplication.cpp`.
- ✅ Eliminado `SkinLoader::isDeveloperOnlyQmlSkin()` (declaración en `skinloader.h`,
  definición y uso en `skinloader.cpp`); el `qWarning` de skin no encontrado se mantiene.
- ⏳ Nomenclatura: el directorio del skin sigue siendo `NeonDeck` y el skin por defecto
  también. **No** se ha renombrado a "ProDeck": si algún día se hace, hay que cambiar
  **también** `src/test/neondeckskin_test.cpp`, que referencia
  `RESOURCE_FOLDER "/skins/NeonDeck"`.

### 6.3 Rehacer el `.deb` de Linux
**HECHO (2026-10-13, 00:59)** junto con el `.msi` final. Los dos paquetes están en
**`c:\Users\giova\Desktop\mixxx\dist\`**:
- `mixxx-2.7-alpha-427-g0fffd3d0d0-modified-amd64.msi` (114 MB) — instalado y verificado.
- `mixxx-2.7.0-alpha-unknown-x86_64.deb` (27,5 MB) — Ubuntu 24.04 amd64;
  `apt-get install -s` resuelve todas las dependencias.

Cómo se hizo (build **incremental**, unos minutos en vez de una hora):
script `wsl_rebuild_deb.sh` de esta sesión
(`c:\Users\giova\.copilot\session-state\e7306c0d-...\files\`), que:
1. sincroniza con `rsync -a --delete` el árbol de Windows a `/home/mixxx/mixxx/`
   **excluyendo** `/build/`, `/buildenv/` (MSVC) y `.git` → el `build/` de Linux en WSL se
   conserva;
2. normaliza los permisos (NTFS marca todo como ejecutable y debhelper falla);
3. `cmake --build` + `cpack -G DEB` como el usuario `mixxx`, con el *shim* de `uname`
   (`/home/mixxx/toolshim`) por el guard de WSL en `CMakeLists.txt:270`;
4. copia el `.deb` a `dist\`.
Los avisos `Time::Piece strptime ... Garbage at end of string` de cpack son del parseo del
changelog y no afectan al paquete.

⚠️ **Compatibilidad con Linux Mint 22.3 (2026-10-13).** El usuario reportó que al instalar
el `.deb` en Mint se le **quitaban `libavcodec`, `mint-meta-codecs` y `libasound2`**.
Causa: Mint instala por defecto las FFmpeg **"-extra"** (`mint-meta-codecs` →
`libavcodec-extra` → `libavcodec-extra60`). Esos paquetes declaran
`Provides: libavcodec60 (= …)` **y** `Conflicts: libavcodec60`. CPack/shlibdeps escribía
`Depends: libavcodec60 (>= 7:6.0)` a secas: `apt` en consola acepta el Provides, pero
**GDebi y el instalador gráfico de Mint no**, quieren el `libavcodec60` "real" y para
eso desinstalan `libavcodec-extra60` → `libavcodec-extra` → `mint-meta-codecs` (y lo que
cuelga de ellos). Mixxx **no** depende de `libasound2` directamente (`objdump -p`:
sólo `libportaudio.so.2`, que ya depende de `libasound2t64`), así que su retirada era
efecto en cadena de lo anterior.
Arreglo: `packaging/CPackDebFfmpegExtraAlternatives.cmake` (nuevo), registrado como
`CPACK_POST_BUILD_SCRIPTS` en `packaging/CPackConfig.cmake` sólo para el generador DEB.
Después de generar el `.deb` lo desempaqueta, reescribe en `DEBIAN/control`
`libavcodecNN`, `libavformatNN` y `libavfilterNN` como
`libavcodec60 (>= 7:6.0) | libavcodec-extra60 (>= 7:6.0)` (genérico para futuras
versiones de FFmpeg; idempotente) y lo vuelve a empaquetar con
`dpkg-deb --root-owner-group -Zxz` (xz: lo entienden todos los dpkg/instaladores; el
zstd por defecto del dpkg nuevo no). Contenido verificado idéntico (3470 ficheros, mismos
permisos/dueños; sólo se añade la entrada `./`). `libavutil58` y `libswresample4` no
tienen variante `-extra` y no se tocan.
Verificación (sin hardware Mint): chroot `debootstrap noble` + repositorio oficial de Mint
22.3 (`packages.linuxmint.com zena`), `apt-get -s install mint-meta-codecs ./mixxx.deb`
→ elige `libavcodec-extra60` + `libavcodec-extra`, **0 paquetes a quitar (`Remv`)**,
exit 0. La instalación real de `mint-meta-codecs` en el chroot se abandonó porque la red
iba a <50 KB/s (el chroot ya se borró). En el Mint real del usuario, comprobar antes con
`sudo apt install --simulate ./mixxx-*.deb` que no aparezca ninguna línea `Remv`.
⚠️ Una espera larga sobre esa descarga **colgó VS Code**: para tareas así, lanzar en
segundo plano y consultar con comprobaciones cortas.

⚠️ **Bug encontrado al empaquetar:** en Linux `CMakeLists.txt` **no instalaba
`res/fonts`** (por diseño espera las fuentes del gestor de paquetes). Open Sans y Ubuntu sí
están empaquetadas (`fonts-open-sans`, `fonts-ubuntu` siguen en `Depends`), pero
**Rajdhani y Font Awesome 6 Free no** → en Linux se perdían la tipografía y **todos los
iconos** de los skins QML. Arreglo en `CMakeLists.txt` (rama `else()` del bloque *Font
files*): instalar sólo `Rajdhani*` y `FontAwesomeFree*` (con sus licencias), como sugiere el
propio comentario de CMake. `FontUtils::initializeFonts` las carga desde
`/usr/share/mixxx/fonts`. Verificado en el contenido del `.deb`. La rama de Windows/macOS
no cambia.

### 6.4 Falta música para poder probar nada
**RESUELTO en parte:** la biblioteca pasó de 1 a **215 pistas** (120 con tonalidad **y**
bpm analizados, 128 con tonalidad). Ya se puede validar con música real: en la sesión del
2026-10-12 se cargaron pistas y se comprobó el waveform por deck y la mezcla armónica.

### 6.5 Validar las funciones que ya están en el binario
Están implementadas y compiladas, pero **sin música no se han podido probar de verdad**:
- AutoDJ armónico (selección por Camelot + ventana de tempo).
- Planificador de setlist con IA (página *AI Assistant* en preferencias: proveedor +
  API key + instrucción en texto).
- Control por OSC (`/Channel1/play`, `/Master/crossfader`, …) en `127.0.0.1:11119`,
  feedback a `11118`.
- Color de waveform por deck (5.1): sólo se ve con una pista cargada; con la biblioteca
  vacía las zonas de waveform salen en negro. **Pendiente de comprobar con una pista.**

### 6.6 Selección armónica del AutoDJ — validada y mejorada (2026-10-12)

El código ya estaba completo (`HarmonicMixing` + `AutoDJCratesDAO::getRandomTrackIdHarmonic`
+ `AutoDJFeature::getHarmonicSelectionConstraints` + UI en *Preferences → Auto DJ*), pero
**sin validar**. Lo comprobado en esta sesión:

1. **Tests:** `HarmonicMixingTest` 8/8 (pasaban ya) y, tras el cambio de abajo, **12/12**.
   `AutoDJProcessorTest` 28/28 y `KeyUtilsTest` 8/8 siguen pasando. Target compilado con
   `build\x64__legacy` → `cmake --build . --target mixxx-test` (script reusable en
   `files\build_tests.cmd` de esta sesión; tarda ~1 min).
2. **Los ids de tonalidad coinciden**: `library.key_id` usa exactamente el enum
   `ChromaticKey` (comprobado contra la biblioteca real: 18 = "Fm", 17 = "Em",
   22 = "Am", 21 = "G#m"…), así que el filtro `key_id IN (...)` del SQL es correcto.
3. **Fallo medible** con la biblioteca real del usuario: con la tolerancia por defecto
   (6 %) **3 de 120** pistas de referencia no tenían ninguna candidata y el código caía a
   **aleatorio puro**, perdiendo la compatibilidad de tonalidad aunque sí existían pistas
   compatibles por tonalidad (el filtro "sólo tonalidad" nunca se quedaba sin candidatas).
   Causa: la biblioteca está concentrada en 120–128 bpm, así que la ventana de tempo es la
   que excluye (sólo 1 de 120 se queda sin candidatas filtrando sólo por bpm).
4. **Mejora aplicada** — degradación progresiva, primero lo que el usuario pidió y sólo
   después se relaja, sin perder nunca la tonalidad:
   `HarmonicMixing::bpmFallbackRanges()` devuelve las ventanas en orden
   (tolerancia configurada → 3× más ancha → rango inválido = "ignora el tempo");
   `AutoDJFeature::slotAddRandomTrack()` las recorre y se queda con la primera que dé
   resultado; `getRandomTrackIdHarmonic()` acepta `bpmMin <= 0` para la consulta sólo por
   tonalidad. Tests nuevos: `BpmFallbackRanges*` (4).
5. **Para activarlo** (viene **apagado** por defecto): *Preferences → Auto DJ →
   "Harmonic mixing"* (casillas `[Auto DJ] HarmonicMixing` y `HarmonicBpmTolerance`,
   por defecto 6 %).

**Pendiente de este punto:** probarlo con el binario instalado (requiere recompilar) y
decidir si la tolerancia por defecto de 6 % es la adecuada (con esta biblioteca, 10 % no
cambia nada y 15 % arregla 2 de los 3 casos que quedaban).

---

### 6.7 Atajos de teclado que no se cargaban (bug encontrado y arreglado 2026-10-13)

El log decía, en **todos** los arranques:

```
KeyboardEventFilter - ".../res/keyboard/es_CL.kbd.cfg" not found, trying en_US.kbd.cfg
KeyboardEventFilter - ".../res/en_US.kbd.cfg" not found, starting without shortcuts
```

Es decir: **el usuario no tenía ni un atajo de teclado** (ni Ctrl+P, ni nada). Los
ficheros existen (`res\keyboard\` tiene 12 idiomas, incluido `en_US.kbd.cfg`), pero el
*fallback* construía mal la ruta en `src\controllers\keyboard\keyboardeventfilter.cpp`:
pasaba `resourcePath` (que termina en `/res/`) a `mappingFilePath()` en vez de
`resourcePath + "keyboard/"`, así que buscaba `res/en_US.kbd.cfg` en lugar de
`res/keyboard/en_US.kbd.cfg` y nunca lo encontraba. Sólo funcionaba si existía el fichero
del locale exacto (no hay `es_CL.kbd.cfg`).

Arreglo (1 línea): pasar `QString(resourcePath).append("keyboard/")`.
Verificado con el binario instalado: el log ya no dice "starting without shortcuts" y
**Ctrl+P abre Preferencias** (captura `shots\win1_Preferencias.png`).

### 6.8 Panel «Track Advisor» (2026-10-13)

Panel plegable nuevo, en la esquina inferior derecha **debajo de la rejilla de samplers**,
que sugiere qué pinchar a continuación: las pistas de la biblioteca que mezclan
armónicamente con la que suena en el deck. La biblioteca se desplaza a la izquierda
cuando el panel está abierto, así que no la tapa.

- **Backend:** `QmlLibraryProxy::findCompatibleTracks()` (`src/qml/qmllibraryproxy.{h,cpp}`).
  Reutiliza `mixxx::HarmonicMixing` (círculo de quintas de `KeyUtils` + la tolerancia
  `[Auto DJ] HarmonicBpmTolerance`, 6 % por defecto) y acepta además las ventanas de
  tempo mitad y doble, que se mezclan igual. Devuelve una lista de mapas (artista,
  título, bpm, clave numérica, veces reproducida, URL), sin proxies.
- **Interruptor:** control `[Skin] show_track_advisor` en `src/skin/skincontrols.cpp`
  (por defecto **apagado**), botón **ADVISOR** en la barra y *View → Show Track Advisor*
  (**Ctrl+7**) en `src/widget/wmainmenubar.cpp`.
- **Skin:** `res/skins/{NeonDeck,PulseDeck}/Advisor/TrackAdvisor.qml`, más el cableado en
  `MainWindow.qml` (sección `advisorSection`, que ancla la de samplers a su borde
  superior) y `Toolbar/Toolbar.qml` (alias `showTrackAdvisor` + botón).
- **Interacción:** clic en una fila carga esa pista en el deck que **no** guía la lista
  (el de referencia no se toca, así la lista sigue siendo válida); el botón ⟳ la
  recalcula. La clave se dibuja con `Mixxx.KeyUtils.keyToString()`, así que respeta la
  notación del usuario.
- **Tamaño:** con los samplers a la vista el panel se queda con la mitad de la columna;
  al quitarlos ocupa **toda** la columna y la lista crece con el alto (el número de
  filas pedidas se deriva de `height`, así que se ve completa y no hay hueco vacío).

Cuatro trampas que costaron tiempo:

1. `location` existe en `library` y en `track_locations`: sin cualificar
   (`tl.location`) SQLite responde *`ambiguous column name: location`* y la consulta
   falla **en silencio** para el QML (sólo se veía con el `qWarning` del log).
2. La lista hay que construirla en `Component.onCompleted`: Mixxx **restaura en los decks
   la última pista** al arrancar, así que `onReferenceTrackChanged` no se dispara nunca y
   el panel se quedaba vacío.
3. `anchors.centerIn` + `width` calculado en el texto de «no hay resultados» producía un
   *binding loop*; con `anchors.fill` + márgenes desaparece.
4. **`effectsSection.bottom` no se puede usar en una expresión**: como valor de anclaje
   (`anchors.top: effectsSection.bottom`) resuelve bien, pero leído desde un *binding*
   devuelve **NaN** (y no avisa), así que `parent.height - effectsSection.bottom` daba
   NaN y el panel quedaba con alto 0, invisible. Hay que escribir
   `effectsSection.y + effectsSection.height`. Se diagnosticó con un rótulo temporal
   dentro del propio `MainWindow.qml`.

Verificado con el binario compilado y la biblioteca real (218 pistas): con *Virus (How
About Now)* (128 BPM, Fm) como referencia, el panel lista candidatas de 128-130 BPM en
Cm/Fm.

### 6.9 Controlador Numark MixTrack Pro II (2026-10-13)

**Síntoma:** el log repetía `MIDI output message 0x90 0x63/0x64 has invalid MixxxControl
[Channel1]/[Channel2], flanger`, el **fader de canal no hacía nada**, el **plato** no
hacía scratch (hacía pitch) y Mixxx **se cerraba** solo.

**Causa raíz: estaba seleccionado el mapeado equivocado.** El `[ControllerPreset]` de
`mixxx.cfg` apuntaba a `Numark Mixtrack Pro.midi.xml`, que es el del **Mixtrack Pro
original (2011)**, no el del Pro II. El mapeado bueno para este hardware existe y se
llama `Numark Mixtrack 2.midi.xml` (`<name>Numark MixTrack (Pro) II</name>`).

Los CC del Pro original no coinciden con el Pro II:

| Control | MixTrack Pro | MixTrack Pro II |
|---|---|---|
| Fader deck 1 (`[Channel1],volume`) | CC `0x08` | CC `0x16` |
| Fader deck 2 (`[Channel2],volume`) | CC `0x09` | CC `0x07` |
| Tacto del plato deck 1 (`wheelTouch`) | nota `0x4E` | nota `0x4D` |
| Tacto del plato deck 2 | nota `0x4D` | nota `0x4E` |
| Crossfader / volumen master | `0x0A` / `0x17` | iguales |

Por eso el crossfader sí iba y los faders no; y como el sensor de tacto estaba cruzado,
el script creía que tocabas el *otro* deck y el giro caía en la rama de pitch.
**Arreglo:** apuntar `[ControllerPreset] MixTrack_Pro_II` a
`C:/Program Files/Mixxx/controllers/Numark Mixtrack 2.midi.xml` (verificado: carga sin
un solo `invalid MixxxControl`).

**El cierre era un `abort()`**, no un fallo aleatorio: el volcado da
`0xC0000409` + `FAST_FAIL_FATAL_APP_EXIT` y la pila
`qFatal → mixxx_debug_assert → PlayerManager::groupForDeck → scratchDisable →
ControllerScriptInterfaceLegacy::timerEvent`. En `Numark-Mixtrack-Pro-scripts.js`
(línea 599) el timer llamaba a `jogWheelStopScratch()` **sin el deck**, y
`scratchDisable(undefined)` acaba en `groupForDeck(-1)`, que falla la assertion; como el
build es `DEBUG_ASSERTIONS_FATAL=True`, Mixxx se cierra. **Arreglado** pasando `deck` y
añadiendo una guarda `if (!(deck >= 1)) return;` (también en el script del Pro II, por el
mismo motivo).

**Controles inválidos en `Numark Mixtrack Pro.midi.xml`** (corregidos, para los usuarios
del Pro de verdad): el grupo `[Flanger]` y `[ChannelN],flanger` ya no existen en Mixxx 2.x.

- Knobs de FX deck 1 → `[EffectRack1_EffectUnit1_Effect1],parameter1/parameter2`
- Knobs de FX deck 2 → `[EffectRack1_EffectUnit2_Effect1],parameter1/parameter2`
- LED del botón FX → `[EffectRack1_EffectUnitN_Effect1],enabled`
- El script usa `NumarkMixTrackPro.fxSlotForDeck(deck)` y el brake toma la velocidad del
  2º knob (`parameter2`), en vez de `[Flanger],lfoDelay`.

**Plato:** el mapeado del Pro II (igual que el del Platinum: *«ignore touch events if not
in vinyl mode»*) solo hace scratch con el deck **sonando** si su **modo vinyl/scratch**
está encendido, y `scratch_mode` arranca en `[false, false]`. El botón por deck es el de
nota `0x48` (A) / `0x50` (B), con LED. Con el modo apagado el giro es *nudge* (pitch), que
es el comportamiento tipo CDJ. Sin sonar, el tacto sí habilita el scratch.

**Herramientas de verificación añadidas** (sin hardware): un arnés Node que ejecuta el
script con un `engine` simulado, dispara **todos** los `<script-binding/>` del XML con
sus notas reales y comprueba (a) que ningún deck inválido llega a
`scratch*/brake` (la clase de fallo que cierra Mixxx) y (b) que cada control que se toca
aparece en el volcado `src/test/co_dumps/co_dump_inital.csv`. ⚠️ Ojo con ese volcado: no
incluye **alias** (`filterLow/Mid/High`, `headVolume`) ni algunos controles reales
(`meta`), así que esas ausencias son falsos positivos.

**Pendiente:** los LEDs no lucen porque el log dice `MIDI device "MixTrack Pro II" not
open for output!` (con varias instancias de Mixxx abiertas es lo esperable; conviene
comprobar con una sola).

### 6.10 Los pads de hotcue tapaban los botones de ajustes del deck (2026-10-13)

**Síntoma:** en PulseDeck, justo debajo de los pads numerados 1-8 asomaban 6 botones con
iconos (un rayo, un imán con forma de "U", una diana, expulsar, repetir y un candado) y
el usuario no podía identificarlos.

**Qué son:** el bloque **DeckSettings** de cada deck, o sea:

| Icono | Control | Para qué sirve |
|---|---|---|
| ⚡ rayo | `[ChannelN],slip_enabled` | Slip mode: el deck sigue corriendo por debajo mientras haces loops/cues |
| 🧲 imán (la "U") | `[ChannelN],quantize` | Cuantiza los cues/loops a la rejilla del beat |
| ✛ diana | `[ChannelN],beats_translate_curpos` (der. `beats_translate_match_alignment`) | Corrige la rejilla del beat a la posición actual |
| ⏏ expulsar | `[ChannelN],eject` | Descarga la pista del deck |
| ↻ repetir | `[ChannelN],repeat` | Repite la pista al terminar |
| 🔒 candado | `[ChannelN],keylock` | Mantiene el tono al cambiar el tempo |

Arriba de ellos está la fila de **estrellas** (valoración de la pista).

**Causa:** `DeckSettingsPlaceholder` mide **63 px de alto** (`implicitHeight: 63`) y en
NeonDeck vive dentro de la fila del overview, que allí mide 63 px (`overviewAndSpinnyRow`)
así que entra justo. En PulseDeck la tira del overview es fina a propósito (18 px), así que
el bloque se desbordaba hacia abajo y la fila de pads (26 px, que se dibuja después) le
pasaba por encima y tapaba las dos primeras filas; solo se veían los 3 botones de abajo.

**Arreglo:** en vez de engordar la tira (que le robaría 45 px al plato), el bloque se
desplaza para empezar **debajo** de los pads:

- `res/skins/PulseDeck/Deck/OverviewRow.qml`: nueva propiedad
  `property real deckSettingsTopOffset: 0` y el bloque pasa de `anchors.fill: parent` a
  `anchors.left/right/top` + `anchors.topMargin: root.deckSettingsTopOffset`; además el
  panel de fondo sigue al bloque (`height: deckSettingsTopOffset + deckSettings.implicitHeight`)
  en lugar de quedarse en los 18 px de la tira.
- `res/skins/PulseDeck/Deck/FullDeck.qml`: `deckSettingsTopOffset: hotcuePads.visible ? 44 : 18`
  (18 de la tira + 26 de los pads).

El deck compacto deja el offset en 0 y no cambia nada; NeonDeck no se toca (ahí ya entra).
Verificado con captura en los dos decks: los 6 botones y las estrellas se ven completos.

**De paso, dos controles muertos detectados en el log** (avisos
`QmlControlProxy: Requested CO ... does not exist`). Los dos están arreglados en el
apartado 6.11; aquí queda constancia de cómo se detectaron:

- `[Channel1],beatloop_size_double` → el nombre correcto es `[ChannelN],loop_double`.
- `[Skin],timing_shift_buttons` → da aviso de arranque aunque el skin sí lo crea.

### 6.11 Botones muertos del deck y de los ajustes de waveform (2026-10-13)

**1) Las flechitas ± del tamaño de loop/beatjump no hacían nada.** El comportamiento
`res/qml/Deck/BeatSizeSpinBoxBehavior.qml` tiene dos modos: si recibe `decrementKey` e
`incrementKey` dispara esos controles, y si no, recorre él mismo los tamaños estándar
(1/32 … 64) escribiendo `beatloop_size`/`beatjump_size`. Los skins le pasaban
`beatloop_size_halve`/`beatloop_size_double` y `beatjump_size_halve`/`beatjump_size_double`,
que **no existen** (el dump solo tiene `loop_halve`/`loop_double`; los de beatjump no
existen en absoluto), así que `trigger()` caía en un control nulo y la flecha no hacía
nada. **Arreglo:** quitar esas cuatro claves para que el propio componente recorra los
tamaños estándar, que es justo lo que hace el `BeatSpinBox` de C++ del skin LateNight
clásico. Ficheros: `res/qml/Deck/BeatJump.qml` (compartido) y
`Deck/TransportLoopBeatjumpCompact.qml` + `Deck/TransportLoopBeatjumpPlaceholders.qml` en
PulseDeck, NeonDeck y LateNightQML.

**2) El interruptor "Hotcue Shift Buttons" parecía muerto.** Comprobado que **sí
funciona**: `res/qml/Deck/BeatgridControlsBehavior.qml` lee `[Skin],timing_shift_buttons`
y con el valor 1 aparecen las flechitas de desplazar hotcues (verificado con captura).
El problema real era otro: esas flechitas viven dentro del panel de controles de beatgrid,
cuya visibilidad es `[Skin],show_beatgrid_controls`, y **ese control no tenía ningún
interruptor en la interfaz** en los skins QML (en el LateNight clásico sí está en los
ajustes, `skin_settings.xml`), así que si lo tenías a 0 la opción de los hotcues no hacía
nada visible. **Arreglo:** añadir "Beatgrid Buttons" al desplegable Waveforms del toolbar
(nuevo `ControlProxy` `showBeatgridControlsControl`) y dejar "Hotcue Shift Buttons"
habilitado solo cuando el panel de beatgrid está activo. Ficheros: `Toolbar/Toolbar.qml`
en PulseDeck, NeonDeck y LateNightQML.

**Nota sobre el aviso de arranque:** `QmlControlProxy: Requested CO "[Skin]",
"timing_shift_buttons" does not exist, resetting...` es cosmético; el proxy se reengancha
cuando el `SkinControlBootstrap` crea el control y el valor acaba aplicándose (lo verifiqué
poniéndolo a 1: las flechitas aparecen). Aun así, para dejar el log limpio se le dio un
`defaultValue: 0.0` explícito a ese creador (y al de `equal_4deck_waveforms`, que daba el
mismo aviso) en el `SkinControlBootstrap.qml` de PulseDeck, NeonDeck y LateNightQML: así el
control existe desde el principio. Comprobado: el log ya no tiene ningún
`does not exist`. El valor persistido en `mixxx.cfg` sigue ganando al default, que es lo
que se quiere (con `show_beatgrid_controls` a 1 las flechitas salen igualmente).

**Estado:** todo esto (más los arreglos del mapeado Numark) está compilado e instalado en
la app de `C:\Program Files\Mixxx` (MSI generado a las 13:4x del 13-10-2026).

---

### 6.12 Windows: cortes de audio, pantalla completa y ajuste "Deck Size" (2026-10-13)

**1) Audio cortado ("se escucha cortado").** Causa: `%LOCALAPPDATA%\Mixxx\soundconfig.xml`
tenía `api="Windows DirectSound"` con el dispositivo *"Controlador primario de sonido"*, que
es la vía emulada y compartida de Windows y la causa clásica de glitches. **No era CPU**: con
una pista sonando, Mixxx medía 26 % de un núcleo (2 % de 12 núcleos) — se midió dando play
por **OSC** (`/Channel1/play 1` a `127.0.0.1:11119`, que es donde escucha).

Arreglo aplicado (verificado en el log: `Opened PortAudio stream successfully` +
`Started stream successfully`):

```xml
<SoundManagerConfig api="Windows WASAPI" deck_count="4" force_network_clock="0"
                    latency="5" samplerate="44100" sync_buffers="2">
 <SoundDevice deviceIndex="8" name="Altavoces (MixTrack Pro II)">
  <output channel="0" channel_count="2" index="0" type="Master"/>
  <output channel="2" channel_count="2" index="1" type="Headphones"/>
 </SoundDevice>
</SoundManagerConfig>
```

Dos trampas del Mixtrack Pro II (útiles si hay que rehacerlo):

- **Solo acepta 44 100 Hz**; con 48 000 el stream falla con `Invalid sample rate`.
- Su interfaz expone **4 canales de salida**, así que hay que usarlos todos (master 1-2 +
  auriculares 3-4). Con `channel_count="2"` falla con `Invalid number of channels`.
- Latencia real reportada por PortAudio: 46 ms (se puede bajar con `latency="3"`, con
  cuidado). Los tres avisos `SoundManager::configureOutput` del log son informativos.

**2) No dejaba poner pantalla completa en Linux.** Bug real: con los skins QML la ventana
visible la crea el `QQmlApplicationEngine` (`QmlApplication`), mientras que el menú nativo
(el que sale en la barra superior cuando el escritorio tiene menú global, y también F11)
estaba conectado a `MixxxMainWindow::slotViewFullScreen()`, que hace `showFullScreen()` sobre
**la ventana QWidget, que está oculta**. De ahí que no pasara nada. Arreglo:
`QmlApplicationProxy::setWindowFullScreen()` (nuevo, mismo patrón que
`registerReloadCallback`) y `MixxxMainWindow::slotViewFullScreen()` lo usa primero, con
`m_pMenuBar->onFullScreenStateChange()` para que el tick del menú quede correcto. Ficheros:
`src/qml/qmlapplicationproxy.h`, `src/qml/qmlapplication.{h,cpp}` y
`src/mixxxmainwindow.cpp`. En Windows no se notaba porque ahí se usa el menú dentro de la
ventana (el del skin), que ya funcionaba. **Necesita `.deb` nuevo** para llegar a Linux.

**3) El deck "se desordena" al ocultar el mixer (fotos de Linux).** No es un fallo de
Linux: el skin tiene un ajuste **"Deck Size"** que se aplica *cuando el mixer está oculto*
(`[Skin],latenight_deck_size_without_mixer`), con **default Compact**. El perfil de Windows
lo tenía en **Full** y el de Linux en Compact, y el deck compacto es precisamente el de las
fotos (plato a la izquierda, sin pads de hotcue, ajustes flotando). Se reprodujo en Windows
poniendo Compact y coincide. **Solución:** toolbar → botón **4 DECKS ▾** → **Deck Size:
Full** (esa opción sólo aparece con el mixer oculto; con el mixer visible pone
"hide mixer to select").

**Estado:** MSI recompilado e **instalado** a las 13:58 (13:56 el binario) con los tres
arreglos de skin de 6.11, el panel de ajustes del deck de 6.10 y el de pantalla completa.

---

### 6.13 Sin sonido al desconectar la mesa: respaldo al dispositivo del sistema (2026-10-13)

**Síntoma:** con el Mixtrack Pro II desconectado, Mixxx arrancaba **sin sonido**. En el log:

```
18:16:57.646 Warning [Main] Output sound device clock reference not set! Using "Flujo de red"
18:16:57.646 Debug [Main] 1 output sound devices opened      <-- solo el dispositivo de red
```

**Por qué:** `SoundManagerConfig::validateDevices()` descarta los dispositivos configurados que
no están conectados (`if (devicesMatchingByName == 0) continue;`), así que la configuración se
quedaba con **cero salidas**; `setupDevices()` solo abría el dispositivo interno de red y
`MixxxMainWindow` preguntaba con `noOutputDlg`. Peor: como el guardado posterior
(`writeToDisk()`) reconstruye el XML **desde lo que quedó en memoria**, el dispositivo que el
usuario había elegido se **borraba del `soundconfig.xml`** al arrancar sin la mesa (de hecho
el del usuario quedó así: `<SoundManagerConfig .../>`, sin ningún `SoundDevice`).

**Arreglo (3 piezas):**

1. **Respaldo al dispositivo por defecto del sistema.** `SoundDeviceEnumerator` tiene ahora
   `getDefaultOutputDevice(api)` (implementado en `PortAudioEnumerator` con
   `Pa_GetHostApiInfo(api)->defaultOutputDevice`, que es lo que el sistema tiene seleccionado)
   y `SoundManager::getDefaultOutputDevice()`. En `validateDevices()`, si **ningún**
   dispositivo de salida configurado está conectado (o no hay ninguno configurado), se añade
   una salida `Main` estéreo en ese dispositivo por defecto, se registra en
   `m_fallbackDevices` (**no se escribe a disco**) y el routing de los dispositivos
   desconectados se conserva (`m_substitutedDevices`) pero **no se reporta como error** en
   `setupDevices()` (si no, saldría el diálogo de "dispositivo no encontrado"). Resultado: la
   configuración del usuario sobrevive y al reconectar la mesa se vuelve a usar sola.
2. **Negociación de la frecuencia de muestreo.** Nuevo
   `SoundDevice::isSampleRateSupported()` (implementado en `SoundDevicePortAudio` con
   `Pa_IsFormatSupported`, y solo `paInvalidSampleRate` cuenta como "no soportado").
   `SoundManagerConfig::checkSampleRate()` ya no se fía solo de la API: prueba la frecuencia
   configurada y, si los dispositivos no la aceptan, la del dispositivo
   (`getDefaultSampleRate()`) y las de la API. **Esto era un fallo real:** el endpoint WASAPI
   de los altavoces Realtek solo acepta 48 000 Hz, así que con `samplerate="44100"` el stream
   fallaba con `Error opening stream: Invalid sample rate` → diálogo de error → Mixxx
   **cerraba** (y de ahí los volcados). Ahora pasa a 48000 automáticamente y, al revés, con la
   mesa (44100) volvería a 44100.
3. **`SoundManager::getDevice(deviceId)`** para resolver un id de la configuración al
   dispositivo enumerado (lo usa el punto 2).

Verificado con el Mixtrack desconectado (stderr del proceso):

```
warning [Main] Configured output device is not connected, using the default "Altavoces (Realtek(R) Audio)"
warning [Main] The configured output devices cannot play 44100 Hz Hz, using 48000 Hz Hz
```

**Trampa importante — compilación incremental con clases virtuales.** Los abortos
`0xc0000409` / `FAST_FAIL_STACK_COOKIE_CHECK_FAILURE` (y en el depurador
`HEAP: Invalid address specified to RtlFreeHeap` dentro de `LV2Backend`) que aparecieron al
probar estos cambios **no eran del código nuevo**: al añadir un método virtual a
`SoundDevice`/`SoundDeviceEnumerator`, Ninja **no recompiló** `sounddevice.cpp` ni
`sounddeviceenumerator.cpp` (sus `.obj` seguían siendo del día anterior), de modo que las
tablas de funciones virtuales quedaron incoherentes entre módulos y la llamada al método nuevo
acabó en corrupción de memoria. **Hay que hacer `cmake --build . --clean-first`** tras tocar
una clase virtual (y desconfiar del build incremental en este árbol).

**Diagnóstico aparte:** con la salida de audio fallando, el diálogo de error lleva a
`exit(0)` y **el cierre a veces revienta** (volcado con falta de acceso en `Qt6Sql.dll`), es
decir, el "y se cierra" que ya se había visto. Con la salida de audio correcta no ocurre.

---

### 6.14 Asistente de dispositivo de audio (estilo VirtualDJ) (2026-10-13)

**Qué hace:** cuando conectas o desconectas un controlador mientras Mixxx está abierto,
aparece un diálogo que **nombra el dispositivo reconocido** y te deja elegir, y la salida de
audio cambia en caliente (sin reiniciar). También avisa cuando el dispositivo que estabas
usando desaparece y pasa al audio del ordenador. Es lo que pidió el usuario ("al conectar una
mesa… o uso de PC sin mesa", "tú define el mejor diálogo").

**Diálogo (caja nativa de Qt, botones e idioma del sistema):**

- Título: *Configuración de audio* · Texto: **"Controlador detectado: Altavoces (MixTrack Pro
  II)"** · Texto informativo: "Su tarjeta de sonido puede usarse para la salida principal y los
  auriculares. Audio del ordenador: Altavoces (Realtek(R) Audio)".
  Botones: **[Usar el controlador]** (por defecto) / **[Usar el audio del ordenador]** y la
  casilla **"Recordar esta elección y no volver a preguntar"**.
- Al desconectarse: "El dispositivo *X* ya no está conectado" + "Usando el audio del
  ordenador: *Y*".

**Cómo se detecta** (`src/soundio/audiodeviceassistant.{h,cpp}`):

- `mixxx::systemConnectedDeviceNames()` (nuevo, `src/util/systemdevices.{h,cpp}`) lee **la lista
  de dispositivos MIDI del sistema con winmm** (Windows) o las tarjetas ALSA de
  `/proc/asound/cards` (Linux). Es una lectura barata y **no toca** ni el motor de audio ni el
  backend de controladores que usa Mixxx, así que se puede consultar cada 2 s sin consecuencias.
- Si la lista cambia (y no hay ningún diálogo modal abierto), el asistente reenumera de verdad:
  `SoundManager::reinitializeDevices()` (nuevo: cierra los streams, `Pa_Terminate`/`Pa_Initialize`
  y vuelve a casar la configuración con los dispositivos encontrados) y luego aplica el
  dispositivo elegido con `setConfig()`. **Eso corta el audio un instante**, inevitable: PortAudio
  sólo enumera al arrancar, así que un dispositivo conectado con Mixxx abierto no se ve de otra
  forma.
- El "controlador" se reconoce **por nombre** (comparando el nombre MIDI con el de los
  dispositivos de salida, normalizando a letras/dígitos en minúscula: `MixTrack Pro II` casa con
  `Altavoces (MixTrack Pro II)`) → **funciona con cualquier marca**, no hay lista de modelos.
- Al elegir la mesa se configura `Main` (canales 1-2) y, si el dispositivo tiene 4 o más canales,
  `Headphones` (canales 3-4), como hacía la configuración del usuario. La frecuencia la ajusta
  `checkSampleRate()` (6.13).
- La elección se guarda en `[Sound],controller_audio_device` = `mixer` | `computer` (vacío =
  preguntar siempre).

**Dónde se crea:** en `QmlApplication` (`src/qml/qmlapplication.cpp`), **no** en
`MixxxMainWindow`: con skins QML el arranque lo lleva `QmlApplication` y `MixxxMainWindow` ni
siquiera se instancia (main.cpp usa un `if/else`), así que la versión en `mixxxmainwindow.cpp`
queda sólo para builds sin QML. ⚠️ Por lo mismo, el menú nativo (`wmainmenubar.cpp`) **no** es el
que se ve en PulseDeck/NeonDeck.

**Entrada de menú:** *Vista → Asistente de configuración de audio* (verificado que aparece ✓)
en los tres skins (`Toolbar/Toolbar.qml` + `Toolbar/ApplicationMenuShortcuts.qml`) y en el menú
compartido (`res/qml/ApplicationMenuActions.qml` + `MainMenuBar.qml`), con atajo `Ctrl+8`
configurable. El callback de C++ (`QmlApplicationProxy::registerAudioDeviceCheckCallback()`)
**se registra** (verificado en el log).

**Verificado:** el asistente avisa al conectar y al desconectar y **cambia el audio en caliente
sin reiniciar** (confirmado por el usuario con el Mixtrack enchufado/desenchufado en marcha). El
vigilante arranca y ve los dispositivos del sistema (`AudioDeviceAssistant: watching devices
QList(...)`), sin avisos espurios en bucle (no detecta cambios falsos). El clic sintético en la
entrada de menú que hice con automatización no disparó la acción, así que si el menú tampoco
responde al clic real, sospechar de `Mixxx.Application.checkAudioDevices()` (vía QML) y probar a
invertirlo con un control `[Skin],check_audio_devices`.

**Trampa de enlace:** `winmm` no lo hereda el ejecutable al estar enlazado `PRIVATE` en la
biblioteca estática, hay que añadirlo también a los targets `mixxx` y `mixxx-test`.

### 6.15 Experimentos de WASAPI y de controlador en caliente: REVERTIDOS (2026-10-13)

Se probaron dos mejoras y **las dos se han quitado** porque rompían lo que ya funcionaba. Queda
aquí escrito para no repetirlas:

**1. Forzar que el asistente eligiera WASAPI (revertido).** Para que el asistente no eligiera
MME se le puso un orden de preferencia de APIs (`Windows WASAPI` primero y luego las demás). Con
la mesa conectada eligió WASAPI (índice 8, 4 canales, 44100 Hz) y el audio sonó, pero:

- la latencia real subió de **23 ms (MME) a 46 ms (WASAPI)** en este Mixtrack → el usuario lo
  notó como "lag";
- el retoque venía con un reintento ("look again for a better API") que **repetía la
  reenumeración de audio hasta 3 veces**, y cada reenumeración bloquea el hilo principal varios
  segundos → el usuario lo describió como "se cuelga".

`listOutputDevices()` ha vuelto a ser la lista simple (`getHostAPIList()` en su orden natural,
MME primero en Windows), que es el estado que el usuario confirmó como bueno: **el asistente no
elige WASAPI, pero el diálogo, el cambio en caliente y el mapeado de la mesa funcionan**. Si
alguien quiere WASAPI, se elige a mano en *Preferencias → Sonido* (es lo que el usuario tenía
antes configurado).

**2. Cargar el mapeado MIDI al enchufar (revertido y pendiente).** Si Mixxx arranca **con la
mesa desconectada** y la mesa se enchufa después, el audio sí cambia pero **los controles no
responden**: `PortMidiEnumerator` sólo enumera dispositivos en su constructor (`Pm_Initialize`)
y `queryDevices()` no vuelve a mirar, así que el controlador nuevo no existe para
`ControllerManager`. Se intentó con un `refreshHostDevices()` virtual (`Pm_Terminate` +
`Pm_Initialize` antes de `queryDevices()`) y **el build se caía al arrancar**:

```
INVALID_POINTER_READ_c0000005_mixxx.exe!ControllerManager::updateControllerList
FAULTING_SOURCE_LINE: src/controllers/controllermanager.cpp (233)
```

Causa: añadir un **virtual** a `ControllerEnumerator` deja obsoletos los `.obj` de las clases
derivadas, y el despacho de `queryDevices()` se fue a la basura (mismo problema de objetos
obsoletos que en 6.13). Para reintentarlo hace falta:

1. `cmake --build . --clean-first` **después** de tocar la clase base (no incremental), y
2. sincronizar el cambio con `dlgprefcontrollers` (el comentario de upstream en
   `updateControllerList()` avisa de que el hotplug puede dejar conexiones colgando y provocar
   crashes).

Mientras no se haga, el comportamiento es el de siempre: si la mesa estaba enchufada al
arrancar, el asistente la deja funcionando sin reiniciar; si se enchufa después, hay que
reiniciar Mixxx para que cargue el mapeo.

**Lo que sí se ha mantenido:** el arreglo de `SoundManagerConfig::addOutput()`/`addInput()`
(quitan la marca de "dispositivo temporal" del respaldo al añadir un dispositivo), necesario
porque sin él la elección del asistente **no se guardaba** en `soundconfig.xml`.

**Regla final de elección de API** (la que quedó, probada y confirmada por el usuario): el
asistente **mira primero la API que ya está en la configuración** y, si esa API lista el
dispositivo, lo usa; sólo si no lo lista recorre el orden natural de `getHostAPIList()`. Ni
fuerza WASAPI ni degrada a MME: la configuración del usuario manda (WASAPI + mesa con Main y
Headphones en `soundconfig.xml`, 44100 Hz, 4 canales). Un re-escaneo por evento (≈3,6 s al
quitar la mesa, ≈0,1 s al ponerla), sin los reintentos que se quitaron.

**También corregido:** `audiodeviceassistant.cpp` no incluía `moc_audiodeviceassistant.cpp`, así
que su `moc` quedaba en `mocs_compilation.cpp` y **el build limpio fallaba** con el test
`moc_included_test.cpp` ("mocs_compilation.cpp not empty"). Como los builds incrementales
anteriores no compilaban ese objetivo, no se había visto hasta el `--clean-first` de hoy.

**Prueba final (21:46, confirmada por el usuario):**

```
21:46:40 devices changed QList("Microsoft GS Wavetable Synth")        (mesa quitada)
21:46:43 using "Altavoces (Realtek(R) Audio)"  48000 Hz              → diálogo de aviso ✓
21:46:54 devices changed QList(..., "MixTrack Pro II")                (mesa puesta)
21:46:54 asking about "Altavoces (MixTrack Pro II)"                   → diálogo ✓
21:46:56 using "Altavoces (MixTrack Pro II)"   44100 Hz (WASAPI)      → mesa OK ✓
```

`soundconfig.xml` queda `api="Windows WASAPI"` + la mesa con `Master` (0) y `Headphones` (2),
44100 Hz ✓ (ahora sí se guarda).

**MSI nuevo:** `dist\mixxx-2.7-alpha-427-g0fffd3d0d0-modified-amd64.msi` (13-10 **22:19**, 74,7 MB).
Para empaquetar hace falta `DOTNET_ROOT=C:\Users\giova\.dotnet` (el `wix.exe` es un tool de
.NET y sin eso falla con "You must install .NET to run this application"). **Falta instalarlo**
(es cosa del usuario, con UAC).

**`.deb` nuevo:** `dist\mixxx-2.7.0-alpha-unknown-x86_64.deb` (13-10 **22:20**, 18,6 MB), rehecho
en WSL con `wsl_rebuild_deb.sh` (sesión `e7306c0d`, carpeta `files/`). Compila limpio en Linux
(`Linking CXX executable mixxx` + `mixxx-test` ✓, incluido el test de `moc` ✓) y se verificó que
el binario llevaba el asistente (`strings usr/bin/mixxx | grep -c AudioDeviceAssistant` = 12 ✓).
Los dos paquetes llevan 6.13, 6.14, 6.15 y 6.16.

### 6.16 Botón "Audio Setup Assistant" sin respuesta y botón sándwich ilegible (2026-10-13)

**1. El botón del menú no hacía nada visible.** En realidad **sí** funcionaba: el log mostraba
`AudioDeviceAssistant: checking for devices now` cada vez que el usuario hacía clic. El problema
era que no daba **ninguna respuesta** cuando había una elección recordada o la mesa ya estaba en
uso: sólo reenumeraba y volvía a aplicar la misma configuración, así que desde fuera parecía
muerto.

Arreglado en `audiodeviceassistant.{h,cpp}`:

- `checkForDevicesNow(bool askUser = false)`: el camino automático (enchufar/desenchufar) usa el
  valor por defecto; el **botón del menú** pasa `true` (en `qmlapplication.cpp` y en los dos
  llamadores de `mixxxmainwindow.cpp`).
- Con `askUser` se ignora la elección recordada y **siempre** pregunta: si hay mesa, sale el
  diálogo de siempre (con el nombre del dispositivo reconocido → *"Controlador detectado:
  Altavoces (MixTrack Pro II)"*); si no hay ninguna, sale un aviso nuevo
  (`informAboutNoController()`): *"No controller with its own sound card was found."* + *"Using
  the audio of the computer: X"*.
- Mientras se buscan los dispositivos se pone el cursor de espera (la reenumeración bloquea la
  interfaz un instante), y se quita antes de abrir los diálogos.

Verificado con Ctrl+8 real enviado a la ventana: `22:17:26 checking for devices now` →
`asking about "Altavoces (MixTrack Pro II)"` ✓.

**2. El botón sándwich se ponía gris pero no se veían las líneas.** El icono era un `Image` con
`btn__menu.svg`, que es **claro** (`#d2d2d1` en classic, `#a89a8c` en palemoon), y al abrirse el
menú el fondo pasa a `toolbarButtonActiveBackgroundColor` (`#9a9a9a`) → el icono desaparecía.

Arreglado en `Toolbar/Toolbar.qml` de **PulseDeck** y **NeonDeck**: se dibuja con la fuente de
iconos igual que el resto de botones de la barra,

```qml
Controls.IconLabel {
    iconColor: menuButton.popup.visible
            ? LateNightTheme.toolbarButtonActiveTextColor   // negro
            : LateNightTheme.toolbarButtonInactiveTextColor
    iconName: "bars"   // \uf0c9, ya estaba en el mapa del tema
    iconSize: 13
}
```

Verificado con capturas: cerrado = líneas claras sobre fondo oscuro; abierto = líneas **negras**
sobre el gris. El desplegable (`btn__fx_selector_down.svg`) ya es oscuro (`#888`), así que no
hacía falta tocarlo.

⚠️ **LateNightQML** tiene el mismo botón con el mismo icono claro, pero su tema **no** tiene la
fuente de iconos (`iconGlyph`/`iconFontFamily`) ni `Controls/IconLabel.qml`, así que ahí sigue
igual. Si molesta, la alternativa es añadir el icono con un `Text` y el mapa de glifos a ese
tema, o dibujar las tres líneas con `Rectangle`s.

---

## 7. Configuración actual del usuario

`%LOCALAPPDATA%\Mixxx\mixxx.cfg` (formato `clave valor`, **separado por espacio**, no
`clave=valor`)
- `ResizableSkin PulseDeck` — apunta al skin nuevo (5.10) para poder verlo. Para volver
  al que trae el código por defecto basta con **borrar esa línea** (o poner
  `ResizableSkin NeonDeck`).
- `Scheme NeonDeck` — la paleta la comparten los dos skins.
- `[Skin] show_stem_controls 0`, `show_beatgrid_controls 0` — desactivados para que la
  banda de waveforms se vea como la captura (ondas a todo el ancho).
- `[OSC] Enabled 1`, `BindAddress 127.0.0.1`, `InputPort 11119`, `OutputPort 11118`

Copia de seguridad de la config antes de tocarla: `%TEMP%\mixxx.cfg.bak`.

Comprobación de que todo va bien (líneas esperadas en `%LOCALAPPDATA%\Mixxx\mixxx.log`):
```
QmlApplication: QML skin loaded via preferences
OSC: listening on "127.0.0.1" 11119 sending feedback to "127.0.0.1" 11118
```

---

## 8. Orden sugerido para la próxima sesión

1. ~~Commitear lo que ya funciona~~ — **el usuario no quiere commitear** (no es su
   repositorio). Todo sigue en el árbol de trabajo.
2. ~~**Compilar** (`build_windows_release.cmd`)~~ — **HECHO el 2026-10-12 23:35**:
   `.msi` reconstruido e instalado (exit 0) con las fuentes (Rajdhani + Font Awesome),
   el tipografiado de widgets nativos (`src/qml/qmlapplication.cpp`), los dos skins
   (NeonDeck + PulseDeck) y la mejora de AutoDJ armónico (6.6). Verificado lanzando el
   binario **instalado** (`shots\installed-01.png`): carga PulseDeck con la paleta nueva
   y los dos decks con música. `[Auto DJ] HarmonicMixing` quedó **activado** en la config
   (se apaga en *Preferences → Auto DJ*).
   Si se vuelve a tocar C++ o QML hay que repetir: `build_windows_release.cmd` → cerrar
   Mixxx → `msiexec /i <msi> /qn` con `-Verb RunAs` (el .exe en ejecución bloquea la
   instalación).
3. Que el usuario termine de añadir música a la biblioteca (ya hay 215 pistas).
4. Validar IA + OSC con música real (el AutoDJ armónico ya está validado en 6.6).
5. Aspecto restante, por orden de impacto:
   - **Panel STEMS por deck**: la captura de VDJ lo tiene; Mixxx sí soporta stems pero
     sólo en la banda de waveforms, así que habría que hacer un panel nuevo (5.10 punto 4).
   - 5.10 punto 2: renombrar la paleta a "pulsedeck" (ojo con `assetScheme`).
   - 5.6: los iconos ya están prácticamente todos en Font Awesome (sólo quedan los pads
     numerados y los badges de sync, a propósito; ver 5.6).
6. ~~**Instalar el `.msi` pendiente**~~ — **HECHO (13-10 00:38)**: instalado el build con
   tipografía, mixer, navegador (QSS), **atajos de teclado arreglados (6.7)** y **diálogos
   oscuros (5.7)**. Verificado con Ctrl+P.
7. **VERSIÓN FINAL (13-10 00:59)** — el usuario pidió "déjalo así y compila para deb y msi
   ya final". Paquetes en `c:\Users\giova\Desktop\mixxx\dist\` (ver 6.3). El `.msi` está
   instalado y verificado (`shots\final-instalado.png`: PulseDeck, colores anteriores y
   ondas en dos tonos). **Nada commiteado**, como pidió el usuario.
