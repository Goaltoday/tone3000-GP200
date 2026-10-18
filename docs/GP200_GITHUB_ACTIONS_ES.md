# Compilar Tone3000 + GP200 en tu fork — revisión 0.2

## Archivos y versión de partida

Este añadido se ha preparado sobre el commit oficial:
`ab5e5ae4388753184687d67816815aa481f2cea9`.

Copia el contenido de `PARA_COPIAR_EN_EL_FORK/` a la raíz del fork, conservando los directorios y sustituyendo los archivos correspondientes. Deben quedar `plugin/`, `test/`, `docs/` y `.github/` en la raíz, sin una carpeta intermedia. El añadido incluye TODA la integración GP200; no necesitas aplicar antes la revisión 0.1.

Si el fork parte de otro commit, revisa los cambios de upstream antes de sustituir archivos. Se incluye `GP200_0.2.patch` como alternativa: `git apply GP200_0.2.patch` sobre el commit indicado. No combines la copia y la aplicación del parche.

## Catálogo Tone3000

Para iniciar sesión y navegar por el catálogo desde una compilación propia, configura tu publishable key de Tone3000:

1. GitHub → tu fork → **Settings → Secrets and variables → Actions**.
2. Crea un **Repository secret** llamado `T3K_PUBLISHABLE_KEY` con tu clave pública `t3k_pub_…`. También se admite una Repository variable con ese nombre; el secret tiene prioridad.
3. No necesitas copiar ni publicar claves de la aplicación oficial.

Si no configuras la clave, el workflow lo indica y permite compilar. La carga de NAM locales sigue disponible, pero el acceso autenticado al catálogo necesita esa configuración. Configurar la clave después de compilar exige repetir el build porque CMake la incorpora a la aplicación.

## Ejecutar Actions

1. Haz commit de todos los cambios en el fork, incluida `.github/workflows/build-gp200-windows.yml`.
2. Abre **Actions**. Si GitHub muestra que los workflows están deshabilitados en el fork, habilítalos.
3. Selecciona **Build Tone3000 GP200 - Windows → Run workflow** en la rama que contiene la integración.
4. Deja activadas las opciones **Also build VST3** y **Build and run Tone3000 UI self-tests**, o desactívalas para compilar solamente el Standalone y las pruebas de protocolo.
5. Descarga el artefacto **TONE3000-GP200-Windows-x64-EXPERIMENTAL** al terminar.

El workflow también se inicia con cambios relevantes en las ramas `main`, `master` y `gp200`. En ese caso compila Standalone/VST3 y ejecuta las pruebas UI. La ejecución manual está definida para la rama predeterminada del repositorio: si el workflow aún no aparece, asegúrate de que el archivo está en esa rama.

## Qué entrega

- `TONE3000-GP200.exe`: aplicación independiente Windows x64.
- `TONE3000.vst3/`: bundle VST3 completo si se seleccionó esa opción. Copia el directorio entero al directorio VST3 del host.
- SHA256 de los archivos, commit de construcción, instrucciones y avisos de licencia.

No incluye firmware, instalador, firma digital ni publicación de una release. Las dependencias JUCE/NAM se obtienen mediante el mecanismo del proyecto y los submódulos fijados en el fork. Este workflow no descarga el SDK ASIO opcional; conserva los controladores disponibles sin ese SDK.

## Pruebas y errores

Antes de compilar la aplicación, ejecuta las pruebas GP200 en Release con los assert habilitados. Comprueban montaje de cinco/siete fragmentos, rechazo de mensajes incoherentes, lector NAM desde memoria/archivo, destinos y ACK NH04. Las pruebas del códec usan tipos JUCE modelados. La aplicación y el testbed se compilan con el JUCE real del proyecto.

Cada ejecución conserva un segundo artefacto **TONE3000-GP200-build-logs**, incluso si falla. Incluye logs de configuración/compilación disponibles y resultados de pruebas. Si hay un error, conserva ese artefacto para identificar el fallo exacto.

## Uso GP200

Carga un NAM en Tone3000 → pulsa GP200 → elige modelo cargado → puertos MIDI → AMP destino → nombre → Upload NAM HOT. Solo acepta los modelos compatibles con nuestro motor A2 Lite 48 kHz. Se envían pesos y nombre, no la cadena/EQ/IR de Tone3000.

La GP200 debe tener instalado firmware NH04 compatible. Selecciona otro AMP en la pedalera antes de cargar y no cambies preset/AMP durante la operación. Si Windows no permite abrir MIDI, cierra otros editores y deshabilita la entrada MIDI de esa GP200 en los ajustes generales de Tone3000 antes de intentarlo desde la pestaña.

## Validación de esta entrega

Revisión local, pruebas de protocolo/códec y chequeos de sintaxis con cabeceras reales JUCE 9.0.3. No se ha ejecutado este workflow en tu cuenta de GitHub ni probado el conjunto físicamente en una GP200. El resultado continúa siendo experimental.

### Si CMake indica que falta NeuralAmpModelerCore

La revisión 0.3 añade una etapa que actualiza explícitamente los submódulos recursivos y comprueba que existen sus fuentes. Si el fork no materializa el gitlink del NAM Core, descarga el commit `1f42f88535884450104b8711d7595019afa0495b` fijado en la base TONE3000 y sus dependencias anidadas. Ese es el submódulo requerido por el error `add_subdirectory ... NeuralAmpModelerCore ... not an existing directory`.
