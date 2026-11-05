#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int N = 4;               // cantidad de procesos
const int PUERTO_BASE = 5000;  // P1 -> 5001, P2 -> 5002, P3 -> 5003, P4 -> 5004

int miId = 0;
int reloj = 0;                       // reloj lógico de Lamport
vector<string> ips(N + 1, "127.0.0.1");  // IP de cada proceso (índices 1..4)

void mostrar(const string& msg) {
    cout << "[P" << miId << " | reloj=" << reloj << "] " << msg << endl;
}

// Regla 1: evento local -> incrementa el reloj
void eventoLocal(const string& descripcion) {
    reloj++;
    mostrar(descripcion);
}

// Regla 1: antes de enviar se incrementa el reloj y se manda como timestamp
void enviar(int destino, const string& texto) {
    reloj++;
    int ts = reloj;
    string paquete = to_string(miId) + "|" + to_string(ts) + "|" + texto;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PUERTO_BASE + destino);
    inet_pton(AF_INET, ips[destino].c_str(), &addr.sin_addr);

    // Reintenta hasta que el destino esté escuchando (permite arrancar en cualquier orden)
    while (true) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            cerr << "Error al crear el socket" << endl;
            exit(1);
        }
        if (connect(s, (sockaddr*)&addr, sizeof(addr)) == 0) {
            send(s, paquete.c_str(), (int)paquete.size(), 0);
            closesocket(s);
            break;
        }
        closesocket(s);
        mostrar("P" + to_string(destino) + " todavia no esta listo, reintentando...");
        this_thread::sleep_for(chrono::seconds(1));
    }
    mostrar("ENVIADO a P" + to_string(destino) + " con timestamp " + to_string(ts));
}

SOCKET crearServidor() {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PUERTO_BASE + miId);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Error en bind (puerto " << PUERTO_BASE + miId << " ocupado?)" << endl;
        exit(1);
    }
    listen(srv, 1);
    return srv;
}

// Regla 2: recepción -> reloj = max(local, timestamp) + 1
void recibir(SOCKET srv) {
    SOCKET c = accept(srv, nullptr, nullptr);
    char buf[512];
    int n = recv(c, buf, sizeof(buf) - 1, 0);
    closesocket(c);
    if (n <= 0) {
        cerr << "Mensaje vacio o error de recepcion" << endl;
        exit(1);
    }
    buf[n] = '\0';

    stringstream ss(buf);
    string origen, tsStr, texto;
    getline(ss, origen, '|');
    getline(ss, tsStr, '|');
    getline(ss, texto);

    int ts = stoi(tsStr);
    int antes = reloj;
    reloj = max(reloj, ts) + 1;
    mostrar("RECIBIDO de P" + origen + " (timestamp " + tsStr + ") -> max(" +
            to_string(antes) + "," + tsStr + ")+1");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Uso: lamport.exe <id 1-4> [ip_P1 ip_P2 ip_P3 ip_P4]" << endl;
        return 1;
    }
    miId = atoi(argv[1]);
    if (miId < 1 || miId > N) {
        cout << "El id debe estar entre 1 y " << N << endl;
        return 1;
    }
    for (int i = 1; i <= N && i + 1 < argc; i++) ips[i] = argv[i + 1];

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "Error al iniciar Winsock" << endl;
        return 1;
    }

    cout << "=== Proceso P" << miId << " iniciado ===" << endl;

    if (miId == 1) {
        // P1 arranca la aplicación
        eventoLocal("Ejecutando la tarea de P1 (inicio de la aplicacion)");
        enviar(2, "TURNO");
    } else {
        SOCKET srv = crearServidor();
        mostrar("Esperando el turno (mensaje de P" + to_string(miId - 1) + ")...");
        recibir(srv);
        closesocket(srv);

        eventoLocal("Ejecutando la tarea de P" + to_string(miId));

        if (miId < N) enviar(miId + 1, "TURNO");
        else mostrar("Fin de la aplicacion: P1 -> P2 -> P3 -> P4 completado");
    }

    WSACleanup();
    return 0;
}