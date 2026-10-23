# H3-PS3: primera textura renderizada

El ejecutable `h3ps3_texture_viewer.self` lee la textura `ui\halox\campaign\locked_ui` de la copia local de `mainmenu.map`, la decodifica en PPU y la presenta en un framebuffer de PS3 a través de GCM. Se vio el candado azul de la interfaz de campaña en RPCS3, Vulkan, 1280x720.

**No es todavía Halo 3 ejecutándose.** Es un visor de una textura real, con lector y conversión propios. No ejecuta código del motor, escenas, shaders de Xbox 360, simulación, audio ni controles del juego. No se ha probado una PS3 física. El dibujo usa CPU para componer el framebuffer y GCM para presentarlo; aún no utiliza un material/textura en un pipeline 3D RSX.

## Prueba realizada

- Tag 2935, recurso e8fc0799, imagen de 284x93.
- Página requerida 29, codec Deflate raw; 6.052.037 bytes comprimidos, 18.344.640 descomprimidos.
- CRC32 de página 0007c6ef; offset de segmento 7.426.048.
- El lector resuelve definición y fixups, verifica salt, tipo, dimensiones, D3DFormat y rangos; rechaza estructuras no soportadas.
- Conversión de bloques Xenos tiled DXT1/BC1 con intercambio de bytes de 16 bits, interpolación RGB565 y alfa BC1.
- Los 26.412 píxeles, 105.648 bytes ARGB y 1.704 bloques coinciden exactamente con un programa Python que recorre las direcciones Xenos en sentido inverso. CRC32 ARGB d69c08d1.
- Framebuffer 1280x720, CRC32 2d30fff2, imagen centrada a escala 3 y fondo de transparencia.
- Ocho comprobaciones C del decodificador: extremos de color, interpolación, transparencia, datos truncados, salida insuficiente, dimensión nula y recorte de imagen 1x1.
- La aplicación presenta 1.800 cuadros y sale automáticamente. Log esperado: H3TEXTURE_TESTS 8/8, H3TEXTURE_PASS, H3TEXTURE_EXIT 0.

## Ejecutar

Con las herramientas portátiles y la copia legal ya presentes en C:\H3-PS3:

```powershell
Set-Location C:\H3-PS3
.\scripts\run-texture-viewer.ps1
python .\scripts\analysis\verify_texture.py
```

El script monta `HALO 3/maps` en `/dev_usb000`, guarda configuraciones VFS previas si difieren, espera la salida y comprueba un log nuevo. No modifica el juego. Las salidas quedan en `dist/texture-viewer`: SELF, TSV de página y `texture-argb.bin`. El verificador produce `research/texture-validation.json` y una imagen PNG para inspección.

## Código y licencias

- src/texture/main.c: resolución de recurso y página, descompresión, verificación y ejecución.
- src/texture/texture.c: decodificador y pruebas. La ecuación de direcciones Xenos se adapta del proyecto público TagTool (GPL-3.0-or-later); atribución y licencia en src/texture/COPYING. El verificador usa las ecuaciones inversas de esa misma fuente.
- src/texture/preview.c: composición y presentación GCM.
- src/cache/cache_index.c y src/map_reader/map_header.c: lectores usados en las pruebas anteriores.

Referencia pública de formatos y ecuaciones: https://github.com/TheGuardians/TagTool/blob/master/TagTool/Bitmaps/XboxGraphics.cs ; definiciones de recursos: https://github.com/TheGuardians/TagTool/blob/master/TagTool/Tags/Resources/BitmapTextureInteropResource.cs . No se utilizó código propietario filtrado.

## Alcance pendiente

El visor selecciona actualmente una textura concreta y acepta D3DFormat 0x1a207f73, imágenes 2D de una capa y recursos locales no intercalados. Existe selección de página primaria/secundaria a partir de la definición, pero la prueba de imagen cubre el caso primario; no se da por validada la imagen de páginas secundarias. Las variantes no cubiertas fallan explícitamente. Faltan otros formatos, tiling/mip tails de casos generales, archivos compartidos, cubemaps y recursos intercalados.

El siguiente trabajo necesario para una escena es ampliar el cargador de geometría y la traducción de materiales, además de un pipeline 3D. Reconstruir la lógica del motor sigue siendo un trabajo independiente de poder mostrar sus recursos. La decompilación existente no equivale a código C portable ni permite recompilar directamente Halo 3 para PS3.
