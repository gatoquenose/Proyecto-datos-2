# Prueba manual con mocks (F6-5)

Preparación: `VITE_USE_MOCKS=true` en `frontend/.env.local`, luego `npm.cmd --prefix frontend run dev`.
Usuario: `ana.tec`. Contraseña: `CorrectHorseBatteryStaple!`

## Sesión
- [ ] Sin sesión, la primera pantalla es login o registro.
- [ ] Login con credenciales incorrectas: el error sale en el formulario, no el aviso global.
- [ ] Registro con username o correo repetido: error en el campo correspondiente.
- [ ] Login correcto: lleva a /videos.
- [ ] Recargar con sesión válida: se mantiene la sesión.
- [ ] Cerrar sesión voluntariamente: no aparece "Tu sesión expiró."

## Lista de videos
- [ ] Ordenar por título, autor y fecha, en asc y desc.
- [ ] Buscar con q: el valor se conserva en la URL y en el campo.
- [ ] "Cargar más" no duplica videos.
- [ ] /videos/mine muestra solo videos propios.
- [ ] Las tarjetas muestran miniatura, título, autor, fecha, duración y, en los propios, visibilidad y estado.

## Detalle y reproducción
- [ ] Reproducir un video propio y uno ajeno.
- [ ] Calidad Auto y calidad manual.
- [ ] Video en processing: muestra el estado y se actualiza solo.

## Subida
- [ ] Subida completa hasta que el video quede listo.
- [ ] Cancelar una subida.
- [ ] Archivo mayor a la cuota restante: se bloquea.
- [ ] Recargar a mitad de subida y reanudar con el mismo archivo.
- [ ] Fallo de completeUpload (400): reintentar o cancelar sin volver a enviar fragmentos.
- [ ] Estado failed: PENDIENTE confirmar qué variable de `.env.example` lo provoca (`VITE_MOCK_UPLOAD_FAILURE`).

## Perfil y edición
- [ ] /profile muestra la cuota solo como lectura.
- [ ] Editar el correo.
- [ ] Eliminar cuenta: el botón se habilita solo con el username exacto; Escape cierra y limpia.
- [ ] Editar metadata y cambiar visibilidad de un video propio.
- [ ] Eliminar un video propio con confirmación.