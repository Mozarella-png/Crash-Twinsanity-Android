# Crash Twinsanity en Android (en construcción)

Estado de esta versión (v1). Lo que está y lo que falta, para no confundirlo con un port jugable:

**Hecho en esta versión**

- Overlay táctil (`android/overlay/`): botones X, Círculo, Cuadrado, Triángulo, L1, R1, Start-Pausa y el stick de movimiento
  (sin cruceta, sin stick de cámara). Cada control se puede mover, cambiar de tamaño y de opacidad en el modo "Editar"
  (botón arriba a la derecha, en partida). La posición se guarda en el teléfono.
- Los toques fuera de los controles pasan al juego como ratón, así los menús se controlan tocando.
- La capa nativa de touch (`src/platform/native/touch/`) y su integración en `pads.cpp`: los botones y el stick entran en la
  lectura del pad solo en partida.
- Workflow de GitHub Actions (`.github/workflows/android.yml`) que arma el proyecto con la plantilla de SDL3 y compila el APK.

**Fase A (renderer a GLES 3.0), hecha**

- Los shaders siguen escritos en GLSL 330 core y `src/platform/native/graphics/hw/glsl.cpp` los adapta al contexto: en
  GLES cambia la versión (`300 es`), las precisiones por defecto, y se quita `noperspective`.
- El dual-source (el segundo output del fragment para el factor de blend) es opcional. Si el contexto lo tiene (core en
  escritorio, `EXT_blend_func_extended` en GLES), se usa como antes. Si no, las mezclas que lo necesitan pasan al
  blending en shader, que ya existía para los casos que la GPU no hace exacto. Eso puede cambiar algún efecto de
  transparencia respecto al PC; hay que comprobarlo en el Poco.
- `glDrawBuffer` se cambió por `glDrawBuffers` (existe en ambos), y `glPointSize` y `glBindFragDataLocationIndexed` son
  opcionales en la carga.
- `android/tools/check_shaders.py` valida los 42 shaders (con sus defines) en ambos modos con glslang, en cada push.
  Limitación: glslang no comprueba ES 3.00 directamente, así que se valida como ES 3.10 y los built-ins de 3.1 se
  revisaron a mano.
- Para compilar el renderer en Android hay que definir `TWIN_GLES` (lo hará el CMake de la fase C).

**Pendiente (bloqueante)**

- La librería nativa `libmain.so` todavía no se compila para Android: falta el CMake del NDK (fase C) y los datos del
  juego en tiempo de ejecución.
- El ultrawide sin estirar en Android (la proyección 21:9 del juego sobre la pantalla del teléfono) no está conectado todavía.
- Los datos del juego: el build nativo los genera desde la ejecutable del disco. En Android tienen que venir del zip que
  importa el jugador, no del APK.
- Nada de esto se ha compilado ni probado en un teléfono todavía.

**Probar el overlay en el Poco X8 Pro Max**: cuando el build nativo esté listo, instalar el APK, entrar en partida y pulsar
"Editar" para ajustar los controles.
