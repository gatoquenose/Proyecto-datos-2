# Árbol B en disco (users.idx)

Borrador de la Fase B. Se completa a medida que se agregan borrado, WAL y recuperación.

## Para qué sirve
`users.dat` guarda los usuarios en orden de llegada. Buscar uno obligaría a leer todo el archivo.
`users.idx` es un índice: dado un username o un email, indica en qué posición (offset) de `users.dat`
está el registro, leyendo pocas páginas en vez de todo el archivo.

## Decisiones
| Decisión | Elección | Motivo |
|---|---|---|
| Tamaño de página | 4096 bytes | Tamaño típico de bloque de disco; una lectura = un nodo |
| Orden | Derivado del tamaño de página (`BTreeLayout.hpp`) | Lo pide el SPEC; no hay números mágicos |
| Claves | Texto de 1 a 127 bytes (ranura fija de 128 bytes) | Ranuras fijas simplifican el formato y permiten calcular el orden |
| Valores | `uint64` = offset del registro en `users.dat` | |
| Algoritmo de inserción | Divide los nodos llenos al bajar (CLRS) | Nunca hay que volver hacia arriba, ideal para disco |
| Enteros en disco | Little-endian explícito (`ByteCodec.hpp`) | El archivo es igual en cualquier máquina |
| Acceso a páginas | Interfaz `IPageStore` (memoria o archivo) | Pruebas rápidas y cumplimiento de inyección de dependencias |

## Formato de página de nodo (4096 bytes)
| Zona | Tamaño | Contenido |
|---|---|---|
| Cabecera | 8 bytes | hoja (1), reservado (1), cantidad de claves (2), reservado (4: aquí irá el CRC32C) |
| Claves | 29 x 128 bytes | 1 byte de largo + texto de la clave |
| Valores | 29 x 8 bytes | offset en `users.dat` |
| Hijos | 30 x 4 bytes | identificadores de página (solo nodos internos) |

Máximo 29 claves por nodo (2t-1 con t = 15). Todo nodo salvo la raíz tiene al menos 14 claves.

## Página 0 (metadatos)
Firma `TFXIDX01` (8 bytes), versión del formato (4), tamaño de página (4), página de la raíz (4).
La raíz cambia de página cuando el árbol crece, por eso se guarda aquí.

## Estado actual
- Hecho: insertar, buscar, recorrer en orden, persistencia en archivo, validación de formato.
- Pendiente: borrado lógico, claves de username y email, `users.dat`, bloques y espacio libre,
  concurrencia (`shared_mutex` + bloqueo de archivo), WAL, CRC32C y reconstrucción del índice.
