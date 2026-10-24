# SIN – Sistema de Nombrado Integral: cómo correrlo

Sistema de nombrado distribuido que da un nombre uniforme (`sin://<nodo>/<tipo>/<ruta>`) a archivos, procesos, servicios, memoria y otros recursos, y lo traduce a la dirección nativa de cada uno.

## Requisitos

- Windows con **cmd**
- **Python 3.8 o superior** (solo usa la librería estándar, no hay nada que instalar)

Comprobar la versión:

```bat
python --version
```

## Estructura de carpetas

```
ejercicio5TP4\
  demo.py
  sin\
    __init__.py
    nombre.py
    adaptadores.py
    nodo.py
    cliente.py
```

**Todos los comandos se ejecutan desde la carpeta `ejercicio5TP4`** (la que contiene la carpeta `sin\`).

## Formato de los nombres

```
sin://<nodo>/<tipo>/<ruta>

sin://nodo1/archivo/informe       -> nodo explícito
sin:///servicio/inventory         -> nodo vacío: el SIN lo busca donde esté
```

Tipos disponibles: `archivo`, `proceso`, `servicio`, `memoria`, `recurso`.

---

## Opción A: demo automática (un solo proceso)

Levanta dos nodos, crea recursos reales y muestra todas las operaciones.

```bat
python demo.py
```

> Usa los puertos 9001, 9002 y 8000. No la ejecutes mientras tengas nodos de la Opción B abiertos.

---

## Opción B: nodos reales en terminales separadas

### 1. Levantar los nodos

**Terminal 1**

```bat
python -m sin.nodo --nombre nodo1 --puerto 9001 --peer nodo2=127.0.0.1:9002
```

**Terminal 2**

```bat
python -m sin.nodo --nombre nodo2 --puerto 9002 --peer nodo1=127.0.0.1:9001
```

Cada nodo muestra `Nodo ... escuchando en ...`. Se dejan abiertos. Para apagarlos: `Ctrl+C`.

### 2. Usar el cliente (Terminal 3)

Formato general:

```bat
python -m sin.cliente --nodo HOST:PUERTO [--sin-cache] OPERACION argumentos
```

| Operación | Argumentos | Qué hace |
|---|---|---|
| `registrar` | `nombre clave=valor ...` | Registra un recurso en el nodo consultado |
| `resolver` | `nombre` | Devuelve la dirección nativa del recurso |
| `baja` | `nombre` | Elimina un recurso o alias |
| `alias` | `nombre destino` | Crea un nombre alternativo |
| `listar` | `[tipo]` | Lista lo registrado en ese nodo |

> `--sin-cache` va **antes** de la operación, junto a `--nodo`. Fuerza la consulta al nodo dueño ignorando el caché.

---

## Pruebas por tipo de recurso

### Servicio

Levantar algo escuchando en el puerto 8000 (en otra terminal):

```bat
python -m http.server 8000
```

Registrar en el nodo2 y resolver desde el nodo1 sin indicar el nodo:

```bat
python -m sin.cliente --nodo 127.0.0.1:9002 registrar sin:///servicio/inventory host=127.0.0.1 puerto=8000
python -m sin.cliente --nodo 127.0.0.1:9001 resolver sin:///servicio/inventory
```

Resultado esperado: `"nodo": "nodo2"` y `"activo": true`. Si se corta el servidor con `Ctrl+C` y se resuelve de nuevo (con `--sin-cache`), pasa a `"activo": false`.

### Archivo

```bat
python -m sin.cliente --nodo 127.0.0.1:9001 registrar sin:///archivo/informe path="C:\ruta\a\un\archivo.txt"
python -m sin.cliente --nodo 127.0.0.1:9002 resolver sin:///archivo/informe
```

Devuelve la ruta absoluta, si existe y su tamaño.

### Proceso

Abrir el Bloc de notas y buscar su PID:

```bat
tasklist | findstr notepad
```

Registrar con ese PID (reemplazar `1234`) y resolver desde el otro nodo:

```bat
python -m sin.cliente --nodo 127.0.0.1:9002 registrar sin:///proceso/bloc pid=1234
python -m sin.cliente --nodo 127.0.0.1:9001 resolver sin:///proceso/bloc
```

Devuelve `"activo": true`. Al cerrar el Bloc de notas y resolver con `--sin-cache`, pasa a `false`.

### Memoria

```bat
python -m sin.cliente --nodo 127.0.0.1:9001 registrar sin:///memoria/buffer tamano=64
python -m sin.cliente --nodo 127.0.0.1:9002 resolver sin:///memoria/buffer
```

Devuelve el nombre del segmento de memoria compartida. Solo es accesible desde la máquina del nodo dueño.

---

## Alias

```bat
python -m sin.cliente --nodo 127.0.0.1:9001 alias sin:///servicio/web sin://nodo2/servicio/inventory
python -m sin.cliente --nodo 127.0.0.1:9001 resolver sin:///servicio/web
```

Se resuelve como si se hubiera pedido el destino. Para cambiar un alias: `baja` y luego `alias` de nuevo.

## Migración (transparencia de ubicación)

Mover el servicio del nodo2 al nodo1 manteniendo el mismo nombre:

```bat
python -m sin.cliente --nodo 127.0.0.1:9002 baja sin:///servicio/inventory
python -m sin.cliente --nodo 127.0.0.1:9001 registrar sin:///servicio/inventory host=127.0.0.1 puerto=8000
python -m sin.cliente --nodo 127.0.0.1:9002 resolver sin:///servicio/inventory
```

El nodo2 ya no lo tiene, pero lo encuentra en el nodo1 porque el nombre no incluye el nodo.

## Caché

Las resoluciones **remotas** se cachean 30 segundos (TTL). Durante ese tiempo la respuesta puede estar desactualizada y aparece `"cache": true`.

```bat
:: puede responder desde el caché
python -m sin.cliente --nodo 127.0.0.1:9001 resolver sin:///servicio/web

:: fuerza la consulta fresca al nodo dueño
python -m sin.cliente --nodo 127.0.0.1:9001 --sin-cache resolver sin:///servicio/web
```

---

## Cómo leer la respuesta

| Campo | Significado |
|---|---|
| `nombre` | Nombre real del recurso (si se pidió un alias, el destino) |
| `nodo` / `ubicacion_nodo` | Nodo dueño del recurso y su dirección |
| `tipo` | Tipo de recurso (define el adaptador usado) |
| `direccion` | Dirección nativa: `host`/`puerto` (servicio), `path` (archivo), `pid` (proceso), `segmento` (memoria) |
| `ttl` | Segundos que se puede cachear la respuesta |
| `cache` | Solo aparece si la respuesta vino del caché |

## Problemas frecuentes

| Mensaje | Causa y solución |
|---|---|
| `No module named 'sin'` | No estás en la carpeta correcta. Hacer `cd` a `ejercicio5TP4` |
| `WinError 10061` / conexión rechazada | El nodo al que apuntás no está corriendo. Revisar `--nodo` y la terminal del nodo |
| `WinError 10048` / dirección en uso | Ese puerto ya está ocupado (otro nodo o la demo abierta) |
| `Nodo desconocido: nodoX` | Falta el parámetro `--peer` al levantar el nodo |
| `Ya registrado: ...` | Ese nombre ya existe. Hacer `baja` primero |
| `No existe: ...` | El recurso no está registrado, o un alias apunta a algo dado de baja |
| `Faltan datos: ...` | Falta una clave en `registrar` (`path`, `pid`, `host`/`puerto`) |
| `Solo se puede registrar en el nodo local` | El nombre indica otro nodo. Registrar en el nodo correcto o usar `sin:///...` |

## Notas

- Cada recurso se registra en el nodo que se consulta, y ese nodo es su dueño.
- Al cerrar un nodo con `Ctrl+C` se liberan sus segmentos de memoria.
- Para agregar un tipo de recurso nuevo, crear un adaptador en `sin\adaptadores.py` y sumarlo en `crear_adaptadores`.