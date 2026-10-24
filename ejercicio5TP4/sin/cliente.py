import argparse
import json

from .nodo import llamar


class ClienteSIN:
    def __init__(self, host="127.0.0.1", puerto=9001):
        self.direccion = (host, puerto)

    def _pedir(self, **pedido):
        return llamar(self.direccion, pedido)

    def registrar(self, nombre, **datos):
        return self._pedir(op="registrar", nombre=nombre, datos=datos)

    def resolver(self, nombre, usar_cache=True):
        return self._pedir(op="resolver", nombre=nombre, usar_cache=usar_cache)

    def baja(self, nombre):
        return self._pedir(op="baja", nombre=nombre)

    def alias(self, nombre, destino):
        return self._pedir(op="alias", nombre=nombre, destino=destino)

    def listar(self, tipo=None):
        return self._pedir(op="listar", tipo=tipo)


def _valor(texto):
    try:
        return int(texto)
    except ValueError:
        return texto


def main():
    p = argparse.ArgumentParser(description="Cliente del SIN")
    p.add_argument("--nodo", default="127.0.0.1:9001", help="HOST:PUERTO del nodo a consultar")
    p.add_argument("--sin-cache", action="store_true", help="ignorar el caché al resolver")
    p.add_argument("operacion", choices=["registrar", "resolver", "baja", "alias", "listar"])
    p.add_argument("args", nargs="*", help="nombre [clave=valor ...] | nombre destino | tipo")
    a = p.parse_args()
    host, _, puerto = a.nodo.rpartition(":")
    c = ClienteSIN(host, int(puerto))
    try:
        if a.operacion == "registrar":
            datos = {k: _valor(v) for k, v in (x.split("=", 1) for x in a.args[1:])}
            res = c.registrar(a.args[0], **datos)
        elif a.operacion == "resolver":
            res = c.resolver(a.args[0], usar_cache=not a.sin_cache)
        elif a.operacion == "baja":
            res = c.baja(a.args[0])
        elif a.operacion == "alias":
            res = c.alias(a.args[0], a.args[1])
        else:
            res = c.listar(a.args[0] if a.args else None)
    except (RuntimeError, OSError) as e:
        raise SystemExit(f"Error: {e}")
    print(json.dumps(res, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()