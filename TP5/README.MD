# Sincronización con relojes lógicos de Lamport (C++)

Práctico de Sistemas Distribuidos. Cuatro computadores (1, 2, 3 y 4) ejecutan cada uno un proceso `P`. La aplicación debe ejecutarse en el orden **P1 → P2 → P3 → P4**, y las máquinas se sincronizan con el **algoritmo de relojes lógicos de Lamport**.

## Algoritmo

Cada proceso mantiene un reloj lógico `L`, que arranca en 0:

1. **Evento local o envío:** `L = L + 1`. Al enviar, el mensaje lleva `L` como timestamp.
2. **Recepción:** `L = max(L_local, timestamp_recibido) + 1`.

## Funcionamiento

1. P1 ejecuta su tarea y envía el mensaje `TURNO` a P2.
2. P2 espera el mensaje, actualiza su reloj, ejecuta su tarea y envía `TURNO` a P3.
3. P3 hace lo mismo y avisa a P4.
4. P4 recibe el turno, ejecuta su tarea y finaliza la aplicación.

El orden queda garantizado por la relación "ocurre antes" (happened-before): cada proceso solo ejecuta después de recibir el mensaje del anterior. La comunicación usa sockets TCP (Winsock). Los procesos pueden iniciarse en cualquier orden, porque el emisor reintenta la conexión cada 1 segundo hasta que el destino esté escuchando.

Puertos: P1 → 5001, P2 → 5002, P3 → 5003, P4 → 5004.

## Requisitos

- Windows
- Compilador g++ (MinGW-w64). Instalación con winget:

```
winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
```

## Compilación

```
g++ lamport.cpp -o lamport.exe -lws2_32 -static
```

## Ejecución

### En una sola PC

Desde la carpeta del proyecto, estos comandos abren una ventana de cmd por proceso:

```
start cmd /k lamport.exe 4
start cmd /k lamport.exe 3
start cmd /k lamport.exe 2
start cmd /k lamport.exe 1
```

### En 4 computadores

Cada máquina ejecuta su propio ID y recibe las IP de las cuatro, en orden:

```
lamport.exe <id> <ip_P1> <ip_P2> <ip_P3> <ip_P4>
```

Ejemplo en la máquina 2:

```
lamport.exe 2 192.168.1.11 192.168.1.12 192.168.1.13 192.168.1.14
```

Hay que permitir en el firewall de Windows los puertos TCP 5001 a 5004.

## Salida esperada

| Proceso | Evento             | Reloj |
| ------- | ------------------ | ----- |
| P1      | tarea local        | 1     |
| P1      | envía a P2         | 2     |
| P2      | recibe: max(0,2)+1 | 3     |
| P2      | tarea local        | 4     |
| P2      | envía a P3         | 5     |
| P3      | recibe: max(0,5)+1 | 6     |
| P3      | tarea local        | 7     |
| P3      | envía a P4         | 8     |
| P4      | recibe: max(0,8)+1 | 9     |
| P4      | tarea local        | 10    |

Ejemplo de salida de P4:

```
=== Proceso P4 iniciado ===
[P4 | reloj=0] Esperando el turno (mensaje de P3)...
[P4 | reloj=9] RECIBIDO de P3 (timestamp 8) -> max(0,8)+1
[P4 | reloj=10] Ejecutando la tarea de P4
[P4 | reloj=10] Fin de la aplicacion: P1 -> P2 -> P3 -> P4 completado
```

## Estructura del repositorio

```
lamport.cpp   código fuente
README.md     este archivo
.gitignore    excluye los ejecutables
```

## Autor

Poggio Joaquin Pablo
