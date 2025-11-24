#include <bits/stdc++.h>
#include <omp.h>
using namespace std;

// -----------------------------------------------------------
// Tipos básicos
// -----------------------------------------------------------
struct ANIMAL {
    char type;      // 'R' = conejo, 'F' = zorro
    int GEN_PROC;   // generaciones desde que nació / procreó
    int GEN_FOOD;   // solo zorros
};

typedef pair<int,int> Coord;
typedef map<Coord, ANIMAL> MapaAnimales;
typedef set<Coord> SetRocas;

// símbolos
const char VACIO  = '.';
const char ROCA   = '#';
const char CONEJO = 'R';
const char ZORRO  = 'F';

// -----------------------------------------------------------
// Estadísticas por generación
// -----------------------------------------------------------
struct Estadisticas {
    vector<int> rabbitsPerGen;
    vector<int> foxesPerGen;

    void registrar(int gen, int numRabbits, int numFoxes) {
        rabbitsPerGen.push_back(numRabbits);
        foxesPerGen.push_back(numFoxes);
    }

    void imprimir(const string &titulo) const {
        cout << "===== " << titulo << " =====\n";
        cout << "GEN\tRABBITS\tFOXES\n";
        for (int i = 0; i < (int)rabbitsPerGen.size(); ++i) {
            cout << i << '\t' << rabbitsPerGen[i] << '\t' << foxesPerGen[i] << '\n';
        }
        cout << '\n';
    }

};

// -----------------------------------------------------------
// Utilidades generales
// -----------------------------------------------------------

bool dentro(int x, int y, int R, int C) {
    return x >= 0 && x < R && y >= 0 && y < C;
}

// vecinos sin rocas en orden N, E, S, O usando set<Coord> de rocas
int obtenerVecinosSinRoca(const SetRocas &rocas,
                          int R, int C,
                          int x, int y,
                          Coord vecinos[4]) {
    int dx[4] = { -1,  0,  1,  0 }; // N, E, S, O
    int dy[4] = {  0,  1,  0, -1 };

    int n = 0;
    for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k];
        int ny = y + dy[k];
        if (!dentro(nx, ny, R, C)) continue;
        if (rocas.count(Coord(nx, ny))) continue; // evita rocas
        vecinos[n] = Coord(nx, ny);
        n++;
    }
    return n;
}

// índice según fórmula (G + X + Y) % P
int elegirIndiceMovimiento(int genActual, int x, int y, int P) {
    if (P == 0) return -1;
    return (genActual + x + y) % P;
}

// -----------------------------------------------------------
// Estructuras de "plan de movimiento"
// -----------------------------------------------------------

struct PasoConejo {
    bool   valido;
    Coord  origen;
    Coord  destino;
    ANIMAL padre;   // después de envejecer / resetear procreación
    bool   cria;    // si deja cría en origen
};

struct PasoZorro {
    bool   vivo;    // false si muere de hambre antes de moverse
    Coord  origen;
    Coord  destino;
    ANIMAL padre;   // después de envejecer / actualizar hambre
    bool   cria;    // si deja cría en origen
    bool   comio;   // si su plan era comer un conejo en destino
};

// -----------------------------------------------------------
// Cálculo de planes (NO modifican los mapas)
// -----------------------------------------------------------

void calcularPasoConejo(const Coord &origen,
                        const SetRocas &rocas,
                        const MapaAnimales &conejosOld,
                        const MapaAnimales &zorrosOld,
                        int R, int C,
                        int genActual,
                        int GEN_PROC_RABBITS,
                        PasoConejo &out) {
    auto it = conejosOld.find(origen);
    if (it == conejosOld.end()) {
        out.valido = false;
        return;
    }

    ANIMAL padre = it->second;
    if (padre.type != CONEJO) {
        out.valido = false;
        return;
    }

    int x = origen.first;
    int y = origen.second;

    padre.GEN_PROC++;   // envejece para procreación

    Coord vecinos[4];
    int nVecinos = obtenerVecinosSinRoca(rocas, R, C, x, y, vecinos);

    // casillas vacías (sin conejo ni zorro en el estado viejo)
    Coord opciones[4];
    int P = 0;
    for (int i = 0; i < nVecinos; ++i) {
        Coord c = vecinos[i];
        if (conejosOld.find(c) == conejosOld.end() &&
            zorrosOld.find(c)  == zorrosOld.end()) {
            opciones[P++] = c;
        }
    }

    Coord destino = origen;
    if (P > 0) {
        int idx = elegirIndiceMovimiento(genActual, x, y, P);
        destino = opciones[idx];
    }

    bool seMovio    = !(destino == origen);
    bool puedeCriar = seMovio && (padre.GEN_PROC > GEN_PROC_RABBITS);

    if (puedeCriar)
        padre.GEN_PROC = 0;

    out.valido  = true;
    out.origen  = origen;
    out.destino = destino;
    out.padre   = padre;
    out.cria    = puedeCriar;
}

void calcularPasoZorro(const Coord &origen,
                       const SetRocas &rocas,
                       const MapaAnimales &conejosOld,
                       const MapaAnimales &zorrosOld,
                       int R, int C,
                       int genActual,
                       int GEN_PROC_FOXES,
                       int GEN_FOOD_FOXES,
                       PasoZorro &out) {
    auto it = zorrosOld.find(origen);
    if (it == zorrosOld.end()) {
        out.vivo = false;
        return;
    }

    ANIMAL padre = it->second;
    if (padre.type != ZORRO) {
        out.vivo = false;
        return;
    }

    int x = origen.first;
    int y = origen.second;

    // aumenta procreación
    padre.GEN_PROC++;

    Coord vecinos[4];
    int nVecinos = obtenerVecinosSinRoca(rocas, R, C, x, y, vecinos);

    Coord opcionesConejo[4]; int Pcon = 0;
    Coord opcionesVacio[4];  int Pv   = 0;

    for (int i = 0; i < nVecinos; ++i) {
        Coord c = vecinos[i];
        bool hayConejo = (conejosOld.find(c) != conejosOld.end());
        bool hayZorro  = (zorrosOld.find(c)  != zorrosOld.end());

        if (hayConejo)
            opcionesConejo[Pcon++] = c;
        else if (!hayZorro)       // solo casillas realmente vacías
            opcionesVacio[Pv++]   = c;
        // si hay zorro, se ignora: NO se puede mover ahí
    }

    Coord destino = origen;
    bool comio    = false;

    if (Pcon > 0) {
        // hay al menos un conejo adyacente → intenta comer
        int idx = elegirIndiceMovimiento(genActual, x, y, Pcon);
        destino = opcionesConejo[idx];
        comio   = true;
        // NO tocamos GEN_FOOD aquí; solo el ganador lo resetea
    } else {
        // no hay conejo → aumenta hambre
        padre.GEN_FOOD++;

        // muere por hambre ANTES de intentar moverse a vacía
        if (padre.GEN_FOOD >= GEN_FOOD_FOXES) {
            out.vivo = false;
            return;
        }

        if (Pv > 0) {
            int idx = elegirIndiceMovimiento(genActual, x, y, Pv);
            destino = opcionesVacio[idx];
        }
        // si no hay vacía tampoco, se queda en origen
    }

    bool seMovio    = !(destino == origen);
    bool puedeCriar = seMovio && (padre.GEN_PROC > GEN_PROC_FOXES);

    if (puedeCriar)
        padre.GEN_PROC = 0;

    out.vivo    = true;
    out.origen  = origen;
    out.destino = destino;
    out.padre   = padre;
    out.cria    = puedeCriar;
    out.comio   = comio;
}

// -----------------------------------------------------------
// Aplicar planes (resolver conflictos simultáneos)
// -----------------------------------------------------------

void aplicarPasosConejos(const vector<PasoConejo> &pasos,
                         MapaAnimales &conejos) {
    MapaAnimales nuevo;

    for (const auto &p : pasos) {
        if (!p.valido) continue;

        // padre en destino (resolver conflictos con GEN_PROC)
        auto itDest = nuevo.find(p.destino);
        if (itDest != nuevo.end()) {
            if (p.padre.GEN_PROC > itDest->second.GEN_PROC)
                itDest->second = p.padre;
        } else {
            nuevo[p.destino] = p.padre;
        }

        // cría en origen
        if (p.cria) {
            ANIMAL cria{CONEJO, 0, 0};
            auto itCria = nuevo.find(p.origen);
            if (itCria != nuevo.end()) {
                if (cria.GEN_PROC > itCria->second.GEN_PROC)
                    itCria->second = cria;
            } else {
                nuevo[p.origen] = cria;
            }
        }
    }

    conejos.swap(nuevo);
}

void aplicarPasosZorros(const vector<PasoZorro> &pasos,
                        MapaAnimales &conejos,
                        MapaAnimales &zorros) {
    MapaAnimales zorrosNew;

    // 1) Agrupamos zorros por celda destino
    map<Coord, vector<int>> grupos;
    for (int i = 0; i < (int)pasos.size(); ++i) {
        if (!pasos[i].vivo) continue;
        grupos[pasos[i].destino].push_back(i);
    }

    // 2) Resolución de movimientos + comida (solo ganadores en destino)
    for (auto &par : grupos) {
        Coord dest = par.first;
        const vector<int> &idxs = par.second;

        // elegir zorro ganador:
        //  mayor GEN_PROC, y si empatan menor GEN_FOOD
        int w = idxs[0];
        for (int k = 1; k < (int)idxs.size(); ++k) {
            int j = idxs[k];
            const ANIMAL &A = pasos[j].padre;
            const ANIMAL &B = pasos[w].padre;
            if (A.GEN_PROC > B.GEN_PROC ||
               (A.GEN_PROC == B.GEN_PROC && A.GEN_FOOD < B.GEN_FOOD))
                w = j;
        }

        PasoZorro ganador = pasos[w]; // copia local

        // si el ganador quería comer y había conejo, se lo come
        auto itConejo = conejos.find(dest);
        if (ganador.comio && itConejo != conejos.end()) {
            conejos.erase(itConejo);
            ganador.padre.GEN_FOOD = 0;   // reset hambre solo del ganador
        }

        // colocar zorro ganador en destino
        zorrosNew[dest] = ganador.padre;
    }

    // 3) Crías en origen para TODOS los zorros que tienen cria=true
    //    (ganadores o perdedores del conflicto en destino)
    for (const auto &p : pasos) {
        if (!p.vivo || !p.cria) continue;

        Coord o = p.origen;
        ANIMAL cria;
        cria.type     = ZORRO;
        cria.GEN_PROC = 0;
        cria.GEN_FOOD = 0;

        auto itCria = zorrosNew.find(o);
        if (itCria != zorrosNew.end()) {
            ANIMAL &otro = itCria->second;
            bool ganaCria = false;
            if (cria.GEN_PROC > otro.GEN_PROC) ganaCria = true;
            else if (cria.GEN_PROC == otro.GEN_PROC &&
                     cria.GEN_FOOD <  otro.GEN_FOOD) ganaCria = true;
            if (ganaCria) itCria->second = cria;
        } else {
            zorrosNew[o] = cria;
        }
    }

    zorros.swap(zorrosNew);
}

// -----------------------------------------------------------
// Simulación SECUENCIAL (sin imprimir tableros) pero con stats
// -----------------------------------------------------------

void simularGeneracionesSec(const SetRocas &rocas,
                            int R, int C,
                            int GEN_PROC_RABBITS,
                            int GEN_PROC_FOXES,
                            int GEN_FOOD_FOXES,
                            int N_GEN,
                            MapaAnimales &conejos,
                            MapaAnimales &zorros,
                            Estadisticas &stats) {
    for (int gen = 0; gen < N_GEN; ++gen) {
        // ===== Registro de estadísticas: población en la generación actual =====
        stats.registrar(gen, (int)conejos.size(), (int)zorros.size());

        // ===== FASE CONEJOS =====
        const MapaAnimales &conejosOld = conejos;
        const MapaAnimales &zorrosOldR = zorros;

        vector<Coord> clavesConejos;
        clavesConejos.reserve(conejosOld.size());
        for (auto &kv : conejosOld)
            clavesConejos.push_back(kv.first);

        vector<PasoConejo> pasosConejos(clavesConejos.size());
        for (int i = 0; i < (int)clavesConejos.size(); ++i) {
            calcularPasoConejo(clavesConejos[i], rocas,
                               conejosOld, zorrosOldR,
                               R, C,
                               gen,
                               GEN_PROC_RABBITS,
                               pasosConejos[i]);
        }
        aplicarPasosConejos(pasosConejos, conejos);

        // ===== FASE ZORROS =====
        const MapaAnimales &zorrosOldF  = zorros;
        const MapaAnimales &conejosOldF = conejos;

        vector<Coord> clavesZorros;
        clavesZorros.reserve(zorrosOldF.size());
        for (auto &kv : zorrosOldF)
            clavesZorros.push_back(kv.first);

        vector<PasoZorro> pasosZorros(clavesZorros.size());
        for (int i = 0; i < (int)clavesZorros.size(); ++i) {
            calcularPasoZorro(clavesZorros[i], rocas,
                              conejosOldF, zorrosOldF,
                              R, C,
                              gen,
                              GEN_PROC_FOXES,
                              GEN_FOOD_FOXES,
                              pasosZorros[i]);
        }
        aplicarPasosZorros(pasosZorros, conejos, zorros);
    }
}

// -----------------------------------------------------------
// Simulación PARALELA (mismo algoritmo, bucles con OpenMP) con stats
// -----------------------------------------------------------

void simularGeneracionesPar(const SetRocas &rocas,
                            int R, int C,
                            int GEN_PROC_RABBITS,
                            int GEN_PROC_FOXES,
                            int GEN_FOOD_FOXES,
                            int N_GEN,
                            MapaAnimales &conejos,
                            MapaAnimales &zorros,
                            int numThreads,
                            Estadisticas &stats) {
    for (int gen = 0; gen < N_GEN; ++gen) { 
        stats.registrar(gen, (int)conejos.size(), (int)zorros.size());

        // ===== FASE CONEJOS =====
        const MapaAnimales &conejosOld = conejos;
        const MapaAnimales &zorrosOldR = zorros;

        vector<Coord> clavesConejos;
        clavesConejos.reserve(conejosOld.size());
        for (auto &kv : conejosOld)
            clavesConejos.push_back(kv.first);

        vector<PasoConejo> pasosConejos(clavesConejos.size());

        #pragma omp parallel for schedule(static) num_threads(numThreads)
        for (int i = 0; i < (int)clavesConejos.size(); ++i) {
            calcularPasoConejo(clavesConejos[i], rocas,
                               conejosOld, zorrosOldR,
                               R, C,
                               gen,
                               GEN_PROC_RABBITS,
                               pasosConejos[i]);
        }
        aplicarPasosConejos(pasosConejos, conejos);

        // ===== FASE ZORROS =====
        const MapaAnimales &zorrosOldF  = zorros;
        const MapaAnimales &conejosOldF = conejos;

        vector<Coord> clavesZorros;
        clavesZorros.reserve(zorrosOldF.size());
        for (auto &kv : zorrosOldF)
            clavesZorros.push_back(kv.first);

        vector<PasoZorro> pasosZorros(clavesZorros.size());

        #pragma omp parallel for schedule(static) num_threads(numThreads)
        for (int i = 0; i < (int)clavesZorros.size(); ++i) {
            calcularPasoZorro(clavesZorros[i], rocas,
                              conejosOldF, zorrosOldF,
                              R, C,
                              gen,
                              GEN_PROC_FOXES,
                              GEN_FOOD_FOXES,
                              pasosZorros[i]);
        }
        aplicarPasosZorros(pasosZorros, conejos, zorros);
    }
}

// -----------------------------------------------------------
// Impresión de estado final en formato solicitado
// -----------------------------------------------------------

void imprimirEstadoFinal(const SetRocas &rocas,
                         const MapaAnimales &conejos,
                         const MapaAnimales &zorros) {
    struct Item {
        Coord c;
        string tipo;  // "ROCK", "RABBIT", "FOX"
    };

    vector<Item> items;
    items.reserve(rocas.size() + conejos.size() + zorros.size());

    for (const Coord &c : rocas)
        items.push_back({c, "ROCK"});
    for (auto &kv : conejos)
        items.push_back({kv.first, "RABBIT"});
    for (auto &kv : zorros)
        items.push_back({kv.first, "FOX"});

    sort(items.begin(), items.end(),
         [](const Item &a, const Item &b){
             if (a.c.first != b.c.first) return a.c.first < b.c.first;
             return a.c.second < b.c.second;
         });

    for (const Item &it : items) {
        cout << it.tipo << ' ' << it.c.first << ' ' << it.c.second << '\n';
    }
}

// -----------------------------------------------------------
// MAIN
// -----------------------------------------------------------

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOOD_FOXES;
    int N_GEN, R, C, N;

    if (!(cin >> GEN_PROC_RABBITS >> GEN_PROC_FOXES >> GEN_FOOD_FOXES
              >> N_GEN >> R >> C >> N)) {
        return 0;
    }

    SetRocas rocas;
    MapaAnimales conejosInicial;
    MapaAnimales zorrosInicial;

    for (int i = 0; i < N; ++i) {
        string obj; int x, y;
        cin >> obj >> x >> y;
        if (obj == "ROCK") {
            rocas.insert(Coord(x, y));
        } else if (obj == "RABBIT") {
            ANIMAL a{CONEJO, 0, 0};
            conejosInicial[Coord(x, y)] = a;
        } else if (obj == "FOX") {
            ANIMAL a{ZORRO, 0, 0};
            zorrosInicial[Coord(x, y)] = a;
        }
    }

    // Copias para secuencial y paralelo
    MapaAnimales conejosSec = conejosInicial;
    MapaAnimales zorrosSec  = zorrosInicial;
    MapaAnimales conejosPar = conejosInicial;
    MapaAnimales zorrosPar  = zorrosInicial;

    int numThreads = 6; // o omp_get_max_threads();

    // Estadísticas
    Estadisticas statsSec;
    Estadisticas statsPar;

    // ===== EJECUCIÓN SECUENCIAL =====
    double t0 = omp_get_wtime();
    simularGeneracionesSec(rocas,
                           R, C,
                           GEN_PROC_RABBITS,
                           GEN_PROC_FOXES,
                           GEN_FOOD_FOXES,
                           N_GEN,
                           conejosSec,
                           zorrosSec,
                           statsSec);
    double t1 = omp_get_wtime();
    double tiempoSec = t1 - t0;

    cout << "===== SALIDA FINAL SECUENCIAL =====\n";
    imprimirEstadoFinal(rocas, conejosSec, zorrosSec);

    // Imprimir estadísticas secuencial
    statsSec.imprimir("ESTADISTICAS SECUENCIAL");

    // ===== EJECUCIÓN PARALELA =====
    double t2 = omp_get_wtime();
    simularGeneracionesPar(rocas,
                           R, C,
                           GEN_PROC_RABBITS,
                           GEN_PROC_FOXES,
                           GEN_FOOD_FOXES,
                           N_GEN,
                           conejosPar,
                           zorrosPar,
                           numThreads,
                           statsPar);
    double t3 = omp_get_wtime();
    double tiempoPar = t3 - t2;

    cout << "===== SALIDA FINAL PARALELA =====\n";
    imprimirEstadoFinal(rocas, conejosPar, zorrosPar);

    // Imprimir estadísticas paralela
    statsPar.imprimir("ESTADISTICAS PARALELA");


    // Bloque opcional de tiempos y Amdahl (coméntalo si el juez
    // solo quiere la salida del estado final).
    cout.setf(ios::fixed);
    cout << setprecision(6);
    cout << "Tiempo secuencial: " << tiempoSec << " segundos\n";
    cout << "Tiempo paralelo (" << numThreads << " hilos): " << tiempoPar << " segundos\n";
    if (tiempoPar > 0.0) {
        double S = tiempoSec / tiempoPar;
        double Nthreads = numThreads;
        double P = (Nthreads * (1.0 - 1.0 / S)) / (Nthreads - 1.0);
        double Smax = 1.0 / (1.0 - P);
        double mejora_pct = (tiempoSec - tiempoPar) / tiempoSec * 100.0;

        cout << "Speedup medido: " << S << "\n";
        cout << "Fraccion paralelizable (Amdahl) P ~ " << P << "\n";
        cout << "Speedup maximo teorico (N -> inf) ~ " << Smax << "\n";
        cout << "Reduccion de tiempo: " << mejora_pct << " %\n";
    }
    return 0;
}
