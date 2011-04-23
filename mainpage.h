/**
*
* @author (c) Copyright 2006, Peter Jakubèo <pjakubco@gmail.com>
*
* @section Uvod Úvod
* Program slú¾i na defragmentovanie obrazu súborového systému FAT32. Je rozdelený
* do niekolkých funkèných modulov, z ktorých ka¾dý doká¾e pracova» so svojou
* problémovou doménou. Jednotlivé moduly navzájom spolupracujú volaním funkcií,
* ktoré sú viditelné (teda ktorých prototyp je deklarovaný v hlavièkových súboroch).
* Mojou snahou bolo zviditelni» èo mo¾no najmen¹í poèet funkcií, aby som dosiahol èo
* mo¾no najväè¹iu "zapúzdrenos»" (aj keï C nie je objektový). Umo¾nilo mi to potom
* lah¹ie odchytenie a odladenie chýb.
* 
* Pri písaní tohto projektu som mal na mysli motto: Keep It Simple Stupid (KISS).
* V prípade nejakých otázok, napí¹te mi na mail pjakubco@gmail.com
*
* <hr>
* @section Kompilovanie Kompilovanie programu
* Pre ulahèenie kompilovania som pou¾il program make a vytvoril som súbor s pravidlami Makefile. Programy
* sú napísané výluène pre kompilátor gcc a na to, aby sa dal projekt skompilova», je potrebné ma» nain¹talovaný
* program sed a xgettext.
* 
* Pravidlá v Makefile sú navrhnuté tak, aby sa spravila kompilácia ka¾dého súboru, ktorý má príponu .cpp, èi¾e
* pravidlá nezávisia od zdrojových súborov. Okrem toho sa pre ka¾dý zdrojový súbor zistia (pomocou gcc s prepínaèom
* -MM) závislosti na hlavièkových súboroch, tieto závislosti sa zapí¹u do samostatného súboru s príponou .d, ktorý sa
* pomocou make funkcie -include vlo¾í do Makefile a je tak zabezpeèená aj kontrola èasovej zmeny hlavièkových súborov,
* a je tak potom v prípade potreby vyvolaná kompilácia daného zdrojového súboru.
*
* Program xgettext je vyu¾ívaný na automatické "vytiahnutie" re»azcov, ktoré budú lokalizované a podporujú tak
* viacjazyènos». Dodatoèný preklad a vytvorenie binárneho lokalizaèného súboru je potrebné vykona» manuálne.
*
* <hr>
* @section Poznamky Poznámky Release
* 
* - v0.1b (12.11.2006)
*   - Opravené v¹etky podstatné bugy, sfunkènenie prepínaèov
* - v0.1a (4.11.2006) 
*   - jednoduchá defragmentácia, manipuluje sa iba s 1 clusterom. Nie je funkèná kontrola FAT systému, pracuje sa iba
*     so systémom FAT32. Algoritmus ako vo windowse (natlaèí v¹etko na zaèiatok)
*
* <hr>
* @section Poziadavky Po¾iadavky na systém
* Program bol napísaný v debian linuxe jadro 2.6.17; je v¹ak funkèný pre hocijakú distribúciu UNIX/LINUX, ktorá obsahuje
* nástroje gcc, make, xgettext, sed.
*
*/

