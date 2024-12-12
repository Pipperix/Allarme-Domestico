# Allarme-Domestico
Il progetto consiste nella realizzazione di un sistema di allarme per appartamento, composto da due sensori di movimento, due pulsanti che simulano sensori "Finestra", un ricevitore IR per attivare e disattivare l'allarme, un LED RGB che segnala lo stato del sistema e un buzzer per l'emissione di un allarme sonoro.

Il sistema funziona in due modalità principali: quando disattivato, il LED rimane verde fisso; quando attivato tramite il tasto rosso del telecomando, il LED lampeggia in rosso. Se un sensore rileva un'intrusione, il sistema attende 10 secondi prima di attivare un allarme sonoro con doppia tonalità (1000 e 500 Hz) per 5 minuti, a meno che non venga disattivato immettendo un codice di sicurezza a 4 cifre entro il tempo limite.

L'implementazione prevede un'architettura modulare con elaborazione in background per: la lettura periodica dello stato dei sensori (attivata da interrupt del clock), la gestione del telecomando con riconoscimento dei tasti "Rosso" e numerici, e il controllo del lampeggio del LED (anch'esso basato su interrupt del clock). 
La funzione `loop()` rimane vuota, poiché tutte le operazioni sono gestite in modo asincrono.

Simulazione [Allarme Domestico](https://wokwi.com/projects/372573036428062721) su Wokwi