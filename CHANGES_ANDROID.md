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
