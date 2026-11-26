# Prueba Manual Completa con Mocks

## 1. Sesión

- [ ] Iniciar la aplicación con `npm.cmd --prefix frontend run dev`
- [ ] Verificar que `VITE_USE_MOCKS=true` esté configurado en el archivo `.env`
- [ ] Acceder al login y credenciales de prueba:
  - Usuario: `ana.tec` o `CorrectHorseBatteryStaple!`
  - Email: `ana@example.test`
- [ ] Iniciar sesión y confirmar que el JWT se almacena en `sessionStorage`
- [ ] Confirmar que la interfaz muestra el modo de desarrollo con mocks activos
- [ ] Abrir la consola del navegador y verificar que no hay errores de MSW

## 2. Lista de Videos

- [ ] Verificar que la lista de videos se carga con los 30 videos mock precargados
- [ ] Confirmar que los videos tienen distintos autores, fechas, visibilidades y estados
- [ ] Probar el campo `q` (búsqueda) en la URL y verificar filtrado
- [ ] Probar `sortBy` y `order` en la URL
- [ ] Confirmar paginación por cursor opaca (no depende del backend)
- [ ] Verificar que el usuario `ana.tec` ve solo sus propios videos cuando `owner=me`

## 3. Detalle y Reproducción

- [ ] Hacer clic en un video y verificar que abre la VideoDetailPage
- [ ] Confirmar que la miniatura PNG 1x1 se muestra correctamente
- [ ] Verificar que el título, ownerUsername y fecha (es-CR) se muestran
- [ ] Confirmar duración en formato mm:ss
- [ ] En videos propios: verificar la visualización de Público/Privado y estado
- [ ] Probar reproducción con Shaka Player y cómodo JWT en manifiesto/segmentos
- [ ] Verificar que el cargador de Shaka se infla por defecto import()

## 4. Subida

- [ ] Iniciar una subida de archivo nuevo
- [ ] Verificar que el asistente de dos pasos muestra etiquetas como chips
- [ ] Confirmar progreso accesible durante la subida
- [ ] Probar el sondeo al completar (`beforeunload` solo durante subida)
- [ ] Verificar estados de subida/procesamiento
- [ ] Probar cancelación con `cancelUpload` (conserva reserva)
- [ ] Probar `completeUpload` con checksum inválido (deja subida en `uploading`, permite reintentar)
- [ ] Verificar que `VITE_MAX_UPLOAD_BYTES` limita el tamaño máximo
- [ ] Confirmar que los fragmentos se envían con `uploadChunk`
- [ ] Probar reanudación de subida con `sessionStorage` (con `{ uploadId, videoId, fileName, sizeBytes, contentType, chunkSize }`)
- [ ] Verificar que tras recargar, el usuario vuelve a elegir el mismo archivo y se releen los bytes enviados localmente

## 5. Perfil

- [ ] Acceder a la página de perfil (`/profile`)
- [ ] Verificar la cuota de solo lectura (usada, total, restante)
- [ ] Confirmar que solo se puede editar el email (PATCH solo envía email)
- [ ] Probar eliminación de cuenta con ConfirmDialog:
  - El botón se habilita solo si el texto coincide EXACTAMENTE con el username
  - Sin trim ni mayúsculas
  - Escape cierra y limpia el texto
  - Tras eliminar con éxito, se limpia la sesión y se va a /login
- [ ] Verificar que el diálogo no promete nada sobre los videos del usuario

## 6. Edición

- [ ] Abrir la edición de un video
- [ ] Modificar metadatos (título, descripción, visibilidad)
- [ ] Confirmar que los cambios se guardan correctamente
- [ ] Verificar la actualización de visibilidad (público/privado)
- [ ] Probar la restauración de sesión tras edición

## 7. Estado Failed

- [ ] Verificar que un video en estado `failed` muestra el mensaje: "El procesamiento del video falló."
- [ ] Confirmar que el sondeo GET /videos/{id}/processing se detiene ante `failed`
- [ ] Verificar que al llegar a `ready` el video se invalida y se actualiza la lista
- [ ] Probar que las acciones del dueño (Editar, cambiar visibilidad, Eliminar) funcionan correctamente
- [ ] Confirmar que no hay subtítulos falsos en v1 (eslint-disable-next-line jsx-a11y/media-has-caption con justificación)