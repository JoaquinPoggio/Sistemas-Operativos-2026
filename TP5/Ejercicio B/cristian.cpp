// cristian.cpp - Sincronizacion de relojes con el algoritmo de Cristian
//
//   cristian.exe servidor [desfasaje_ms]
//   cristian.exe cliente <ip_servidor> [desfasaje_ms]
//
// El cliente le pide la hora al servidor, mide el tiempo de ida y vuelta (RTT)
// y ajusta su reloj a:   hora_servidor + RTT/2
#include "comun.h"

const int PUERTO = 6000;
const int CONSULTAS = 5;   // se repite la consulta y se usa la de menor RTT

void modoServidor() {
    SOCKET srv = crearServidor(PUERTO);
    cout << "=== Servidor de hora (puerto " << PUERTO << ") ===" << endl;
    cout << "Hora del servidor: " << formatearHora(ahoraMs()) << endl;

    while (true) {
        SOCKET c = accept(srv, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;

        string msg;
        if (recibirLinea(c, msg) && msg == "HORA?") {
            long long t = ahoraMs();          // se toma lo mas tarde posible
            enviarLinea(c, to_string(t));
            cout << "[SERVIDOR] Hora enviada: " << formatearHora(t) << endl;
        }
        closesocket(c);
    }
}

void modoCliente(const string& ip) {
    cout << "=== Cliente (servidor de hora: " << ip << ":" << PUERTO << ") ===" << endl;
    cout << "Hora local ANTES de sincronizar:   " << formatearHora(ahoraMs()) << endl << endl;

    long long mejorRtt = -1;
    long long mejorDelta = 0;

    for (int i = 1; i <= CONSULTAS; i++) {
        SOCKET s = conectarA(ip, PUERTO, 10);
        if (s == INVALID_SOCKET) {
            cerr << "No se pudo conectar con el servidor" << endl;
            exit(1);
        }

        long long t0 = monotonicoUs();        // T0: sale el pedido
        enviarLinea(s, "HORA?");
        string resp;
        bool ok = recibirLinea(s, resp);
        long long t1 = monotonicoUs();        // T1: llega la respuesta
        long long localT1 = ahoraMs();        // hora local en el instante T1
        closesocket(s);
        if (!ok) {
            cerr << "El servidor cerro la conexion" << endl;
            exit(1);
        }

        long long ts = stoll(resp);                    // hora del servidor
        long long rtt = t1 - t0;                       // en microsegundos
        long long estimada = ts + (rtt / 2 + 500) / 1000;   // Ts + RTT/2 (en ms)
        long long delta = estimada - localT1;          // cuanto hay que corregir

        printf("Consulta %d: Ts=%s  RTT=%.3f ms  correccion=%+lld ms\n",
               i, formatearHora(ts).c_str(), rtt / 1000.0, delta);

        if (mejorRtt < 0 || rtt < mejorRtt) {
            mejorRtt = rtt;
            mejorDelta = delta;
        }
        Sleep(200);
    }

    offsetMs += mejorDelta;   // se aplica la correccion de la consulta con menor RTT

    printf("\nSe usa la consulta de menor RTT (%.3f ms): correccion de %+lld ms\n",
           mejorRtt / 1000.0, mejorDelta);
    printf("Precision maxima de la sincronizacion: +/- %.3f ms (RTT/2)\n", mejorRtt / 2000.0);
    cout << "Hora local DESPUES de sincronizar: " << formatearHora(ahoraMs()) << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Uso:\n"
             << "  cristian.exe servidor [desfasaje_ms]\n"
             << "  cristian.exe cliente <ip_servidor> [desfasaje_ms]\n";
        return 1;
    }
    string modo = argv[1];
    iniciarRed();

    if (modo == "servidor") {
        if (argc >= 3) offsetMs = atoll(argv[2]);
        modoServidor();
    } else if (modo == "cliente" && argc >= 3) {
        if (argc >= 4) offsetMs = atoll(argv[3]);
        modoCliente(argv[2]);
    } else {
        cout << "Modo invalido" << endl;
        return 1;
    }

    WSACleanup();
    return 0;
}