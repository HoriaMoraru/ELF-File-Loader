# ELF-File-Loader
Implemented a shared library for loading executable ELF format files for linux





Tema propune implementarea unui handler de seg-faulturi ce functioneaza
prin demand paging.
Se parcurg segmentele executabilului si se cauta segmentul in care se produce
semnalul de seg-fault(se verifica daca adresa semnalului , ce are informatiile
retinute in structura data ca parametru siginfo_t se afla intre adresa de inceput
a segmentului curent si adresa de final a acestuia).
Mapez in memorie o pagina la adresa paginii care imi produce mie seg fault-ul
cu permisiuni initiale de READ/WRITE , folosing flag-ul MAP_FIXED pentru a mapa
fix la adresa data in mmap , MAP_SHARED pentru a fii vizibil si de catre alte
procese (cred ca ar fi mers si MAP_PRIVATE), si MAP_ANONYMOUS pentru a-mi zeroiza
continutul.
Deschid fisierul doar read only pentru a citi din el, mut cursorul de read cu offset
pentru a ajunge exact la segmentul meu in fisier, si apoi ma mut pe pagina
cautata, citesc din segment si copiez in pagina noua.
Apoi folosesc mprotect pentru a schimba permisiunile initiale de r/w cu cele
ale segmentului meu.
Pentru a verifica daca pagina a fost deja mapata creez un vector de frecventa
pentru a retine acest fapt.
