# H3-PS3 — primera decompilación del arranque

Fecha: 2026-10-22 (America/Bogota). Estado: primer análisis estático completado; no hay todavía un port de Halo 3 ni fuentes recompilables.

## Resultados comprobados

- Ghidra 12.1.3, Temurin JDK 21.0.12.1+1 y XEXLoaderWV 12.1.3 instalados de forma portable dentro de `C:\H3-PS3\tools`.
- Los tres paquetes se verificaron por SHA-256 antes de utilizarlos. Java y javac respondieron con su versión real.
- El loader reconoció, descifró y descomprimió `default.xex`, cargó sus 12 secciones PE y declaró 15.831 funciones a partir de `.pdata`. Esto no significa que se hayan decompilado ni revisado esas 15.831 funciones, ni que todas pertenezcan al motor.
- Se procesaron 590 registros de importación: 300 referencias IAT y 290 thunks. Se modelaron los 290 thunks como funciones externas; el script terminó con cero fallos de modelado.
- Se exportaron **39 funciones únicas** del arranque en dos pasadas de 20 funciones, con una función común. Hay ensamblado, pseudocódigo, llamadas, símbolos y mapa de memoria.
- Ambas pasadas finales generaron 20 salidas y cero salidas con `halt_baddata` o advertencias de instrucciones inválidas. Persisten otras advertencias en ocho funciones únicas, entre ellas tipos/alias de globales, saltos indirectos y código no alcanzable. La ausencia de cortes por instrucciones inválidas no valida por sí sola la semántica del pseudocódigo.
- El proyecto Ghidra quedó guardado correctamente. No se ejecutó el juego ni se continuaron pruebas en RPCS3, conforme a la nueva dirección del usuario.

El SHA-256 de `default.xex`, comprobado otra vez al terminar, coincide con el original inventariado y con la copia de trabajo:

`1779cf9e1ee9bdabf59ff612b4b60855063369ad0cd87b09abb759699e75ccae`

## Recorrido de arranque encontrado

| Dirección | Observación o interpretación | Evidencia y límite |
|---|---|---|
| 0x82582A98 | Entrada declarada del XEX; envoltorio de arranque con inicialización y análisis de argumentos | Cabecera, ensamblado y pseudocódigo. Los nombres originales y tipos no están recuperados. |
| 0x821982E8 | Crea un hilo, espera si obtiene un handle y entra en una espera permanente | Sus instrucciones pasan 0x82198338 como dirección de inicio a un wrapper de ExCreateThread. |
| 0x82580C78 | Wrapper hacia ExCreateThread | La cadena de llamadas decompilada alcanza la importación resuelta. El prototipo todavía es provisional. |
| 0x82580C88 | Wrapper de espera | La cadena alcanza NtWaitForSingleObjectEx. |
| 0x8219A308 | Bucle infinito, sin retorno | Una única instrucción `b 0x8219a308`, cuerpo de cuatro bytes. Se marcó no-retorno en el modelo. |
| 0x82198338 | Entrada del hilo de aplicación | Inicializa estructuras y llama a 0x82197C70; si devuelve verdadero, llama a 0x821A9790 y después a 0x82197F30. |
| 0x82197C70 | Candidata a inicialización de la aplicación | Muchas inicializaciones y llamadas; devuelve 1. También usa XamLoaderGetLaunchDataSize y XNotifyDelayUI. Función no revisada completamente. |
| 0x821A9790 | Candidata a bucle principal | Bucle condicionado por un global, sincronización, operaciones atómicas y llamadas repetidas. No se ha probado que represente por sí sola todo el ciclo de juego. |
| 0x82197F30 | Candidata a cierre/limpieza | Se invoca después del bucle candidato y contiene limpieza de estados/estructuras. Interpretación provisional. |

Los nombres genéricos de Ghidra se conservaron. Las descripciones anteriores son anotaciones de investigación, no nombres originales recuperados.

## Correcciones realizadas al modelo

La primera exportación intentó decompilar stubs de importación como si contuvieran implementaciones locales; produjo cortes por datos de instrucción inválidos. Se crearon funciones externas para las API importadas, sin sustituir sus implementaciones ni escribir bytes sintéticos en la copia del XEX.

Se identificaron 78 saltos incondicionales hacia thunks externos y se modelaron como tail calls mediante metadatos de flujo de Ghidra. Por ejemplo, el wrapper en 0x82589D40 ahora se representa como una llamada a MmFreePhysicalMemory en lugar de detenerse en el thunk.

La advertencia en 0x8219832C se debía a que el decompilador suponía que el bucle en 0x8219A308 podía retornar y continuaba hasta cuatro bytes cero. Se comprobó la instrucción de salto a sí misma y se marcó la función como no-retorno. No se parcheó ese relleno como si fuera código.

Ghidra inicialmente intentó guardar la selección del JDK fuera del proyecto y no pudo. El script redirige APPDATA, LOCALAPPDATA y preferencias Java solo en el proceso para guardarlas dentro de `tools/ghidra-user`. No cambia esas variables de forma persistente en Windows.

## Archivos útiles

- `research/ghidra-project/Halo3Research.gpr`: proyecto guardado, con sus datos asociados en `.rep`.
- `research/startup-final/`: primera pasada desde la entrada XEX.
- `research/engine-entry-final/`: segunda pasada desde el hilo de aplicación y los wrappers seleccionados.
- En ambas carpetas: `*.c` son pseudocódigo sin revisar; `*.asm` es el ensamblado exportado; `functions.tsv`, `symbols.tsv`, `startup-calls.tsv`, `memory-blocks.tsv` y `xbox-imports.tsv` contienen metadatos.
- `research/startup-final/control-flow-model.tsv`: 78 ajustes de tail calls y la anotación de no-retorno. La segunda pasada no repitió esos ajustes ya aplicados.
- `research/decomp-status.json`: lista de las 39 funciones únicas, advertencias y hashes comprobados.
- `research/inventory.csv` y `research/audit.json`: inventario y cabeceras de la copia local.
- `logs/ghidra-import-retry-console.txt`, `logs/ghidra-startup-final-console.txt` y `logs/ghidra-engine-final-console.txt`: ejecución real de importación/exportación.

Las carpetas iniciales `startup`, `startup-reviewed` y `engine-entry` se conservan como resultados intermedios. Para evaluar el estado actual se deben usar las carpetas que terminan en `final`.

## Reproducir

Con las herramientas ya instaladas y el proyecto existente:

```powershell
C:\H3-PS3\scripts\analysis\run-ghidra.ps1 -OutputName startup-final
C:\H3-PS3\scripts\analysis\run-ghidra.ps1 -OutputName engine-entry-final -Entry 82198338 -ExtraSeeds 82580c78,82580c88,8219a308
```

El parámetro `-Import` se usa solamente al crear un proyecto nuevo desde `research/input/default.xex`; no es necesario para abrir el proyecto existente. Las exportaciones están acotadas a 20 funciones por pasada; no son una decompilación automática de todo el juego. El script no inicia el análisis automático global de Ghidra.

## Límites y siguiente trabajo técnico

Los `.c` contienen tipos `undefined`, inferencias de registros, saltos indirectos no resueltos y prototipos provisionales. Los helpers de guardado/restauración de registros y la convención de llamadas Xbox requieren más trabajo. No deben compilarse directamente con PS3DK ni presentarse como el código fuente original.

El loader deja el metadato Image Base de Ghidra en cero, aunque las secciones están mapeadas en sus direcciones virtuales 0x820... y el punto de entrada coincide con la cabecera. No se ha aplicado un rebase: mover las secciones para corregir solo esa etiqueta sería incorrecto. Las referencias se deben revisar usando las direcciones del mapa de memoria exportado.

El siguiente frente concreto es reconstruir tipos y límites de funciones alrededor de 0x82197C70 y 0x821A9790, resolver sus llamadas indirectas y separar servicios Xbox de lógica del juego. Después se podrá seleccionar un subsistema pequeño para reimplementación portable. Gráficos, shaders, audio y recursos todavía no se han convertido a PS3.

## Procedencia de herramientas

- Ghidra 12.1.3: https://github.com/NationalSecurityAgency/ghidra/releases/tag/Ghidra_12.1.3_build
- XEXLoaderWV 12.1.3: https://github.com/zeroKilo/XEXLoaderWV/releases/tag/12.1.3
- Temurin JDK 21: https://adoptium.net/temurin/releases/?version=21

SHA-256 verificados: Ghidra `93a5d11a9ad510622acaaf908c556a7b9b764d338e78a7567f3689bf5081fd54`; loader `afdaf425bb8e16706f28a453a836ac84fcd0d5000f93a2275a02105807d030a3`; JDK `f9d6e191ab098c0d416e7d588a24420a8621cd2f4720dab2459b8b7b2d2d8b4e`.

El usuario autorizó las herramientas necesarias en adelante para el proyecto. No se usó código propietario filtrado; se trabajó con la copia aportada y herramientas públicas, y los archivos del juego permanecieron locales.
