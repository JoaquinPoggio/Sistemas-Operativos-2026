from dataclasses import dataclass


@dataclass(frozen=True)
class Nombre:
    """sin://<nodo>/<tipo>/<ruta>   (nodo vacío = en cualquier nodo)"""
    nodo: str
    tipo: str
    ruta: str

    @staticmethod
    def parse(texto: str) -> "Nombre":
        if not texto.startswith("sin://"):
            raise ValueError(f"Nombre inválido (debe empezar con sin://): {texto}")
        nodo, _, resto = texto[6:].partition("/")
        tipo, _, ruta = resto.partition("/")
        if not tipo or not ruta:
            raise ValueError(f"Nombre inválido (falta tipo o ruta): {texto}")
        return Nombre(nodo, tipo, ruta)

    def __str__(self) -> str:
        return f"sin://{self.nodo}/{self.tipo}/{self.ruta}"