# H3-PS3 — lector inicial de mainmenu.map

Fecha: 2026-10-22 (America/Bogota). **Resultado: compilado para PS3 y ejecutado correctamente en RPCS3.**

Se implementó un lector C11 independiente que abre una copia local de `mainmenu.map` mediante CellFs, consulta su tamaño y lee únicamente los primeros 12.288 bytes. Valida firmas, versión, tamaño y cadenas de longitud acotada. No necesita reservar los 55 MB del mapa en la memoria del programa.

## Datos reconocidos durante la ejecución

| Campo | Valor |
|---|---|
| Archivo abierto por PS3 | /app_home/mainmenu.map |
| Nombre | mainmenu |
| Versión | 11 |
| Tamaño real y declarado | 55.234.560 bytes |
| Build | 11855.07.08.20.2317.halo3_ship |
| Escenario | levels\ui\mainmenu\mainmenu |
| Firmas | head en 0x0; foot en 0x2FFC |
| CRC32 de los 12 KiB de cabecera | 51094e28 |
| Comprobaciones del parser | 8/8 |
| Salida de la aplicación | H3MAP_EXIT 0 |

El CRC32 calculado por el código PS3 coincide con una referencia calculada de manera independiente con Python/zlib en Windows. El informe que escribió el programa también coincide en nombre, versión, tamaño, build y escenario.

## Implementación y comprobaciones

- `src/map_reader/map_header.c` y `.h`: parser propio, con decodificación big-endian explícita y sin casts a estructuras empaquetadas.
- `src/map_reader/main.c`: apertura y lectura con CellFs, comprobaciones, informe de texto y visualización por sysutil. El diálogo se cierra automáticamente tras 25 segundos.
- `CMakeLists.txt`: nuevo target `h3ps3_map_reader`, enlazado con PS3DK. Se conserva el programa hello anterior.
- `config/rpcs3-map-reader.yml`: perfil con Vulkan en GTX 1650 Ti, dos hilos de compilación, ventana sin pantalla completa y cierre automático del emulador al terminar el proceso.

Las ocho comprobaciones se ejecutaron dentro del programa PS3: cabecera real válida, entrada truncada, puntero nulo, firma inicial corrupta, versión no soportada, tamaño incoherente, firma final corrupta y nombre sin terminador. Las alteraciones se hicieron en buffers de memoria; no en el mapa original.

La primera compilación detectó una posible truncación al formar el mensaje de error. Se corrigió usando un límite explícito para esa cadena. La compilación final pasó con `-Wall -Wextra -Werror`.

Persistieron los avisos USB/MIDI del entorno restringido de RPCS3, pero la ejecución alcanzó `H3MAP_PASS`, escribió su informe, mostró el resultado y salió con código 0. No se cambiaron drivers ni permisos del sistema.

## Repetir la ejecución

```powershell
Set-Location C:\H3-PS3
.\scripts\run-map-reader.ps1
```

El script verifica y prepara una copia del mapa, compila el target y lo arranca en RPCS3. Para preparar y compilar sin ejecutar:

```powershell
.\scripts\run-map-reader.ps1 -BuildOnly
```

Ejecutable: `C:\H3-PS3\dist\map-reader\h3ps3_map_reader.self` (87.952 bytes). Su carpeta debe contener `mainmenu.map`; el script se encarga de prepararlo desde la copia del juego. Un archivo SELF es un ejecutable PS3 y se abre con RPCS3, no como un EXE de Windows.

SHA-256 del SELF probado: `e0690d844308e7ef5a219cb529e8c32fb61d14211e38672a89f64fcbc1be5c50`.

## Evidencia y conservación de datos

- `dist/map-reader/map-reader-result.txt`: informe generado por el ejecutable PS3.
- `research/map-reader-validation.json`: comparación automática de resultado y referencia.
- `logs/map-reader-TTY.log`: `H3MAP_PASS`, lectura de cabecera y `H3MAP_EXIT 0`.
- `logs/map-reader-RPCS3.log`: registro de ejecución y cierre automático.
- `logs/map-reader-build-final.txt`: compilación final.
- `evidence/map-reader-rpcs3.png`: captura del resultado visible.
- `scripts/analysis/verify-map-reader.py`: verificación independiente de campos, CRC y hashes.

El original y la copia preparada coinciden con el hash del inventario inicial: `edce049a6e199a766d2d85ade1efcb284dd919ab0e883efe6ac4c2f28ec350e0`. El original no se modificó. La carpeta dist queda excluida de Git y no se incluyeron datos del juego entre los entregables de este informe.

## Alcance del hito

Se reconoció la cabecera de esta variante retail de Halo 3, versión 11 y sin compresión declarada. El CRC32 es de la cabecera, no una validación completa del contenido del mapa. Todavía no se interpretan índices de tags ni recursos, ni se dibuja el menú del juego. La ejecución comprobada fue en RPCS3, no en una consola física.

El siguiente frente es documentar la traducción de direcciones y la tabla de tags de este build, con comprobaciones de límites antes de leer recursos. Este lector aporta la entrada de archivos necesaria para ese trabajo.

Formato contrastado con la inspección del archivo local y la definición pública de cabecera Gen3 de TagTool: https://github.com/TheGuardians/TagTool/blob/master/TagTool/Cache/CacheFile/CacheFileHeaderGen3.cs . Se utilizó la descripción del formato; el parser fue escrito para este proyecto.
