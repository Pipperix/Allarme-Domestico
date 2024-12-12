/* COMPONENTI 
- IR Receiver - PIN 2
- Pushbuttons - PIN 3
- RGB Led: Verde PIN 8 e Rosso - PIN 9
- Motion Sensor 1 - PIN 12
- Motion Sensor 2 - PIN 11
- Buzzer - PIN 10
*/

/* VARIABILI GLOBALI */
volatile uint16_t tick; // Tick del Timer2
volatile uint8_t alarm_state = 0; // Stato dell'allarme [0-3]
volatile uint8_t time = 0; // Temporizzazione delle task

// Decodifica segnale NEC
volatile byte  i, nec_state = 0, command;
volatile unsigned long nec_code;

// Codice Allarme
const String code = "1234";
volatile String ins_code = "";

// Macro per gestire il registro General Purpose
// Al bit 0 viene gestito il cambio frequenza del Buzzer
// Al bit 1 viene gestito il processo di decodifica del segnale NEC
// Al bit 2 viene gestita la lettura dei Pulsanti
#define SMY_FLAG(n, mode) (mode == 1 ? (GPIOR0 |= _BV(n)) : (GPIOR0 &= ~_BV(n)))

/* CONFIGURAZIONI TIMER 1 */
void TIMER1_reset(){
  TCCR1A = 0; // Reset del registro
  TCCR1B = 0; // Reset del registro
  TCNT1  = 0; // Reset del registro
  TIMSK1 = 0; // Reset del registro
  SMY_FLAG(0,0);  // Reset del bit per la frequenza del buzzer
  SMY_FLAG(2,0);  // Reset del bit per la lettura dei pulsanti
  EIMSK &= ~(1 << INT0); // Disattivo External Interrupt IR Receiver (INT0)
}

void TIMER1_IR_setup(){
  TCCR1A = 0; // Reset del registro
  TCCR1B = 0; // Reset del registro
  TCNT1  = 0; // Reset del registro
  TIMSK1 = 1 << TOIE1; // Abilito interrupt TIMER1_OVF
  EIMSK |= 1 << INT0; // Riattivo Externl Interrupt IR Receiver (INT0)
}

void TIMER1_buzzer_setup(){
  // Configura il Timer 1 in modalità Fast PWM, prescaler a 64
  TCCR1A = (1 << WGM11) | (1 << COM1B1);
  TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11) | (1 << CS10);
  // Chiamo la funzione che mi configura la frequenza
  updatePWMFrequency(1000);
}

/* CONFIGURAZIONE TIMER 2 */
void TIMER2_setup(){
  TCCR2A = 0; // Reset del registro
  TCCR2B = 0; // Reset del registro
  TCCR2B = 1 << CS22;   // Prescaler a 64
  TIMSK2 = 1 << TOIE2;  // Abilito interrupt TIMER2_OVF
}

void setup(){
  
  // Disabilito interrupt
  cli();

  // All'avvio configuro il Timer1 per la decodifica dei segnali IR
  TIMER1_IR_setup();
  // e configuro il Timer2 per eseguire le task
  TIMER2_setup();

  // Configuro il LED a "Verde"
  greenLED();

  // Interrupt INT0 e INT1
  EICRA |= 1 << ISC00 | 1 << ISC10;
  EIMSK |= 1 << INT0 | 1 << INT1;

  // Configuro il registro DDRB
  DDRB = 1 << PINB0 | 1 << PINB1 | 1 << PINB2; 
  // PIN8 - PIN9 - PIN10 ad output: RGB LED e Buzzer
  // PIN11 e PIN12 ad input: Motion Sensor

  // DDRD = 0 - Pushbuttons e IR Receiver ad input

  // Abilito gli interrupt
  sei();

}

// Timer 2 - Overflow scatta ogni 1.024ms
ISR(TIMER2_OVF_vect){
  
  // Ogni ms aggiorno il tick
  tick++;

  // Ogni mezzo secondo eseguo le task
  if(tick >= 488){ // 976 = 1sec, 488 = 0.5sec

    // Controllo lo stato dell'allarme
    switch(alarm_state){

      // Allarme disattivato
      case 0:
        break;

      // Allarme attivato
      case 1:
        time = 0; // Reset del tempo
        flash();  // Lampeggio del LED
        read_data();  // Lettura dei sensori
        break;

      // Attesa spegnimento o attivazione allarme sonoro
      case 2:
        time++; // Incremento il tempo
        flash();  // Lampeggio del LED

        // Dopo 10 secondi passo allo stato successivo
        if((time/2) >= 10){
          alarm_state = 3;
          TIMER1_reset(); // Reset del Timer1
          TIMER1_buzzer_setup();  // Configuro il Timer1 per il Buzzer
          time = 0; // Reset del tempo
        }
        break;

      // Allarme sonoro
      case 3:
        time++; // Incremento il tempo
        flash();  // Lampeggio del LED
        updateWaveform(); // Cambio frequenza del Buzzer

        // Dopo 5 minuti di allarme, riprende i controlli
        if((time/2) >= 300){
          alarm_state = 1;
          TIMER1_reset(); // Reset del Timer1
          TIMER1_IR_setup();  // Configuro il Timer1 per i segnali IR
          time = 0; // Reset del tempo
        }
        break;

      default:
        break;
    }

    // Azzero il tick
    tick = 0;

  }
}

// Interrupt Timer 1 per riconoscimento segnali IR
ISR(TIMER1_OVF_vect) {
  
  nec_state = 0;  // Reset del processo di decodifica
  TCCR1B = 0; // Disabilito il modulo

  // Se il messaggio è stato ricevuto con successo
  if(GPIOR0 & _BV(1)){
    SMY_FLAG(1,0);  // Reset del processo di decodifica
    command = bitSwap(nec_code >> 8);  // Salvo il codice del comando

    // Controllo stato dell'allarme
    if(alarm_state == 0){
      if(command == 162){  //Premuto "Rosso"
        PORTB &= ~(1 << PORTB0);  // Disattivo LED verde
        alarm_state = 1;
      }
    }else if(alarm_state != 0 && alarm_state != 3){
      //Riconoscimento per sequenza...
      switch(command){
        case 104: ins_code += "0"; break;
        case 48:  ins_code += "1"; break;
        case 24:  ins_code += "2"; break;
        case 122: ins_code += "3"; break;
        case 16:  ins_code += "4"; break;
        case 56:  ins_code += "5"; break;
        case 90:  ins_code += "6"; break;
        case 66:  ins_code += "7"; break;
        case 74:  ins_code += "8"; break;
        case 82:  ins_code += "9"; break;
        case 176: ins_code = "";   break;
        default: break;
      }

      // Spegnimento allarme
      if(ins_code == code){
        alarm_state = 0;
        ins_code = "";
        greenLED();
      } // Reset codice inserito
      else if(ins_code.length() == 4){
        ins_code = "";
      }
    }

    //Riattivo external interrupt su INT0 - IR Receiver
    EIMSK |= 1 << INT0; 
  }
}

// External Interrupt IR Receiver
ISR(INT0_vect){
  // Valore del timer 
  unsigned int timer_value;

  if(nec_state != 0){
    timer_value = TCNT1;  // Salvo il contenuto del registro
    TCNT1 = 0;            // Reset Timer1
  }

  switch(nec_state){
    // Inizio a ricevere il segnale (inizio dell'impulso da 9ms)
    case 0:               
      TCNT1 = 0;            // Reset del contatore
      TCCR1B = 1 << CS11;   // Abilito il prescaler a 8 (2 tick ogni 1 us)
      nec_state = 1;        // Prossimo stato: fine dell'impulso da 9ms (inizio dello spazio da 4.5ms)
      i = 0;
      return;

    // Inizio dello spazio da 4.5ms
    case 1:
      // Intervallo non valido ==> stop decodifica e reset
      if((timer_value > 19000) || (timer_value < 17000)){
        TCCR1B = 0;
        nec_state = 0;
      }
      else
        nec_state = 2;    // Prossimo stato: fine dello spazio di 4.5ms (inizio impulso da 562µs)
      return;

    // Inizio impulso da 562µs  
    case 2:
      if((timer_value > 10000) || (timer_value < 8000)){
        TCCR1B = 0;
        nec_state = 0;
      }
      else
        nec_state = 3;  // Prossimo stato: fine dell'impulso da 562µs (inizio dello spazio da 562µs o 1687µs)
      return;

    // Inizio dello spazio da 562µs o 1687µs
    case 3:
      if((timer_value > 1400) || (timer_value < 800)){
        TCCR1B = 0;
        nec_state = 0;
      }
      else
        nec_state = 4;  // Prossimo stato: fine dello spazio da 562µs o 1687µs
      return;

    case 4:
      if((timer_value > 3600) || (timer_value < 800)){
        TCCR1B = 0;
        nec_state = 0;
        return;
      }

      // Se l'ampiezza dello spazio è > 1ms (spazio lungo)
      if(timer_value > 2000) 
        nec_code |= 1UL << (31 - i); // Scrivo 1 al bit (31 - i)
      else  // Se l'ampiezza dello spazio  è < 1ms (spazio corto)
        nec_code &= ~(1UL << (31 - i)); // Scrivo 0 al bit (31 - i)
      
      i++; // Incremento i

      // Se tutti i bit sono stati ricevuti
      if(i > 31){
        SMY_FLAG(1,1);  // Processo di decodifica OK                 
        EIMSK &= ~(1 << INT0);  // Disabilito external interrupt (INT0)
        return;
      }
      nec_state = 3;  // Prossimo stato: fine dell'impulso da 562µs (inizio dello spazio da 562µs o 1687µs)
  }
}

// External Interrupt Pushbuttons
ISR(INT1_vect){
  // Aggiorno il registro per indicare l'intrusione
  SMY_FLAG(2,1);
}

/* FUNZIONI CUSTOM */

// Setup LED verde
void greenLED(){
  // Imposto a 0 il bit PORTB1 (rosso)
  PORTB &= ~(1 << PORTB1);
  // Imposto a 1 il bit PORTB0 (verde)
  PORTB |= 1 << PORTB0;
}

// Setup LED rosso lampeggiante
void flash(){
  //PORTB &= ~(1 << PORTB0);
  if(PINB & _BV(PINB1)){
    PORTB &= ~(1 << PORTB1);  // Spengo
  }else{
    PORTB |= 1 << PORTB1; // Accendo
  }
}

// Lettura per rilevamento intrusione
void read_data(){
  // Controllo i pin dei sensori di movimento e il registro dei pulsanti
  if((PINB & _BV(PINB4)) || (PINB & _BV(PINB3)) || (GPIOR0 & _BV(2))){
    alarm_state = 2;
  }
}

// Swap dei bit per decodifica corretta dei segnali NEC
byte bitSwap(byte command){
  command = ((command & 0x01) << 7) |
            ((command & 0x02) << 5) |
            ((command & 0x04) << 3) |
            ((command & 0x08) << 1) |
            ((command & 0x10) >> 1) |
            ((command & 0x20) >> 3) |
            ((command & 0x40) >> 5) |
            ((command & 0x80) >> 7);
  return command;
}

// Tempo per ogni cambio di lunghezza d'onda del Buzzer
void updatePWMFrequency(unsigned long frequency) {
  // Calcola il nuovo periodo per la frequenza data
  unsigned long period = F_CPU / (64UL * frequency) - 1;
  
  // Imposta il nuovo periodo nel registro ICR1
  ICR1 = period;
}

// Cambio della lunghezza d'onda del Buzzer
void updateWaveform() {
  if (GPIOR0 & _BV(0)) {
    updatePWMFrequency(1000); // Imposto frequenza a 1000Hz
    SMY_FLAG(0,0);
  } else {
    updatePWMFrequency(500);  // Imposto frequenza a 500Hz
    SMY_FLAG(0,1);
  }
}

void loop(){}