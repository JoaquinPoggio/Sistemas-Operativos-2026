// berkeley.cpp - Sincronizacion de relojes con el algoritmo de Berkeley
//
//   berkeley.exe esclavo <puerto> [desfasaje_ms]
//   berkeley.exe maestro <desfasaje_ms> <ip:puerto> [<ip:puerto> ...]
//
// El maestro (coordinador) le pregunta la hora a cada esclavo, calcula el
// promedio de los relojes (incluido el suyo) y le dice a cada maquina
// CUANTO debe ajustar su reloj (no la hora absoluta).
#include "comun.h"
#include <vector>

const long long UMBRAL_MS = 30000;   // relojes a mas de 30 s del maestro no entran en el promedio

struct Esclavo {
    string ip;
    int puerto = 0;
    SOCKET sock = INVALID_SOCKET;
    bool activo = false;        // contesto la consulta
    bool enPromedio = false;    // su diferencia esta dentro del umbral
    long long diff = 0;         // reloj del esclavo - reloj del maestro (ms)
};

long long valorAbs(long long x) { return x < 0 ? -x : x; }

// ---------------------------------------------------------------
void modoEsclavo(int puerto) {
    SOCKET srv = crearServidor(puerto);
    cout << "=== Esclavo (puerto " << puerto << ") ===" << endl;
    cout << "Hora local: " << formatearHora(ahoraMs()) << endl;

    while (true) {
        SOCKET c = accept(srv, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;

        string msg;
        while (recibirLinea(c, msg)) {
            if (msg == "HORA?") {
                long long t = ahoraMs();            // se toma lo mas tarde posible
                enviarLinea(c, to_string(t));
                cout << "[ESCLAVO] El maestro pidio la hora. Respondi: "
                     << formatearHora(t) << endl;
            } else if (msg.rfind("AJUSTE ", 0) == 0) {
                long long ajuste = stoll(msg.substr(7));
                long long antes = ahoraMs();
                offsetMs += ajuste;
                enviarLinea(c, "OK");
                printf("[ESCLAVO] Ajuste recibido: %+lld ms\n", ajuste);
                cout << "          Hora antes:   " << formatearHora(antes) << endl;
                cout << "          Hora despues: " << formatearHora(ahoraMs()) << endl;
                break;
            }
        }
        closesocket(c);
    }
}

// ---------------------------------------------------------------
void modoMaestro(vector<Esclavo>& esc) {
    cout << "=== Maestro ===" << endl;
    cout << "Hora del maestro ANTES: " << formatearHora(ahoraMs()) << endl << endl;

    // Fase 1: preguntar la hora a cada esclavo y calcular su diferencia
    cout << "--- Fase 1: consulta de horas ---" << endl;
    for (auto& e : esc) {
        e.sock = conectarA(e.ip, e.puerto, 5);
        if (e.sock == INVALID_SOCKET) {
            cout << e.ip << ":" << e.puerto << " -> no responde, se ignora" << endl;
            continue;
        }

        long long t0 = monotonicoUs();
        enviarLinea(e.sock, "HORA?");
        string resp;
        bool ok = recibirLinea(e.sock, resp);
        long long t1 = monotonicoUs();
        long long localRec = ahoraMs();     // hora del maestro al recibir la respuesta
        if (!ok) {
            closesocket(e.sock);
            e.sock = INVALID_SOCKET;
            continue;
        }

        long long ts = stoll(resp);
        long long rtt = t1 - t0;
        long long estimada = ts + (rtt / 2 + 500) / 1000;   // hora del esclavo corregida por RTT/2
        e.diff = estimada - localRec;
        e.activo = true;

        printf("%s:%d -> hora %s  RTT=%.3f ms  diferencia=%+lld ms\n",
               e.ip.c_str(), e.puerto, formatearHora(ts).c_str(), rtt / 1000.0, e.diff);
    }

    // Fase 2: promedio (el maestro cuenta con diferencia 0)
    long long suma = 0;
    int cantidad = 1;
    for (auto& e : esc) {
        if (e.activo && valorAbs(e.diff) <= UMBRAL_MS) {
            e.enPromedio = true;
            suma += e.diff;
            cantidad++;
        } else if (e.activo) {
            cout << e.ip << ":" << e.puerto << " queda fuera del promedio (diferencia muy grande)" << endl;
        }
    }
    long long promedio = suma / cantidad;
    printf("\n--- Fase 2: promedio de diferencias (%d relojes, incluido el maestro) = %+lld ms ---\n",
           cantidad, promedio);

    // Fase 3: enviar el ajuste a cada esclavo (ajuste = promedio - su diferencia)
    cout << "\n--- Fase 3: ajustes ---" << endl;
    for (auto& e : esc) {
        if (!e.activo) continue;
        long long ajuste = promedio - e.diff;
        enviarLinea(e.sock, "AJUSTE " + to_string(ajuste));
        string resp;
        recibirLinea(e.sock, resp);
        closesocket(e.sock);
        printf("%s:%d -> ajuste enviado: %+lld ms (%s)\n",
               e.ip.c_str(), e.puerto, ajuste, resp == "OK" ? "confirmado" : "sin confirmacion");
    }
    offsetMs += promedio;   // el maestro tambien se ajusta
    printf("Maestro -> ajuste propio: %+lld ms\n", promedio);

    cout << "\nHora del maestro DESPUES: " << formatearHora(ahoraMs()) << endl;
}

// ---------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Uso:\n"
             << "  berkeley.exe esclavo <puerto> [desfasaje_ms]\n"
             << "  berkeley.exe maestro <desfasaje_ms> <ip:puerto> [<ip:puerto> ...]\n";
        return 1;
    }
    string modo = argv[1];
    iniciarRed();

    if (modo == "esclavo") {
        if (argc >= 4) offsetMs = atoll(argv[3]);
        modoEsclavo(atoi(argv[2]));
    } else if (modo == "maestro" && argc >= 4) {
        offsetMs = atoll(argv[2]);
        vector<Esclavo> esc;
        for (int i = 3; i < argc; i++) {
            string a = argv[i];
            size_t p = a.find(':');
            if (p == string::npos) {
                cout << "Formato invalido: " << a << " (use ip:puerto)" << endl;
                return 1;
            }
            Esclavo e;
            e.ip = a.substr(0, p);
            e.puerto = atoi(a.substr(p + 1).c_str());
            esc.push_back(e);
        }
        modoMaestro(esc);
    } else {
        cout << "Modo invalido" << endl;
        return 1;
    }

    WSACleanup();
    return 0;
}