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
