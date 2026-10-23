# H3-PS3

Investigación experimental sobre una posible adaptación de Halo 3 de Xbox 360 a PlayStation 3. Fecha de actualización: 2026-10-22 (America/Bogota).

## Estado actual

- Fase 1: compilación homebrew conseguida y mensaje observado en RPCS3.
- Fase 2: copia XEX cargada en Ghidra, 15.831 funciones definidas desde .pdata y pseudocódigo exportado de 39 funciones únicas del arranque.
- No existe un ejecutable de Halo 3 para PS3. El SELF de prueba es un programa independiente.

## Equipo y herramientas verificadas

Windows AMD64, versión NT 10.0.26300 / 26H2; Intel Core i5-10300H; 15,78 GiB RAM; Intel UHD Graphics y NVIDIA GeForce GTX 1650 Ti. RPCS3 detectó y usó la NVIDIA mediante Vulkan.

Herramientas portátiles dentro de `tools/`:

| Herramienta | Versión ejecutada |
|---|---|
| Git | 2.56.0.windows.3 |
| CMake | 4.4.4 |
| Ninja | 1.13.2 |
| PS3DK | 0.25.0 |
| Compilador PPU GCC | 12.4.0 |
| Compilador SPU GCC | 9.5.0 |
| RPCS3 | 0.0.43-20375-0dec46b1 |
| Firmware PS3 instalado en RPCS3 | 4.93 |

WSL no está instalado y no fue necesario. Los scripts configuran variables en el proceso; no se modificó el PATH persistente ni se usó un SDK propietario filtrado. Los archivos fuente de este proyecto son independientes del motor de Halo 3.

## Prueba homebrew realizada

El código C11 de `src/main.c` usa un diálogo sysutil. CMake y el helper de PS3DK generaron `h3ps3_hello.elf`, `h3ps3_hello.self` (85.232 bytes) y `h3ps3_hello.fake.self`. `readelf` identificó el ELF como PowerPC64, ELF64, big-endian.

Se observó dentro de RPCS3 el texto:

> H3-PS3: Hola PlayStation 3! Prueba experimental. Selecciona Si o No para salir.

La captura está en `evidence/rpcs3-message.png`. El TTY disponible tras la ejecución contiene `H3PS3_DIALOG_OPENED` y `H3PS3_NO_RESPONSE`. Por tanto, la visualización está confirmada, pero no se validó una respuesta del diálogo ni la rama de salida por confirmación. El usuario interrumpió el control de Windows con Escape y después explicó que fue accidental. Por su nueva instrucción no se continuaron pruebas del homebrew.

Para compilar si se necesita en el futuro:

```powershell
Set-Location C:\H3-PS3
.\scripts\build.ps1
```

El perfil `config/rpcs3-smoke.yml` limita compilación LLVM/shaders a dos hilos y usa Vulkan con la GTX 1650 Ti. El perfil de teclado está en `config/H3PS3-keyboard.yml`; su funcionamiento todavía no está validado. `scripts/run.ps1` es auxiliar y requiere revisión del perfil de entrada antes de afirmar que valida interacción.

## Problemas encontrados y resolución

- Acceso CIM denegado: hardware consultado por registro y API Win32.
- Descargas lentas: PS3DK descargado por bloques, ensamblado y verificado mediante SHA-256 del archivo completo.
- Instalación de firmware headless: RPCS3 devolvió error al terminar, aunque su log confirmó instalación de 4.93 y se comprobaron los archivos de dev_flash. La interfaz gráfica abrió correctamente después.
- Preparación inicial de shader interpreter: se interrumpió tras un uso elevado de recursos. El perfil local cambió a Async Recompiler sin shader interpreter y el siguiente arranque alcanzó el mensaje.
- Avisos USB/MIDI y creación de una clave durante el inicio en entorno restringido: persistieron, pero no impidieron mostrar el mensaje. No se modificaron drivers, seguridad ni permisos USB del sistema.
- Primer script enumeraba SELF del firmware: se corrigió para mostrar únicamente los artefactos del proyecto.
- Git se inicializó, pero el cambio de nombre de rama no pudo actualizar HEAD por permisos. No se creó ningún commit; HEAD permanece en master.

Logs en `logs/`: `build-first.txt`, `build-final.txt`, `elf-header.txt`, `rpcs3-first-run.log`, `rpcs3-visible-no-input.log` y `checksums.json`. Algunos intentos fueron interrumpidos y no deben interpretarse como ejecuciones exitosas completas.

## Ingeniería inversa iniciada

Origen autorizado por el usuario: `C:\H3-PS3\HALO 3`. La copia permanece sin modificar.

El inventario contiene 166 archivos y 6.094.128.838 bytes. `default.xex` declara Title ID `4D5307E6`, nombre PE `halo3_cache_release.exe`, base `0x82000000`, entrada `0x82582A98`, cifrado normal y compresión básica. Sus dependencias declaradas incluyen `xam.xex` y `xboxkrnl.exe`. No se encontraron fuentes C/C++ ni PDB.

Resultados completos y límites del análisis: `research/README.md`, `research/audit.json` y `research/inventory.csv`. Script reproducible: `scripts/analysis/audit_game.py`.

El usuario autorizó las herramientas necesarias en adelante. Se instalaron y verificaron Ghidra 12.1.3, Temurin JDK 21.0.12.1+1 y XEXLoaderWV 12.1.3. Se cargó el XEX y se exportaron 39 funciones únicas de su arranque. El pseudocódigo todavía no es fuente recompilable. Resultados actuales, correcciones y comandos: research/DECOMPILACION.md. Proyecto guardado: research/ghidra-project/Halo3Research.gpr. Exportaciones finales: research/startup-final y research/engine-entry-final.

## Fuentes y procedencia

- PS3DK: https://github.com/FirebirdTA01/PS3DK/releases/tag/v0.25.0
- RPCS3: https://github.com/RPCS3/rpcs3-binaries-win
- Firmware oficial Sony: https://www.playstation.com/es-es/support/hardware/ps3/system-software/
- Formato XEX2: https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/util/xex2_info.h
- Ghidra: https://github.com/NationalSecurityAgency/ghidra
- Loader XEX: https://github.com/zeroKilo/XEXLoaderWV

PS3DK ZIP verificado: SHA-256 `90163d789bf3f2655314152eb73b76258ac900e817b2932172bfdca561c649f5`. Firmware: SHA-256 `158471fd834f8ea8036136b6aab43cd86c7ba73d79ca30e0af3c0fe0001cf365`; MD5 coincidente con el identificador del enlace oficial `a2b60b6ac1d2e49e230144345616927c`.

## Primer lector de recursos ejecutado en PS3

Se implementó y compiló `h3ps3_map_reader.self`, y se ejecutó en RPCS3: leyó la cabecera real de mainmenu.map, reconoció versión 11, build, escenario y tamaño de 55.234.560 bytes. El CRC32 de cabecera coincidió con Windows y las ocho comprobaciones del parser pasaron. El programa registró H3MAP_EXIT 0.

Ejecutar: `.\scripts\run-map-reader.ps1`. Resultado completo, límites y evidencias: `research/LECTOR_MAPAS.md`. El alcance actual es reconocimiento de cabecera; la carga de tags y el renderizado del menú siguen pendientes.

## Índices, bitmaps y página de recursos probados

El target h3ps3_tag_browser se ejecutó en RPCS3 con los 25 mapas locales: 251.128 entradas de tags y 34.136 descripciones de imágenes coincidieron con la referencia independiente en Windows. Descomprimió además una página real de mainmenu.map (8.429.568 bytes), con CRC32 coincidente 34934881. Terminó con H3INDEX_EXIT 0 y 15/15 comprobaciones del índice.

Ejecutar: `.\scripts\run-tag-browser.ps1`. Documentación: `research/LECTOR_TAGS_RECURSOS.md`. El motor, el renderizado de texturas y la ejecución de Halo 3 siguen pendientes; este SELF es una herramienta de lectura homebrew, probada en emulador, no en consola física.

## Primera textura real presentada en RPCS3

El target h3ps3_texture_viewer decodifica y presenta `ui\halox\campaign\locked_ui` desde mainmenu.map. Sus 26.412 píxeles coinciden byte por byte con la referencia Python (CRC32 d69c08d1). Se comprobó visualmente el candado de campaña en el framebuffer de RPCS3. Código, licencia GPL del módulo de direcciones, comandos y límites: research/TEXTURA_PS3.md. Ejecutar: `.\scripts\run-texture-viewer.ps1`. Esto todavía no ejecuta el motor ni las escenas del juego.
