typedef int (*Entry)(int argc, char *argv[], char *envp[]);

extern void __Heap_Initialize();

void Libc_Init(Entry entry, int argc, char *argv[], char *envp[]) {
	__Heap_Initialize();
	entry(argc, argv, envp);
}