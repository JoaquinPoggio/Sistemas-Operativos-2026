import argparse
import json
import socket
import socketserver
import threading
import time

from .adaptadores import crear_adaptadores
from .nombre import Nombre

TTL = 30  # segundos que puede cachearse una resolución remota


def llamar(direccion, pedido, timeout=3):
    """Envía un pedido JSON (una línea) a un nodo y devuelve su resultado."""
    with socket.create_connection(direccion, timeout=timeout) as s:
        s.sendall((json.dumps(pedido) + "\n").encode("utf-8"))
        linea = s.makefile("r", encoding="utf-8").readline()
    respuesta = json.loads(linea)
    if not respuesta["ok"]:
        raise RuntimeError(respuesta["error"])
    return respuesta["resultado"]


class _Manejador(socketserver.StreamRequestHandler):
    def handle(self):
        for linea in self.rfile:
            try:
                pedido = json.loads(linea)
                respuesta = {"ok": True, "resultado": self.server.nodo.atender(pedido)}
            except Exception as e:
                respuesta = {"ok": False, "error": f"{type(e).__name__}: {e}"}
            self.wfile.write((json.dumps(respuesta, ensure_ascii=False) + "\n").encode("utf-8"))


class _Servidor(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class NodoSIN:
    def __init__(self, nombre, host="127.0.0.1", puerto=0, peers=None):
        self.nombre = nombre
        self.peers = dict(peers or {})       # {"nodo2": ("127.0.0.1", 9002)}
        self.tabla = {}                      # (tipo, ruta) -> datos nativos
        self.alias = {}                      # "sin://nodo/tipo/ruta" -> destino
        self.cache = {}                      # nombre -> (expira, resultado)
        self.adaptadores = crear_adaptadores(nombre)
        self.lock = threading.RLock()
        self._server = _Servidor((host, puerto), _Manejador)
        self._server.nodo = self
        self.host, self.puerto = self._server.server_address

    # ---------- ciclo de vida ----------
    def iniciar(self):
        threading.Thread(target=self._server.serve_forever, daemon=True).start()
        return self

    def detener(self):
        self._server.shutdown()
        self._server.server_close()
        for (tipo, ruta), datos in list(self.tabla.items()):
            self._adaptador(tipo).liberar(ruta, datos)

    # ---------- protocolo ----------
    def atender(self, p):
        op = p["op"]
        if op == "registrar":
            return self.registrar(p["nombre"], p.get("datos", {}))
        if op == "resolver":
            return self.resolver(p["nombre"], p.get("solo_local", False), p.get("usar_cache", True))
        if op == "baja":
            return self.baja(p["nombre"])
        if op == "alias":
            return self.crear_alias(p["nombre"], p["destino"])
        if op == "listar":
            return self.listar(p.get("tipo"))
        raise ValueError(f"Operación desconocida: {op}")

    # ---------- operaciones ----------
    def _adaptador(self, tipo):
        if tipo not in self.adaptadores:
            raise ValueError(f"Tipo desconocido: {tipo} (válidos: {', '.join(self.adaptadores)})")
        return self.adaptadores[tipo]

    def _local(self, n):
        if n.nodo not in ("", self.nombre):
            raise ValueError(f"Solo se puede registrar en el nodo local ({self.nombre})")
        return Nombre(self.nombre, n.tipo, n.ruta)

    def registrar(self, texto, datos):
        n = self._local(Nombre.parse(texto))
        with self.lock:
            if (n.tipo, n.ruta) in self.tabla:
                raise ValueError(f"Ya registrado: {n} (haga baja primero)")
            self.tabla[(n.tipo, n.ruta)] = self._adaptador(n.tipo).registrar(n.ruta, datos)
        return str(n)

    def baja(self, texto):
        n = self._local(Nombre.parse(texto))
        with self.lock:
            datos = self.tabla.pop((n.tipo, n.ruta), None)
            alias = self.alias.pop(str(n), None)
        if datos is None and alias is None:
            raise KeyError(f"No existe: {n}")
        if datos is not None:
            self._adaptador(n.tipo).liberar(n.ruta, datos)
        return str(n)

    def crear_alias(self, texto, destino):
        n = self._local(Nombre.parse(texto))
        Nombre.parse(destino)  # valida el formato
        with self.lock:
            self.alias[str(n)] = destino
        return str(n)

    def listar(self, tipo=None):
        with self.lock:
            nombres = [str(Nombre(self.nombre, t, r)) for (t, r) in self.tabla if tipo in (None, t)]
            return {"nombres": sorted(nombres), "alias": dict(self.alias)}

    def resolver(self, texto, solo_local=False, usar_cache=True, _prof=0):
        if _prof > 5:
            raise RuntimeError("Demasiados alias encadenados (posible ciclo)")
        n = Nombre.parse(texto)
        if n.nodo in ("", self.nombre):
            completo = str(Nombre(self.nombre, n.tipo, n.ruta))
            with self.lock:
                destino = self.alias.get(completo)
                datos = self.tabla.get((n.tipo, n.ruta))
            if destino:
                return self.resolver(destino, solo_local, usar_cache, _prof + 1)
            if datos is not None:
                return self._armar(n.tipo, n.ruta, datos)
            if n.nodo or solo_local:
                raise KeyError(f"No existe: {completo}")
            return self._buscar_en_peers(n, usar_cache)
        if solo_local:
            raise KeyError(f"No existe localmente: {texto}")
        return self._remoto(n.nodo, texto, usar_cache)

    def _armar(self, tipo, ruta, datos):
        return {
            "nombre": str(Nombre(self.nombre, tipo, ruta)),
            "nodo": self.nombre,
            "ubicacion_nodo": [self.host, self.puerto],
            "tipo": tipo,
            "direccion": self._adaptador(tipo).resolver(ruta, datos),
            "ttl": TTL,
        }

    def _remoto(self, nodo, texto, usar_cache):
        if nodo not in self.peers:
            raise KeyError(f"Nodo desconocido: {nodo}")
        hit = self.cache.get(texto)
        if usar_cache and hit and hit[0] > time.time():
            return {**hit[1], "cache": True}
        res = llamar(self.peers[nodo], {"op": "resolver", "nombre": texto, "solo_local": True})
        self.cache[texto] = (time.time() + res["ttl"], res)
        return res

    def _buscar_en_peers(self, n, usar_cache):
        for peer in self.peers:
            try:
                return self._remoto(peer, str(Nombre(peer, n.tipo, n.ruta)), usar_cache)
            except (KeyError, OSError, RuntimeError):
                continue
        raise KeyError(f"No se encontró {n} en ningún nodo")


def main():
    p = argparse.ArgumentParser(description="Nodo del Sistema de Nombrado Integral")
    p.add_argument("--nombre", required=True)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--puerto", type=int, required=True)
    p.add_argument("--peer", action="append", default=[], metavar="NOMBRE=HOST:PUERTO")
    a = p.parse_args()
    peers = {}
    for item in a.peer:
        nombre, _, dire = item.partition("=")
        host, _, puerto = dire.rpartition(":")
        peers[nombre] = (host, int(puerto))
    nodo = NodoSIN(a.nombre, a.host, a.puerto, peers).iniciar()
    print(f"Nodo {nodo.nombre} escuchando en {nodo.host}:{nodo.puerto} (Ctrl+C para salir)")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        nodo.detener()


if __name__ == "__main__":
    main()