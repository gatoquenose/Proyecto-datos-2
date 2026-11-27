# Almacenamiento de usuarios: users.dat y árbol B (users.idx)

Fase B. Incluye detección de corrupción (CRC32C), reconstrucción del índice y registro de intención (WAL) para que las escrituras sean atómicas.

## Para qué sirve
`users.dat` guarda los usuarios, un registro tras otro. Buscar uno obligaría a leer todo el archivo.
`users.idx` es un índice: dado un username, un email o un id, indica en qué posición (offset) de
`users.dat` está el registro, leyendo pocas páginas en vez de todo el archivo.

## Decisiones
| Decisión | Elección | Motivo |
|---|---|---|
| Tamaño de página | 4096 bytes | Tamaño típico de bloque de disco; una lectura = un nodo |
| Orden | Derivado del tamaño de página (`BTreeLayout.hpp`) | Lo pide el SPEC; no hay números mágicos |
| Claves | Texto de 1 a 127 bytes (ranura fija de 128 bytes) | Ranuras fijas simplifican el formato |
| Valores | `uint64` = offset del registro en `users.dat` | |
| Un solo árbol, claves con prefijo | `u:<username>`, `e:<email>`, `i:<id>` | El SPEC pide un único `users.idx` |
| Clave `i:<id>` (extra) | Búsqueda por id | `GET /users/{id}`, el JWT y las reservas de cuota identifican por id |
| Mayúsculas | Username y email se comparan en minúsculas | Evita dos cuentas "Alice" y "alice" |
| Inserción | Divide los nodos llenos al bajar (CLRS) | Nunca hay que volver hacia arriba |
| Borrado | Lógico: el valor pasa a `kTombstone` (`UINT64_MAX`) | Lo pide el SPEC; la reconstrucción del índice las elimina |
| Registro | Tamaño fijo de 384 bytes | El offset del registro N es `64 + N * 384` |
| Enteros en disco | Little-endian explícito (`ByteCodec.hpp`) | El archivo es igual en cualquier máquina |
| Email | Máximo 120 caracteres | La clave `e:<email>` debe caber en 127 bytes |

## Formato de página de nodo (4096 bytes)
| Zona | Tamaño | Contenido |
|---|---|---|
| Cabecera | 8 bytes | hoja (1), reservado (1), cantidad de claves (2), CRC32C de la página (4) |
| Claves | 29 x 128 bytes | 1 byte de largo + texto |
| Valores | 29 x 8 bytes | offset en `users.dat` |
| Hijos | 30 x 4 bytes | identificadores de página (solo nodos internos) |

Máximo 29 claves por nodo (2t-1 con t = 15).

## Página 0 del índice
Firma `TFXIDX01` (8 bytes), versión (4), tamaño de página (4), página de la raíz (4).

## Formato de users.dat
Cabecera de 64 bytes: firma `TFXUSR01` (8), versión (4), tamaño de registro (4); el resto reservado.

Registro de 384 bytes:
| Campo | Tamaño |
|---|---|
| Estado (1 = activo, 2 = eliminado) | 1 |
| id | 1 + 36 |
| username | 1 + 64 |
| email | 1 + 120 |
| hash de contraseña (Argon2id) | 1 + 128 |
| quotaBytes | 8 |
| usedBytes | 8 |
| CRC32C del registro | 4 |
| Relleno | resto |

## Operaciones de FileUserRepository
- `add`: comprueba id, username y email libres; agrega el registro al final e inserta las 3 claves.
- `update`: escribe el registro en su mismo offset. Rechaza cambios de `username` y `quotaBytes`.
  Si cambia el email, mueve la clave `e:` en el índice.
- `remove`: borrado lógico de las 3 claves y marca el registro como eliminado.
- `findById`, `findByUsername`, `findByEmail`.

## Integridad: CRC32C
Cada página del índice, cada registro de `users.dat` y cada registro de `reservations.dat` guarda un
CRC32C (polinomio de Castagnoli) calculado sobre todos sus bytes **menos** los 4 del propio campo.
- Al escribir (`encode`) se calcula y se guarda; al leer (`decode`) se recalcula y se compara.
- Si no coincide se lanza `CorruptionError`. Una página de ceros (nunca escrita) tampoco es válida.
- `UserRecordCodec::setStatus` (marcar eliminado) verifica el checksum antes de volver a sellarlo:
  si no, un registro dañado quedaría "legalizado" con un CRC nuevo.
- Se eligió CRC32C porque detecta bien los errores de bits y ráfagas típicos de disco, es barato de
  calcular y es el estándar en sistemas de almacenamiento. No protege contra manipulación
  intencional (para eso haría falta un HMAC); aquí el objetivo es detectar daño accidental.
- **Cambio de formato:** la cabecera de `users.dat` y `reservations.dat` pasó a versión 2. Los archivos
  anteriores (sin checksum) se rechazan al abrir en vez de leerse mal; en desarrollo se borra la carpeta de datos.

## Recuperación: reconstrucción del índice
`users.dat` es la fuente de verdad; `users.idx` siempre se puede volver a armar desde él.
- `IndexRebuilder` lee los registros en orden. Por cada usuario activo inserta sus tres claves
  (`i:`, `u:`, `e:`) apuntando a su offset. Los eliminados no se indexan (así desaparecen las claves
  con tombstone), los dañados se cuentan y omiten, y si dos registros chocan en id, username o email
  gana el primero.
- Se escribe en `users.idx.rebuild` y recién al terminar se renombra sobre `users.idx`: una caída a mitad
  de la reconstrucción no deja un índice a medias.
- **Automática al arrancar:** `ensureUserIndex` abre el índice, recorre todas sus páginas (verifica los
  checksums) y comprueba que tenga exactamente tres claves por usuario activo. Si algo falla (archivo
  borrado, página dañada, caída entre escribir el registro y el índice) lo reconstruye y deja un log `warn`.
- **Manual:** `users-api --rebuild-index` (con las mismas variables de entorno del servicio) reconstruye y termina.
  Sale con código 1 si encontró registros dañados. Con el servicio detenido. En Docker:
  `docker compose run --rm users-api --rebuild-index`.
- Los registros dañados en `users.dat` no se pueden reparar: se omiten y se reportan.

## Atomicidad: WAL (`users.wal`)
Agregar, actualizar o eliminar un usuario toca dos archivos (`users.dat` y `users.idx`) y varias claves del
árbol. Si el proceso cae en medio, quedarían a medias. El WAL lo evita:
1. **Anotar:** antes de tocar nada, se escribe en `users.wal` la operación completa: tipo, offset y las
   imágenes del registro *antes* y *después* (con su CRC32C).
2. **Aplicar:** se escribe el registro en `users.dat` y se ajustan las claves del índice.
3. **Borrar la anotación:** la operación quedó completa.
4. **Al arrancar** (`recoverPending`): si hay una anotación válida sin borrar, se **rehace**.

Rehacer es seguro porque aplicar es **idempotente**: el registro se escribe por su offset (o se agrega solo
si aún no está), y cada clave se "asegura" (se crea o se corrige) o se "suelta" solo si todavía apunta a este
registro. Repetir la operación, o haber hecho la mitad, da el mismo resultado final.

| Cae después de… | Estado en disco | Al arrancar |
|---|---|---|
| anotar | nada aplicado | se aplica completa |
| escribir el registro | registro sin índice (o índice parcial) | se completan las claves, sin duplicar el registro |
| mover/borrar claves | claves listas, registro viejo | se escribe el registro final |
| aplicar todo, antes de borrar la anotación | todo aplicado | se repite sin efecto y se borra |
| anotar a medias (checksum inválido) | nada aplicado | la anotación se descarta: la operación no empezó |

Decisiones:
- **Una sola ranura**, no un log que crece. Las escrituras ya están serializadas (`LockedUserRepository`),
  así que hay a lo sumo una operación en curso. Es más simple y no necesita compactarse.
- **Imágenes completas** del registro, no "instrucciones": rehacer deja bytes idénticos y no depende del
  estado en que quedaron los archivos.
- Si aplicar falla (disco lleno, error de E/S), la anotación sigue pendiente y el repositorio **se niega a
  aceptar nuevas escrituras** hasta reiniciar y recuperar: anotar otra sobrescribiría la pendiente y se perdería.
- Orden de arranque: primero `ensureUserIndex` (reconstruye si el índice no coincide con `users.dat`),
  después `recoverPending`. Así también se cierra el caso "el borrado a medias resucita al usuario": la
  reconstrucción lo vuelve a indexar y el WAL lo elimina.
- **Límite:** el WAL se vacía al sistema operativo (`flush`) pero no se fuerza a disco (`fsync`), porque
  `std::fstream` no lo ofrece. Protege ante la caída del proceso (kill, excepción, reinicio del contenedor);
  no ante un corte de energía justo en ese instante. Cerrarlo exigiría llamadas propias de cada sistema operativo.
- No cubre `reservations.dat`: sus operaciones son idempotentes por `reservationId` y el barrido las corrige.

## Pendiente
Recorrido por prefijo (para `GET /users?username=`), bloques y lista de espacio libre, concurrencia con
bloqueo de archivo del sistema operativo (hoy: un mutex dentro del proceso).
