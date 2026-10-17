UAE4ALL2 HD — Kickstart ROM (a cura dell'utente)
=============================================

Per motivi di copyright QUESTA CARTELLA NON CONTIENE NESSUNA ROM.
I file .PAT/.RTB presenti servono solo a WHDLoad e NON sono ROM.

Metti qui dentro, manualmente, le tue ROM originali:

  OBBLIGATORIE (la maggior parte dei giochi):
    kick13.rom              Kickstart 1.3  A500 (256 KB)
    kick31.rom              Kickstart 3.1  A1200 (512 KB)

  OPZIONALI:
    kick20.rom              Kickstart 2.04 A500+ (512 KB)
    kick37350.A600          Kickstart 2.05 A600
    kick40060.CD32          Kickstart 3.1 CD32 (512 KB)
    kick40060.CD32.ext      Extended ROM CD32 (512 KB)

DOVE METTERLE SU PS3:
  /dev_hdd0/uae4all/kickstarts/
  (oppure /dev_usb000/uae4all/kickstarts/ da USB)

SCHERMATE DIAGNOSTICHE AMIGA:
  VERDE lampeggiante = Chip RAM / Kickstart 1.3 mancante o errato.
    -> Usa preset "Amiga 500", metti kick13.rom (256 KB) originale.
  ROSSO lampeggiante = Kickstart del preset corrente mancante/danneggiato.
    -> Esempio: preset A1200 senza kick31.rom (512 KB), preset A600 senza
       kick37350.A600, preset CD32 senza kick40060.CD32 + .ext.
    -> NON usare file .ADF o .PAT/.RTB come ROM: non avviano nulla.

Verifica rapida (PC): la dimensione deve essere 262144 (256 KB) o 524288
(512 KB). File da ~901 KB (.adf) o da pochi KB (.pat/.rtb) NON sono ROM.
