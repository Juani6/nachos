#include "file_system.hh"
#include "open_file.hh"
#include "threads/semaphore.hh"
#include "threads/system.hh"
#include <string.h>
#include <stdio.h>

extern FileSystem *fileSystem;

// ============================================================================
// CASO 1: Test de Atomicidad y Seeks Independientes (Restricciones A y B)
// ============================================================================

static Semaphore *testAB_Sem;

void HiloEscritorA(void *arg) {
    OpenFile *f = fileSystem->Open("test_atomico.txt");
    ASSERT(f != nullptr);

    char datosA[100];
    memset(datosA, 'A', 100);

    int escritos = f->Write(datosA, 100);
    ASSERT(escritos == 100);
    DEBUG('f', "Hilo A escribió %d bytes.\n", escritos);

    delete f;
    testAB_Sem->V();
}

void HiloEscritorB(void *arg) {
    OpenFile *f = fileSystem->Open("test_atomico.txt");
    ASSERT(f != nullptr);

    char datosB[100];
    memset(datosB, 'B', 100);

    int escritos = f->Write(datosB, 100);
    ASSERT(escritos == 100);
    DEBUG('f', "Hilo B escribió %d bytes.\n", escritos);

    delete f;
    testAB_Sem->V();
}

void TestAtomicidadYSeek() {
    printf("\n--- INICIANDO TEST: ATOMICIDAD Y SEEK (Restricciones A y B) ---\n");

    bool ok = fileSystem->Create("test_atomico.txt", 200);
    ASSERT(ok);

    // Relleno inicial para que todo el archivo tenga contenido conocido
    OpenFile *init = fileSystem->Open("test_atomico.txt");
    ASSERT(init != nullptr);
    char relleno[200];
    memset(relleno, 'X', 200);
    init->Write(relleno, 200);
    delete init;

    testAB_Sem = new Semaphore("TestAB_Sem", 0);
    Thread *t1 = new Thread("Escritor A");
    Thread *t2 = new Thread("Escritor B");
    t1->Fork(HiloEscritorA, nullptr);
    t2->Fork(HiloEscritorB, nullptr);
    testAB_Sem->P();
    testAB_Sem->P();

    OpenFile *verificar = fileSystem->Open("test_atomico.txt");
    ASSERT(verificar != nullptr);
    char resultado[201];
    int leidos = verificar->Read(resultado, 200);
    ASSERT(leidos == 200);
    resultado[200] = '\0';
    delete verificar;

    // Con seeks independientes ambos empiezan en 0.
    // Uno sobreescribe al otro en [0,100); [100,200) queda 'X' o del último.
    // Lo que sí podemos garantizar: cada bloque de 100 es homogéneo (atomicidad).
    bool atomico = true;
    // Verificar que no hay mezcla de 'A' y 'B' dentro de ningún bloque de 100
    for (int bloque = 0; bloque < 2; bloque++) {
        char base = resultado[bloque * 100];
        // base puede ser 'A', 'B', o 'X' (no tocado)
        for (int i = 1; i < 100; i++) {
            if (resultado[bloque * 100 + i] != base) {
                atomico = false;
                printf("[ERROR] Mezcla en bloque %d, byte %d: '%c' != '%c'\n",
                       bloque, i, resultado[bloque * 100 + i], base);
            }
        }
    }

    if (atomico) {
        printf("[OK] No hay mezcla de bytes entre hilos (atomicidad garantizada).\n");
        printf("     Bloque 0: '%c' x100, Bloque 1: '%c' x100\n",
               resultado[0], resultado[100]);
    } else {
        printf("[FAIL] Se detectó mezcla de bytes.\n");
    }

    delete testAB_Sem;
    fileSystem->Remove("test_atomico.txt");
    printf("--- FIN TEST: ATOMICIDAD Y SEEK ---\n");
}

// ============================================================================
// CASO 2: Test de Borrado Diferido (Restricción C)
// ============================================================================

static Semaphore *semLectorListo;
static Semaphore *semBorradorListo;
static Semaphore *semFinTestC;
static bool testCPassed;

void HiloLectorC(void *arg) {
    OpenFile *f = fileSystem->Open("notas.txt");
    if (f == nullptr) {
        printf("[ERROR] No se pudo abrir notas.txt en el lector.\n");
        testCPassed = false;
        semFinTestC->V();
        return;
    }
    printf("[Lector] Archivo abierto. Avisando al borrador...\n");
    semLectorListo->V();

    semBorradorListo->P();

    // El archivo ya fue removido del directorio; debe seguir operativo.
    char texto[] = "Sigo vivo en memoria!";
    unsigned len = strlen(texto);
    int escritos = f->Write(texto, len);
    if ((unsigned) escritos != len) {
        printf("[ERROR] Write en archivo borrado retornó %d (esperado %u).\n", escritos, len);
        testCPassed = false;
        delete f;
        semFinTestC->V();
        return;
    }
    printf("[Lector] Write OK (%d bytes).\n", escritos);

    f->Seek(0);
    char buffer[30];
    memset(buffer, 0, sizeof(buffer));
    f->Read(buffer, len);
    if (strcmp(buffer, texto) != 0) {
        printf("[ERROR] Read devolvió '%s', esperado '%s'.\n", buffer, texto);
        testCPassed = false;
        delete f;
        semFinTestC->V();
        return;
    }
    printf("[Lector] Read OK: '%s'\n", buffer);

    // Reabrir debe fallar (ya no está en el directorio).
    OpenFile *fInvalido = fileSystem->Open("notas.txt");
    if (fInvalido != nullptr) {
        printf("[ERROR] Se pudo reabrir un archivo ya removido del directorio.\n");
        delete fInvalido;
        testCPassed = false;
    } else {
        printf("[OK] Open post-Remove falló correctamente.\n");
    }

    printf("[Lector] Cerrando (último Close, se gatilla borrado físico)...\n");
    delete f;

    semFinTestC->V();
}

void HiloBorradorC(void *arg) {
    semLectorListo->P();

    printf("[Borrador] Llamando a Remove(\"notas.txt\")...\n");
    bool exito = fileSystem->Remove("notas.txt");
    if (!exito) {
        printf("[ERROR] Remove falló.\n");
        testCPassed = false;
        semBorradorListo->V();
        return;
    }
    printf("[Borrador] Remove exitoso (borrado lógico).\n");
    semBorradorListo->V();
    // El borrador termina; el borrado físico lo hace el lector al hacer delete f.
}

void TestBorradoDiferido() {
    printf("\n--- INICIANDO TEST: BORRADO DIFERIDO (Restricción C) ---\n");

    bool ok = fileSystem->Create("notas.txt", 100);
    ASSERT(ok);

    semLectorListo  = new Semaphore("semLector",   0);
    semBorradorListo = new Semaphore("semBorrador", 0);
    semFinTestC     = new Semaphore("semFinC",      0);
    testCPassed     = true;

    Thread *tl = new Thread("Hilo Lector C");
    Thread *tb = new Thread("Hilo Borrador C");
    tl->Fork(HiloLectorC, nullptr);
    tb->Fork(HiloBorradorC, nullptr);

    semFinTestC->P();

    delete semLectorListo;
    delete semBorradorListo;
    delete semFinTestC;

    printf(testCPassed ? "[OK] Borrado diferido correcto.\n"
                       : "[FAIL] Borrado diferido falló.\n");
    printf("--- FIN TEST: BORRADO DIFERIDO ---\n");
}

// ============================================================================
// CASO 3: Test de Deadlock en escritura desalineada (WriteAt interno llama ReadAt)
// ============================================================================

// Para que el test sea realmente concurrente lanzamos dos hilos que hacen
// escrituras desalineadas simultáneas sobre el mismo archivo abierto.

static Semaphore *semDeadlock;

void HiloDeadlockA(void *arg) {
    OpenFile *f = (OpenFile *) arg;
    f->Seek(50);
    int w = f->Write("AAAA", 4);
    ASSERT(w == 4);
    DEBUG('f', "[DeadlockA] escribió 4 bytes en pos 50.\n");
    semDeadlock->V();
}

void HiloDeadlockB(void *arg) {
    OpenFile *f = (OpenFile *) arg;
    f->Seek(130); // también desalineado respecto a SECTOR_SIZE=128
    int w = f->Write("BBBB", 4);
    ASSERT(w == 4);
    DEBUG('f', "[DeadlockB] escribió 4 bytes en pos 130.\n");
    semDeadlock->V();
}

void TestEvitarDeadlock() {
    printf("\n--- INICIANDO TEST: EVITAR DEADLOCK EN OPERACIONES DESALINEADAS ---\n");

    bool ok = fileSystem->Create("deadlock.txt", 500);
    ASSERT(ok);

    // Relleno inicial para que las lecturas internas de WriteAt tengan datos.
    OpenFile *init = fileSystem->Open("deadlock.txt");
    ASSERT(init != nullptr);
    char basura[300];
    memset(basura, 'X', 300);
    init->Write(basura, 300);
    delete init;

    // Ambos hilos comparten la misma OpenFile para maximizar la contención.
    OpenFile *f = fileSystem->Open("deadlock.txt");
    ASSERT(f != nullptr);

    semDeadlock = new Semaphore("semDeadlock", 0);

    Thread *ta = new Thread("DeadlockA");
    Thread *tb = new Thread("DeadlockB");
    ta->Fork(HiloDeadlockA, f);
    tb->Fork(HiloDeadlockB, f);

    semDeadlock->P();
    semDeadlock->P();

    printf("[OK] Ambas escrituras desalineadas completaron sin deadlock.\n");

    delete f;
    delete semDeadlock;
    fileSystem->Remove("deadlock.txt");
    printf("--- FIN TEST: EVITAR DEADLOCK ---\n");
}

// ============================================================================
// Runner principal
// ============================================================================

void RunFileSystemConcurrencyTests() {
    printf("\n=======================================================\n");
    printf("EJECUTANDO BATERÍA DE TESTS DEL SISTEMA DE ARCHIVOS\n");
    printf("=======================================================\n");

    TestEvitarDeadlock();
    TestAtomicidadYSeek();
    TestBorradoDiferido();

    printf("\n=======================================================\n");
    printf("TODOS LOS TESTS FINALIZADOS\n");
    printf("=======================================================\n");
}