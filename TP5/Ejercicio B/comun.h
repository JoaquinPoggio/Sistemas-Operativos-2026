// comun.h - funciones compartidas por cristian.cpp y berkeley.cpp
#pragma once
#define _CRT_SECURE_NO_WARNINGS
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ---------- Reloj de esta computadora ----------
// Para no tocar la hora real de Windows (requiere ser administrador) se usa
// un reloj propio: hora del sistema + un desfasaje (offsetMs).
// Con el desfasaje inicial se simula un reloj adelantado (+) o atrasado (-).
// Sincronizar = corregir offsetMs.
long long offsetMs = 0;

long long ahoraMs() {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);          // hora del sistema, alta precision
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    // FILETIME cuenta de a 100 ns desde 1601 -> pasar a ms desde 1970
    long long ms = (long long)(u.QuadPart / 10000ULL) - 11644473600000LL;
    return ms + offsetMs;
}

// Reloj monotonico en microsegundos: sirve para medir el RTT
long long monotonicoUs() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return c.QuadPart * 1000000LL / f.QuadPart;
}

string formatearHora(long long ms) {
    time_t seg = (time_t)(ms / 1000);
    tm* t = localtime(&seg);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
             t->tm_hour, t->tm_min, t->tm_sec, (int)(ms % 1000));
    return string(buf);
}

// ---------- Sockets ----------
void iniciarRed() {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) {
        cerr << "Error al iniciar Winsock" << endl;
        exit(1);
    }
}

SOCKET crearServidor(int puerto) {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(puerto);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cerr << "Error en bind: el puerto " << puerto << " esta ocupado?" << endl;
        exit(1);
    }
    listen(srv, 8);
    return srv;
}

// Se conecta a ip:puerto; reintenta cada 1 s. Devuelve INVALID_SOCKET si se agotan los intentos.
SOCKET conectarA(const string& ip, int puerto, int intentos) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(puerto);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    for (int i = 0; i < intentos; i++) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        int nodelay = 1;  // evita demoras de Nagle al medir el RTT
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
        if (connect(s, (sockaddr*)&addr, sizeof(addr)) == 0) return s;
        closesocket(s);
        cout << "  (sin respuesta de " << ip << ":" << puerto << ", reintentando...)" << endl;
        Sleep(1000);
    }
    return INVALID_SOCKET;
}

// Los mensajes son lineas de texto terminadas en '\n'
bool enviarLinea(SOCKET s, const string& txt) {
    string m = txt + "\n";
    return send(s, m.c_str(), (int)m.size(), 0) == (int)m.size();
}

bool recibirLinea(SOCKET s, string& out) {
    out.clear();
    char c;
    while (true) {
        int n = recv(s, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') return true;
        out += c;
    }
}