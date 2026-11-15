# Almacenamiento de usuarios: users.dat y árbol B (users.idx)

Borrador de la Fase B. Se completa a medida que se agregan WAL y recuperación.

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
| Cabecera | 8 bytes | hoja (1), reservado (1), cantidad de claves (2), reservado (4: CRC32C) |
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
| CRC32C (reservado) | 4 |
| Relleno | resto |

## Operaciones de FileUserRepository
- `add`: comprueba id, username y email libres; agrega el registro al final e inserta las 3 claves.
- `update`: escribe el registro en su mismo offset. Rechaza cambios de `username` y `quotaBytes`.
  Si cambia el email, mueve la clave `e:` en el índice.
- `remove`: borrado lógico de las 3 claves y marca el registro como eliminado.
- `findById`, `findByUsername`, `findByEmail`.

## Pendiente
Recorrido por prefijo (para `GET /users?username=`), bloques y lista de espacio libre, concurrencia
(`shared_mutex` + bloqueo de archivo), WAL, CRC32C y reconstrucción del índice (`rebuild-index`).
