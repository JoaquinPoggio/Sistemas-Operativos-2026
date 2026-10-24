import json
import subprocess
import sys
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
from multiprocessing import shared_memory

from sin.cliente import ClienteSIN
from sin.nodo import NodoSIN


def mostrar(titulo, resultado):
    print(f"\n== {titulo}")
    print(json.dumps(resultado, indent=2, ensure_ascii=False))


# Dos nodos que se conocen entre sí (en una red real serían máquinas distintas)
n1 = NodoSIN("nodo1", puerto=9001, peers={"nodo2": ("127.0.0.1", 9002)}).iniciar()
n2 = NodoSIN("nodo2", puerto=9002, peers={"nodo1": ("127.0.0.1", 9001)}).iniciar()
c1, c2 = ClienteSIN(puerto=9001), ClienteSIN(puerto=9002)

# Recursos reales a nombrar
web = HTTPServer(("127.0.0.1", 8000), SimpleHTTPRequestHandler)
threading.Thread(target=web.serve_forever, daemon=True).start()
proc = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])

# 1) Archivo registrado en nodo1, consultado desde nodo2
c1.registrar("sin:///archivo/demo", path="demo.py")
mostrar("Archivo (pedido a nodo2)", c2.resolver("sin://nodo1/archivo/demo"))

# 2) Proceso en nodo2, buscado desde nodo1 SIN indicar el nodo
c2.registrar("sin:///proceso/worker-1", pid=proc.pid)
mostrar("Proceso (nodo1 no sabe dónde está)", c1.resolver("sin:///proceso/worker-1"))

# 3) Servicio en nodo2
c2.registrar("sin:///servicio/inventory", host="127.0.0.1", puerto=8000)
mostrar("Servicio", c1.resolver("sin:///servicio/inventory"))

# 4) Memoria compartida en nodo1, resuelta desde nodo2 y usada
c1.registrar("sin:///memoria/buffer", tamano=64)
dir_mem = c2.resolver("sin:///memoria/buffer")
mostrar("Memoria", dir_mem)
seg = shared_memory.SharedMemory(name=dir_mem["direccion"]["segmento"])
seg.buf[:5] = b"hola!"
print("Contenido del segmento:", bytes(seg.buf[:5]))
seg.close()

# 5) Alias entre nodos
c1.alias("sin:///servicio/web", "sin://nodo2/servicio/inventory")
mostrar("Alias web -> inventory", c1.resolver("sin:///servicio/web"))

# 6) Transparencia de ubicación: el servicio "migra" de nodo2 a nodo1
c2.baja("sin:///servicio/inventory")
c1.registrar("sin:///servicio/inventory", host="127.0.0.1", puerto=8000)
mostrar("Tras migrar (mismo nombre, otro nodo)", c2.resolver("sin:///servicio/inventory"))

mostrar("Catálogo nodo1", c1.listar())

n1.detener()
n2.detener()
web.shutdown()
proc.terminate()