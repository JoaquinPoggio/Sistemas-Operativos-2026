# Sincronización de relojes en C++ (Cristian y Berkeley)

Práctico de Sistemas Distribuidos, ejercicio B: programas en C++ para sincronizar computadoras. Se implementan dos algoritmos clásicos de sincronización de relojes físicos:

- **Algoritmo de Cristian:** un cliente se sincroniza con un servidor de hora.
- **Algoritmo de Berkeley:** un maestro (coordinador) promedia los relojes de varias computadoras y le indica a cada una cuánto debe ajustarse.

La comunicación usa sockets TCP (Winsock) y los mensajes son líneas de texto.

## Reloj simulado

Para no modificar la hora real de Windows (requiere permisos de administrador), cada programa usa un reloj propio:

```
hora = hora del sistema + desfasaje (offsetMs)
```

Con un parámetro de línea de comandos se le da un desfasaje inicial para simular un reloj adelantado (+) o atrasado (−). Sincronizar significa corregir ese desfasaje.

## Algoritmo de Cristian

1. El cliente envía `HORA?` al servidor y mide el tiempo de ida y vuelta (RTT).
2. El servidor responde con su hora `Ts`.
3. El cliente estima la hora correcta como `Ts + RTT/2` y corrige su reloj.
4. La consulta se repite 5 veces y se usa la de **menor RTT**, que es la más precisa.

La precisión máxima de la sincronización es de ± RTT/2.

Puerto del servidor: **6000**.

## Algoritmo de Berkeley

1. **Consulta:** el maestro le pide la hora a cada esclavo y calcula la diferencia de cada reloj respecto del suyo, compensando con RTT/2.
2. **Promedio:** promedia las diferencias, contando el reloj del maestro como 0. Los relojes a más de 30 segundos del maestro quedan fuera del promedio, pero igual reciben el ajuste.
3. **Ajuste:** envía a cada esclavo cuánto debe corregir su reloj (`promedio − diferencia`), no la hora absoluta, y se ajusta a sí mismo. Al final todos quedan con la misma hora.

Cada esclavo elige su puerto al iniciarse.

## Requisitos

- Windows
- Compilador g++ (MinGW-w64). Instalación con winget:

```
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
```

## Compilación

Desde la carpeta del proyecto, en una terminal cmd (**Terminal 1**):

```
g++ cristian.cpp -o cristian.exe -lws2_32 -static
g++ berkeley.cpp -o berkeley.exe -lws2_32 -static
```

## Uso

```
cristian.exe servidor [desfasaje_ms]
cristian.exe cliente <ip_servidor> [desfasaje_ms]

berkeley.exe esclavo <puerto> [desfasaje_ms]
berkeley.exe maestro <desfasaje_ms> <ip:puerto> [<ip:puerto> ...]
```

## Ejecución en una sola PC

### Cristian

**Terminal 1** (abre una segunda ventana con el servidor, con un reloj 10 s adelantado):

```
start cmd /k cristian.exe servidor 10000
```

**Terminal 1** (el cliente, con un reloj 5 s atrasado):

```
cristian.exe cliente 127.0.0.1 -5000
```

El cliente debe mostrar una corrección de unos +15000 ms y quedar con la misma hora que el servidor. Ejemplo de salida:

```
=== Cliente (servidor de hora: 127.0.0.1:6000) ===
Hora local ANTES de sincronizar:   21:44:57.682

Consulta 1: Ts=21:45:12.683  RTT=0.442 ms  correccion=+15000 ms
Consulta 2: Ts=21:45:12.884  RTT=0.419 ms  correccion=+15000 ms
...
Se usa la consulta de menor RTT (0.404 ms): correccion de +14999 ms
Precision maxima de la sincronizacion: +/- 0.202 ms (RTT/2)
Hora local DESPUES de sincronizar: 21:45:13.687
```

### Berkeley

**Terminal 1** (abre una ventana por cada esclavo, con relojes desfasados +5 s, −3 s y +2 s):

```
start cmd /k berkeley.exe esclavo 7001 5000
start cmd /k berkeley.exe esclavo 7002 -3000
start cmd /k berkeley.exe esclavo 7003 2000
```

**Terminal 1** (el maestro, con su reloj en 0):

```
berkeley.exe maestro 0 127.0.0.1:7001 127.0.0.1:7002 127.0.0.1:7003
```

Ejemplo de salida del maestro:

```
=== Maestro ===
Hora del maestro ANTES: 21:44:51.998

--- Fase 1: consulta de horas ---
127.0.0.1:7001 -> hora 21:44:56.998  RTT=0.359 ms  diferencia=+4999 ms
127.0.0.1:7002 -> hora 21:44:48.999  RTT=0.351 ms  diferencia=-3000 ms
127.0.0.1:7003 -> hora 21:44:53.999  RTT=0.298 ms  diferencia=+1999 ms

--- Fase 2: promedio de diferencias (4 relojes, incluido el maestro) = +999 ms ---

--- Fase 3: ajustes ---
127.0.0.1:7001 -> ajuste enviado: -4000 ms (confirmado)
127.0.0.1:7002 -> ajuste enviado: +3999 ms (confirmado)
127.0.0.1:7003 -> ajuste enviado: -1000 ms (confirmado)
Maestro -> ajuste propio: +999 ms

Hora del maestro DESPUES: 21:44:53.000
```

Cálculo esperado: (5000 − 3000 + 2000 + 0) / 4 = +1000 ms. Las 4 máquinas quedan con la hora del maestro más 1 segundo (±1 ms por redondeo).

Los esclavos quedan esperando una nueva consulta, así que se puede volver a correr el maestro y las diferencias deben dar casi 0. Para cerrarlos, Ctrl+C en cada ventana.

## Ejecución en varias computadoras

- **Cristian:** en una PC se ejecuta `cristian.exe servidor` y en las otras `cristian.exe cliente <ip_del_servidor>`.
- **Berkeley:** en cada PC esclava se ejecuta `berkeley.exe esclavo 7000`, y en la PC maestra `berkeley.exe maestro 0 <ip2>:7000 <ip3>:7000 <ip4>:7000`.
- Hay que permitir en el firewall de Windows el puerto TCP 6000 (Cristian) o el elegido para los esclavos (Berkeley).
- Si no se indica desfasaje, los programas miden y corrigen las diferencias reales entre los relojes de las PC.

## Estructura del repositorio

```
comun.h         reloj simulado y funciones de sockets compartidas
cristian.cpp    algoritmo de Cristian (servidor y cliente)
berkeley.cpp    algoritmo de Berkeley (maestro y esclavo)
README.md       este archivo
.gitignore      excluye los ejecutables
```