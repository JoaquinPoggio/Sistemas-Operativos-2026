import hashlib
import os
import socket
from multiprocessing import shared_memory


def _requeridos(datos, *campos):
    faltan = [c for c in campos if c not in datos]
    if faltan:
        raise ValueError(f"Faltan datos: {', '.join(faltan)}")


class Adaptador:
    """Traduce entre un nombre SIN y la dirección nativa del recurso.
    La clase base sirve como tipo genérico 'recurso'."""

    def registrar(self, ruta, datos):   # valida/normaliza al hacer bind
        return dict(datos)

    def resolver(self, ruta, datos):    # devuelve la dirección nativa actual
        return dict(datos)

    def liberar(self, ruta, datos):     # limpieza al hacer unbind
        pass


class AdaptadorArchivo(Adaptador):
    def registrar(self, ruta, datos):
        _requeridos(datos, "path")
        return {"path": os.path.abspath(datos["path"])}

    def resolver(self, ruta, datos):
        p = datos["path"]
        existe = os.path.exists(p)
        tamano = os.path.getsize(p) if existe and os.path.isfile(p) else None
        return {"path": p, "existe": existe, "tamano": tamano}


def _proceso_vivo(pid):
    if os.name == "nt":
        # En Windows os.kill(pid, 0) TERMINA el proceso, por eso se usa la API directamente
        import ctypes
        k32 = ctypes.windll.kernel32
        h = k32.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
        if not h:
            return False
        codigo = ctypes.c_ulong()
        ok = k32.GetExitCodeProcess(h, ctypes.byref(codigo))
        k32.CloseHandle(h)
        return bool(ok) and codigo.value == 259  # STILL_ACTIVE
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


class AdaptadorProceso(Adaptador):
    def registrar(self, ruta, datos):
        _requeridos(datos, "pid")
        return {"pid": int(datos["pid"])}

    def resolver(self, ruta, datos):
        return {"pid": datos["pid"], "activo": _proceso_vivo(datos["pid"])}


class AdaptadorServicio(Adaptador):
    def registrar(self, ruta, datos):
        _requeridos(datos, "host", "puerto")
        return {"host": datos["host"], "puerto": int(datos["puerto"])}

    def resolver(self, ruta, datos):
        try:
            with socket.create_connection((datos["host"], datos["puerto"]), timeout=0.5):
                activo = True
        except OSError:
            activo = False
        return {**datos, "activo": activo}


class AdaptadorMemoria(Adaptador):
    """Nombra segmentos de memoria compartida del nodo que los crea."""

    def __init__(self, nodo):
        self.nodo = nodo
        self._segmentos = {}

    def _id(self, ruta):
        return "sin_" + hashlib.sha1(f"{self.nodo}/{ruta}".encode()).hexdigest()[:12]

    def registrar(self, ruta, datos):
        tamano = int(datos.get("tamano", 1024))
        seg = shared_memory.SharedMemory(name=self._id(ruta), create=True, size=tamano)
        self._segmentos[ruta] = seg
        return {"segmento": seg.name, "tamano": tamano}

    def resolver(self, ruta, datos):
        return {**datos, "activo": ruta in self._segmentos}

    def liberar(self, ruta, datos):
        seg = self._segmentos.pop(ruta, None)
        if seg:
            seg.close()
            seg.unlink()


def crear_adaptadores(nodo):
    """Para agregar un tipo nuevo basta con sumar una entrada acá."""
    return {
        "archivo": AdaptadorArchivo(),
        "proceso": AdaptadorProceso(),
        "servicio": AdaptadorServicio(),
        "memoria": AdaptadorMemoria(nodo),
        "recurso": Adaptador(),
    }