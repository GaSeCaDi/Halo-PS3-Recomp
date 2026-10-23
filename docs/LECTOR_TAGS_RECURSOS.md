# H3-PS3: lector de tags y recursos

Estado probado: ejecutable homebrew para PS3 ejecutado en RPCS3. **Halo 3 todavía no arranca y no existe un motor portado.** La salida visible es un diálogo de diagnóstico; no es el menú del juego. No se ha probado hardware PS3 físico.

## Resultados

- 25 mapas procesados: 23 contienen índice de tags; campaign.map y shared.map tienen esa sección vacía.
- 251.128 entradas comparadas campo por campo con un lector Python independiente.
- 34.136 registros de imágenes de tags bitm comparados en los 23 mapas: dimensiones, formato numérico, flags, mipmaps y referencias de recursos.
- 15/15 comprobaciones positivas y negativas del índice ejecutadas en PS3: límites, identificadores, salt, errores de lectura, firmas y tablas corruptas.
- Recurso e1630000 de shaders\default_bitmaps\bitmaps\random4_warp: página requerida 0 de mainmenu.map. Descompresión Deflate raw desde 3.267.468 bytes a 8.429.568 bytes, consumiendo exactamente la entrada y produciendo exactamente el tamaño declarado.
- CRC32 comprimido b750cd29, página 34934881, primeros 64 bytes del segmento 761e5231. Coinciden con Python/zlib en Windows.
- H3RESOURCE_PASS y H3INDEX_EXIT 0 en el log de RPCS3.

## Código y ejecución

Fuentes propias C: src/cache/cache_index.c y src/tag_browser/main.c. CMake target h3ps3_tag_browser. El programa se enlaza con zlib de PS3DK y las bibliotecas homebrew de PS3.

```powershell
Set-Location C:\H3-PS3
.\scripts\build.ps1 -Target h3ps3_tag_browser
.\scripts\run-tag-browser.ps1 -SkipBuild
python .\scripts\analysis\verify_tag_index.py
python .\scripts\analysis\verify_bitmaps.py
```

El lanzador configura el montaje virtual /dev_usb000 para C:/H3-PS3/HALO 3/maps/. Conserva una copia de vfs.yml si necesita sustituir una configuración distinta. Espera el cierre de RPCS3 y exige marcas de éxito en un log nuevo. El diálogo se cierra automáticamente en 30 segundos. No requiere pulsar botones.

Binario: dist/tag-browser/h3ps3_tag_browser.self. Los mapas se abren exclusivamente para lectura. Las salidas TSV y el resultado de texto se escriben en dist/tag-browser mediante /app_home. No se modifican los originales.

Referencias independientes reproducibles:

```powershell
python .\scripts\analysis\inspect_tag_index.py --maps '.\HALO 3\maps' --out '.\research\tag-index-reference'
python .\scripts\analysis\inspect_bitmap_resources.py
```

Los scripts de bitmaps y verificadores actualmente esperan C:\H3-PS3. Los resultados de validación están en research/tag-index-validation.json y research/bitmap-validation.json; los logs, en logs/tag-browser-TTY.log y logs/run-tag-browser-final.log.

## Formatos implementados y límites

Cabeceras retail big endian versión 11, cuatro secciones, direcciones virtuales de tags, tablas de grupos/instancias/globales, nombres y comprobación de salt. Se validan rangos antes de leer y se imponen límites de memoria. Los descriptores bitm usan raíz de 164 bytes, dos bloques de imágenes de 48 bytes y referencias de hardware de 8 bytes. Los valores de formato se exportan numéricamente; aún no se convierten píxeles.

El descompresor implementa un caso concreto: recurso normal de bitmap del mapa mainmenu, página requerida local, codec Deflate 0 y tamaños hasta 32 MiB. Rechaza otros tipos, referencias intercaladas o archivos compartidos distintos. No resuelve todavía todos los recursos, fixups, páginas opcionales ni mipmaps. No guarda ni distribuye la página descomprimida.

La lectura correcta de una página no demuestra que se haya reconstruido una textura. Faltan el layout Xbox 360, orden de bytes, tiling, compresión de bloques y subida a RSX. Después deben implementarse geometría, materiales/shaders, escena, entrada, audio, simulación y servicios del motor. El pseudocódigo de Ghidra existente no es fuente recompilable: sigue dependiendo de memoria y servicios de Xbox 360.

## Procedencia

Implementación C original basada en observación de la copia local y en estructuras públicas de TagTool: https://github.com/TheGuardians/TagTool (Bitmap, ResourceGestalt, ResourceLayoutTable, ResourceData y ResourcePage). No se utilizó código propietario filtrado. El paquete de código entregable excluye archivos originales del juego, SDK, firmware y resultados de descompilación.
