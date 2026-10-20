Cambios de esta versión (ver android/README.md para el estado):
- NUEVO src/platform/native/touch/touchpad.{h,cpp}: estado del overlay y puente JNI.
- MODIFICADO src/platform/native/pads.cpp: el overlay entra en la lectura del pad solo en partida.
- MODIFICADO src/platform/native/ui/bindings.h y bindings.cpp: InPlay() pasa a ser público (fuera del namespace anónimo).
- NUEVO android/overlay/: CrashActivity, TouchOverlay, TouchBridge y los PNG del touch.
- NUEVO android/tools/assemble.sh y .github/workflows/android.yml.

Fase A (GLES del renderer):
- NUEVO src/platform/native/graphics/hw/glsl.{h,cpp}: adaptación de shaders y contexto (escritorio / TWIN_GLES).
- MODIFICADO gl.h / gl.cpp: DrawBuffers; glPointSize y dual-source opcionales; dual-source detectado.
- MODIFICADO hwgs.cpp: shaders vía Glsl::Adapt; main con outFactor solo con TWIN_DUAL_SOURCE; blend SRC1 solo con dual-source;
  buffer -> circuitBuffer en el shader de merge; GL_PROGRAM_POINT_SIZE solo en escritorio; atributos del contexto por Glsl.
- MODIFICADO display.cpp, overlayrender.cpp: compilación vía Glsl::Adapt y atributos del contexto por Glsl.
- NUEVO android/tools/check_shaders.py y paso en .github/workflows/android.yml.

Corrección de CI (v3): se quita android-actions/setup-android@v3 (falla en la imagen actual: sdkmanager 'tools' ya no existe; el SDK viene preinstalado) y setup-java pasa a v5.

Ícono y nombre (v4): android/overlay/res/ con el ícono de Crash en todas las densidades; assemble.sh lo instala, fija el
ícono y la etiqueta en el manifest y quita el ícono adaptativo de la plantilla.

Fase C (primer APK, v5): libmain.so de prueba (android/native/stub_main.c: ventana SDL3 amarilla) y la capa táctil
compilada con el NDK por Android.mk (generado por assemble.sh). jni/SDL enlaza con SDL3 en el build. minSdk 24 y arm64.

Fase B (parcial, v6): el jugador importa su ISO PAL (se copia a la carpeta privada de la app) y el núcleo lo recibe por
TWINSANITY_ISO; SettingsFolder usa TWIN_SETTINGS_DIR en Android (android_glue.cpp, NativeBridge.java, CrashActivity).
Pendiente: data.cpp (ver android/README.md).

Fase B (v7): generación de data.cpp en CI (android/tools/gen_data.sh) con asm/ desde un repo privado; paso en el workflow.

v8: el repo privado se toma del dueño del repo principal (github.repository_owner), no de un nombre fijo.

v9: el conversor necesita también assets/ y SLES_525.68 (además de asm/); el workflow los copia del repo privado.

v10: el paso del toolchain de PS2 busca el compilador dentro del tarball y fija PS2DEV a su raíz (con ps2sdk/).

v11: convert_data.py: clean() ignora alignas(...); el error de tamaño dice qué layouts parecidos tiene (diagnóstico de Matrix4x4).

v12 (revisión): la actividad lanzadora es com.giopixel.crashtwinsanity.CrashActivity (antes quedaba SDLActivity y el
overlay no corría); namespace de la app = com.giopixel.crashtwinsanity; assemble.sh falla si un reemplazo no coincide.
bindings.cpp restaurado al original: la función pública es NativeUi::PlayingNow() (antes rompía el ámbito del namespace
anónimo). CI: glslang (nombre del binario de Ubuntu), apt-get update, y el ensamblado del proyecto antes de gen_data.sh
(necesita las cabeceras de SDL).

v13: convert_data.py imprime cuántos layouts da cada target y cuáles tienen nombres de Matrix/Vector (diagnóstico de Matrix4x4).

v14: convert_data.py: el probe fuerza el layout de cada registro de las cabeceras (sizeof), porque clang solo vuelca los
layouts que se usan; antes faltaban incluso Vector4 (147/199 layouts). Sin probar con clang: el g++ del probe compila (0 errores).

v15 (revisión del build en CI): assemble.sh escribe LOCAL_SRC_FILES de jni/src con rutas relativas (antes absolutas) y
comprueba que cada fuente exista; build.gradle fija options.encoding = 'UTF-8' para el Java (el overlay tiene texto en
español y no depender del locale del runner). Verificado: shaders (42 variantes, 0 fallos), toolchain PS2 (la URL y la
raíz que busca el workflow), JNI (nombres de touchpad.cpp y android_glue.cpp contra las declaraciones native de Java),
compilación en sintaxis de touchpad.cpp y android_glue.cpp con jni.h, y assemble.sh ejecutado de punta a punta contra
SDL 3.2.0. No verificado: ndk-build y Gradle (necesitan NDK y Android SDK).

v16 (el juego en el build de Android, pasos 1 y 2 de la hoja de ruta): jni/src/Android.mk lo genera ahora
android/tools/make_android_mk.py con los mismos globs que native/CMakeLists.txt (src/game, core, reused_ps2.txt,
src/platform/native), con movie_off.cpp en lugar de movie/movie.cpp (sin FFmpeg: Open falla y el juego salta la película).
- SDL 3.4.18 (la versión del escritorio; SDL_LoadPNG existe desde 3.4): assemble.sh y el NDK r28c, que es el que fija la plantilla 3.4.18.
- entry.cpp incluye SDL_main.h en Android (SDL llama a SDL_main, no a main).
- assemble.sh reescrito: el namespace se reemplaza con `namespace =` o sin él; APP_PLATFORM se fija para cualquier nivel.
- gen_data.sh hace también script_layouts.py y vu1_translate.py (VU1 y VU0), como el CMake.
Verificado aquí: las 288 fuentes compilan en sintaxis con clang 18 y las banderas de Android contra SDL 3.4; el código
solo-Android (entry, touchpad, android_glue, movie_off) también; assemble.sh corre de punta a punta.
No verificado (necesita NDK y asm/ del repo privado): gen_data.sh, la compilación con ndk-build, el enlazado. El
enlazado puede pedir los stand-ins de native/tools/make_stubs.py (el CMake de escritorio los genera); no están todavía.

v17 (menús, pantalla y fps en el Android): src/platform/native/ui/mouse.cpp: en Android un dedo es el puntero de los menús (un toque
selecciona, SDL manda también los eventos de ratón de ese toque y se cuentan una vez). graphics/presenter.h y display.cpp:
NativeGraphicsWindowAspect(), la proporción de la ventana. fixes.cpp: en Android WideTvAspect() sigue la proporción de la
pantalla, entre 16:9 y 21:9 (con la opción ultrawide), así que no hay bordes y no se estira; proyección, vista y ratón usan
ese mismo valor. ui/options.cpp: en Android, por defecto 120 Hz (refresh_60hz = 2) y ultrawide encendido; en escritorio sigue
igual. Un options.txt ya guardado conserva sus valores (se restablecen en Ajustes). Sin compilar ni probar en el Poco todavía.

v18 (rendimiento, título, pantalla): ui/options.cpp: por defecto 60 Hz también en Android (con 120 el juego da pasos de 60 y
repite cuadros; se veía a tirones). Sigue en 21:9. touch/touchpad.{h,cpp} y pads.cpp: un toque en la pantalla de título
(StateTitle) presiona Start unos cuadros, así que se pulsa "Start" tocando. ui/mouse.cpp: el dedo en título llama a PressStart.
CrashActivity.java: pantalla completa con WindowInsetsController en Android 11+ (barras ocultas, vuelven solo con un deslizamiento
del borde y se ocultan otra vez); los botones de volumen cambian el volumen sin el panel del sistema (flags 0).
Sin compilar ni probar en el Poco todavía.

v22 (fps y barras, sin compilar): TouchOverlay.java: el overlay ya no se redibuja en cada vblank (postInvalidateOnAnimation desde onDraw); solo al tocar, al editar o cuando el juego entra o sale de partida, que se mira cada 250 ms. CrashActivity.java: las barras se vuelven a ocultar 500 ms después de onResume y del foco, por si SDL aplica sus banderas después.
Pendiente para el FPS: medir en el Poco con TWIN_FRAME_STATS (hoy solo se activa con variable de entorno, en Android no hay forma de ponerla).
