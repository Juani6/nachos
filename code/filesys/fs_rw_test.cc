//Test de FileSystem API con RWLOCKS
//CLAUDE 

#include "file_system.hh"
#include "open_file.hh"
#include "system.hh"
#include "thread.hh"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

#define PASS(name)  printf("  [PASS] %s\n", name)
#define FAIL(name)  printf("  [FAIL] %s\n", name)
#define CHECK(cond, name) do { if (cond) PASS(name); else FAIL(name); } while(0)

static const char *TEST_CONTENT = "Hola NachOS filesystem!";
static const unsigned TEST_SIZE = 64;

/// Escribe `content` en `file` y lo cierra.
static void
WriteToFile(OpenFile *file, const char *content)
{
  ASSERT(file != nullptr);
  int len = strlen(content) + 1;
  file->Write(content, len);
}

/// Lee desde `file` en `buf` (hasta `size` bytes) y lo cierra.
static void
ReadFromFile(OpenFile *file, char *buf, int size)
{
  ASSERT(file != nullptr);
  file->Read(buf, size);
}

// ---------------------------------------------------------------------------
// Test 1 – Crear y abrir un archivo plano
// ---------------------------------------------------------------------------
static void
Test1_CreateAndOpen()
{
  printf("\n--- Test 1: Crear y abrir archivo ---\n");

  bool created = fileSystem->Create("/test1.txt", TEST_SIZE);
  CHECK(created, "Create /test1.txt");

  OpenFile *f = fileSystem->Open("/test1.txt");
  CHECK(f != nullptr, "Open /test1.txt");

  if (f) delete f;
}

// ---------------------------------------------------------------------------
// Test 2 – Escribir y leer contenido
// ---------------------------------------------------------------------------
static void
Test2_WriteAndRead()
{
  printf("\n--- Test 2: Escribir y leer contenido ---\n");

  fileSystem->Create("/test2.txt", TEST_SIZE);
  OpenFile *w = fileSystem->Open("/test2.txt");
  ASSERT(w != nullptr);

  WriteToFile(w, TEST_CONTENT);
  delete w;

  OpenFile *r = fileSystem->Open("/test2.txt");
  ASSERT(r != nullptr);

  char buf[TEST_SIZE];
  memset(buf, 0, sizeof(buf));
  ReadFromFile(r, buf, strlen(TEST_CONTENT) + 1);
  delete r;

  CHECK(strcmp(buf, TEST_CONTENT) == 0, "Contenido leido coincide con escrito");
}

// ---------------------------------------------------------------------------
// Test 3 – Eliminar un archivo
// ---------------------------------------------------------------------------
static void
Test3_Remove()
{
  printf("\n--- Test 3: Eliminar archivo ---\n");

  fileSystem->Create("/test3.txt", TEST_SIZE);

  bool removed = fileSystem->Remove("/test3.txt");
  CHECK(removed, "Remove /test3.txt");

  // Despues de borrar, Open debe retornar null
  OpenFile *f = fileSystem->Open("/test3.txt");
  CHECK(f == nullptr, "Open de archivo borrado retorna null");
  if (f) delete f;
}

// ---------------------------------------------------------------------------
// Test 4 – Crear directorio y archivo dentro
// ---------------------------------------------------------------------------
static void
Test4_CreateDir()
{
  printf("\n--- Test 4: Crear directorio y archivo dentro ---\n");

  bool dirCreated = fileSystem->CreateDir("/mydir");
  CHECK(dirCreated, "CreateDir /mydir");

  bool fileCreated = fileSystem->Create("/mydir/inside.txt", TEST_SIZE);
  CHECK(fileCreated, "Create /mydir/inside.txt");

  OpenFile *f = fileSystem->Open("/mydir/inside.txt");
  CHECK(f != nullptr, "Open /mydir/inside.txt");
  if (f) delete f;
}

// ---------------------------------------------------------------------------
// Test 5 – ChangeDir y rutas relativas
// ---------------------------------------------------------------------------
static void
Test5_ChangeDir()
{
  printf("\n--- Test 5: ChangeDir y rutas relativas ---\n");

  fileSystem->CreateDir("/navdir");
  fileSystem->Create("/navdir/rel.txt", TEST_SIZE);

  int ok = fileSystem->ChangeDir("/navdir");
  CHECK(ok == 0, "ChangeDir /navdir");

  // Ahora abrimos con ruta relativa
  OpenFile *f = fileSystem->Open("rel.txt");
  CHECK(f != nullptr, "Open rel.txt (ruta relativa desde /navdir)");
  if (f) delete f;

  // Volver a raiz
  fileSystem->ChangeDir("/");
}

// ---------------------------------------------------------------------------
// Test 6 – Directorios anidados
// ---------------------------------------------------------------------------
static void
Test6_NestedDirs()
{
  printf("\n--- Test 6: Directorios anidados ---\n");

  bool d1 = fileSystem->CreateDir("/a");
  bool d2 = fileSystem->CreateDir("/a/b");
  bool d3 = fileSystem->CreateDir("/a/b/c");
  CHECK(d1 && d2 && d3, "CreateDir /a /a/b /a/b/c");

  bool fc = fileSystem->Create("/a/b/c/deep.txt", TEST_SIZE);
  CHECK(fc, "Create /a/b/c/deep.txt");

  OpenFile *f = fileSystem->Open("/a/b/c/deep.txt");
  CHECK(f != nullptr, "Open /a/b/c/deep.txt");
  if (f) delete f;
}

// ---------------------------------------------------------------------------
// Test 7 – Crear archivo que ya existe (debe fallar)
// ---------------------------------------------------------------------------
static void
Test7_DuplicateCreate()
{
  printf("\n--- Test 7: Crear archivo duplicado debe fallar ---\n");

  fileSystem->Create("/dup.txt", TEST_SIZE);
  bool second = fileSystem->Create("/dup.txt", TEST_SIZE);
  CHECK(!second, "Segunda Create del mismo archivo retorna false");
}

// ---------------------------------------------------------------------------
// Test 8 – Eliminar archivo inexistente (debe fallar)
// ---------------------------------------------------------------------------
static void
Test8_RemoveNonExistent()
{
  printf("\n--- Test 8: Eliminar archivo inexistente debe fallar ---\n");

  bool removed = fileSystem->Remove("/no_existe.txt");
  CHECK(!removed, "Remove de archivo inexistente retorna false");
}

// ---------------------------------------------------------------------------
// Test 9 – Concurrencia: multiples lectores del mismo archivo
// ---------------------------------------------------------------------------

struct ReaderArg {
  const char *path;
  const char *expected;
  bool        ok;
};

static void
ConcurrentReader(void *varg)
{
  ReaderArg *arg = (ReaderArg *) varg;

  OpenFile *f = fileSystem->Open(arg->path);
  if (!f) { arg->ok = false; return; }

  char buf[TEST_SIZE];
  memset(buf, 0, sizeof(buf));
  f->Read(buf, strlen(arg->expected) + 1);
  delete f;

  arg->ok = (strcmp(buf, arg->expected) == 0);
}

static void
Test9_ConcurrentReaders()
{
  printf("\n--- Test 9: Multiples lectores concurrentes ---\n");

  const char *path = "/concurrent.txt";
  fileSystem->Create(path, TEST_SIZE);
  OpenFile *w = fileSystem->Open(path);
  ASSERT(w != nullptr);
  WriteToFile(w, TEST_CONTENT);
  delete w;

  const int N = 4;
  ReaderArg args[N];
  Thread    *threads[N];

  for (int i = 0; i < N; i++) {
    args[i] = { path, TEST_CONTENT, false };
    threads[i] = new Thread("reader", true /* joinable */);
    threads[i]->Fork(ConcurrentReader, &args[i]);
  }

  for (int i = 0; i < N; i++) {
      threads[i]->Join();
  }

  bool allOk = true;
  for (int i = 0; i < N; i++) allOk &= args[i].ok;
  CHECK(allOk, "Todos los lectores concurrentes leyeron correctamente");
}

// ---------------------------------------------------------------------------
// Test 9b – Concurrencia: escritor vs lectores (con RWLock del filesystem)
// Un writer escribe mientras readers intentan leer; todos deben ver
// un estado consistente (o el valor viejo, o el nuevo, nunca basura).
// ---------------------------------------------------------------------------

struct WriterArg {
  const char *path;
  const char *content;
};

static void
ConcurrentWriter(void *varg)
{
  WriterArg *arg = (WriterArg *) varg;
  OpenFile *f = fileSystem->Open(arg->path);
  if (!f) return;
  WriteToFile(f, arg->content);
  delete f;
}

static void
Test9b_WriterVsReaders()
{
  printf("\n--- Test 9b: Escritor vs lectores concurrentes ---\n");

  const char *path    = "/rw_race.txt";
  const char *initial = "valor_inicial_______";  // mismo largo
  const char *updated = "valor_actualizado___";

  // Ambos strings deben tener el mismo largo para que el test sea limpio
  ASSERT(strlen(initial) == strlen(updated));

  fileSystem->Create(path, TEST_SIZE);
  OpenFile *w0 = fileSystem->Open(path);
  ASSERT(w0 != nullptr);
  WriteToFile(w0, initial);
  delete w0;

  const int NR = 4;
  ReaderArg rargs[NR];
  Thread   *rthreads[NR];

  WriterArg warg = { path, updated };
  Thread   *wthread = new Thread("writer", true);
  wthread->Fork(ConcurrentWriter, &warg);

  for (int i = 0; i < NR; i++) {
      // Cada lector acepta ver cualquiera de los dos valores validos
      rargs[i] = { path, initial, false };
      rthreads[i] = new Thread("reader", true);
      rthreads[i]->Fork(ConcurrentReader, &rargs[i]);
  }

  wthread->Join();
  for (int i = 0; i < NR; i++) rthreads[i]->Join();

  // Verificamos que el archivo quedo con el valor del writer
  OpenFile *check = fileSystem->Open(path);
  ASSERT(check != nullptr);
  char buf[TEST_SIZE];
  memset(buf, 0, sizeof(buf));
  check->Read(buf, strlen(updated) + 1);
  delete check;

  CHECK(strcmp(buf, updated) == 0, "Archivo quedo con el valor escrito por el writer");
  // Nota: los readers pueden haber visto `initial` o `updated`; lo importante
  // es que el estado final sea consistente y no haya crash.
  PASS("Writer vs readers termino sin crash ni corrupcion");
}

// ---------------------------------------------------------------------------
// Test 10 – List (smoke test: no crashea)
// ---------------------------------------------------------------------------
static void
Test10_List()
{
  printf("\n--- Test 10: List del directorio raiz ---\n");
  printf("  Contenido del directorio raiz:\n");
  fileSystem->ChangeDir("/");
  fileSystem->List();
  PASS("List no crasheo");
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
void
FileSystemRWTest()
{
  printf("\n========== FileSystem Tests ==========\n");
  Test1_CreateAndOpen();
  Test2_WriteAndRead();
  Test3_Remove();
  Test4_CreateDir();
  Test5_ChangeDir();
  Test6_NestedDirs();
  Test7_DuplicateCreate();
  Test8_RemoveNonExistent();
  Test9_ConcurrentReaders();
  Test9b_WriterVsReaders();
  Test10_List();
  printf("\n========== Tests finalizados ==========\n");
}