/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "exec_parser.h"	

static so_exec_t *exec;
static char* path_to_exec;

static int create_freq_vec_for_mapped_pages() {
	
	int* mapped_pages;
	int number_of_pages;
	int pageSize = getpagesize();

	for (int i = 0; i < exec->segments_no; i++) {
		number_of_pages = exec->segments[i].file_size / pageSize;
		
		mapped_pages = calloc(number_of_pages, sizeof(int));

		if (!mapped_pages) {
			return -1;
		}
		exec->segments[i].data = mapped_pages;
	}
	return 0;
}

static int find_page(so_seg_t segment, siginfo_t *info) {
	int pageSize = getpagesize();
	int pageNum;
	int pageAdd;
	uintptr_t add = (uintptr_t) info->si_addr;
	//Calculez adresa paginii la care se produce faultul
	pageAdd = add - segment.vaddr;
	//Calculez numarul paginiii la care se produce faultul
	pageNum = pageAdd / pageSize;
	return pageNum;
}
//Elibereaza memoria din campul data al fiecarui segment
static void free_data()
{
	for(int i = 0; i < exec->segments_no; i++) {
		free(exec->segments[i].data);
	}
}

static void segv_handler(int signum, siginfo_t *info, void *context) {
	//Daca semnalul nu este SIGSEGV , atunci ies din handler
	if(signum != SIGSEGV || info->si_signo != SIGSEGV) {
		return;
	}
	uintptr_t add = (uintptr_t) info->si_addr; // adresa semnalului la care s-a produs page fault-ul
	size_t pageSize = (size_t) getpagesize();  // dimensiunea unei pagini , 4kb
	int pageNum ; // numarul paginii
	char* readData = malloc(pageSize * sizeof(char)); //buffer in care citesc datele din segment
	int flags_mmap = MAP_FIXED | MAP_SHARED | MAP_ANONYMOUS;//flagu-uri pentru mmap
	
	for(int i = 0 ; i < exec->segments_no; i++) {
		//Verificam daca adresa faultului se afla in segmentul curent
		if(add <= exec->segments[i].vaddr + exec->segments[i].mem_size && add >= exec->segments[i].vaddr) {

			pageNum = find_page(exec->segments[i], info);
			int* mapped_pages = (int*)(exec->segments[i].data);
			//Daca pagina este deja mapata , se apeleaza handler-ul default
			if(mapped_pages[pageNum] == 1) {
				fprintf(stderr, "Pagina este deja mapata: ");
				SIG_DFL(signum);
			}
			int file = open(path_to_exec, O_RDONLY);
			
			if(file == -1) {
				fprintf(stderr, "Fisierul nu a putut fii deschis: ");	
				SIG_DFL(signum);
			}
			//Demand paging cu permisiuni initiale de read/write pentru a putea scrie/citi in zona respectiva de memorie
			void* map_return = mmap((void*) (exec->segments[i].vaddr + pageNum * pageSize), pageSize, PERM_R | PERM_W ,flags_mmap, -1, 0);

			if(map_return == MAP_FAILED) {
				fprintf(stderr, "Maparea a esuat: ");
				SIG_DFL(signum);
			}
			//Verific daca pagina cautata iese din dimensiune fisierului
			if(exec->segments[i].file_size > pageNum * pageSize) {
				//Mut cursorul de read pana ajung la pagina pe care doresc sa o citesc	
				lseek(file, exec->segments[i].offset + pageSize * pageNum, SEEK_CUR);

				if(exec->segments[i].file_size < (pageNum + 1) * pageSize) {
					int bytes_read = read(file, readData, exec->segments[i].file_size - pageNum * pageSize);
					if(bytes_read == 0 ) {
						exit(EXIT_FAILURE);
					}
					//Copiez datele din segment in pagina
					memcpy((void*) (exec->segments[i].vaddr + pageNum * pageSize), readData, exec->segments[i].file_size - pageNum * pageSize);
				}
				else {
					int bytes_read = read(file, readData, pageSize);
					if(bytes_read == 0 ) {
						exit(EXIT_FAILURE);
					}
					memcpy((void*) (exec->segments[i].vaddr + pageNum * pageSize), readData, pageSize);
				}
			}
			//Schimb permisiunile initiale de read/write cu cele ale segmentului meu
			mprotect((void*) (exec->segments[i].vaddr + pageNum * pageSize), pageSize, exec->segments[i].perm);	
		
			close(file);

			mapped_pages[pageNum] = 1;

			free(readData);
			return;
		}
	}
	//Daca se parcurg toate segmentele si nu a fost gasit fault-ul se apeleaza handler-ul default
	fprintf(stderr, "Segmentul nu a fost gasit: ");
	SIG_DFL(signum);
}

int so_init_loader(void)
{	
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{

	path_to_exec = path;

	exec = so_parse_exec(path);

	if (!exec) { 
		return -1;
	}
	//Creez un vector de frecventa pentru paginile mapate
	int vec_create_succes = create_freq_vec_for_mapped_pages();
	if(vec_create_succes < 0) {
		return -1;
	}
	so_start_exec(exec, argv);

	free_data();
	return -1;
}