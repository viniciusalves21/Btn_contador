#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pico/bootrom.h"
#include "pio_matrix.pio.h"

#define NUM_PIXELS 25 //MATRIZ 5X5
#define WS2812_OUT 7
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12
#define BTN_A 5
#define BTN_B 6

volatile int numero_atual = 0; //armazena o numero atual a ser exibido na matriz de LEDS
const uint32_t debounce_delay = 200000; //define o tempo minimo entre dois pressionamentos de botão consecutivos
volatile uint32_t ultimotempo_a = 0; //armazena ultimo tempo em que o btn A foi pressionado(usado para implementar o debounce)
volatile uint32_t ultimotempo_b = 0; //armazena ultimo tempo em que o btn A foi pressionado(usado para implementar o debounce)

//converte os valores rgb em um valor que os leds ws2812 entendem
uint32_t matrix_rgb(double r, double g, double b){
    double brilho = 0.4; //Ajusta o brilho para 40%
    unsigned char R = (unsigned char)(r * 255 * brilho);
    unsigned char G = (unsigned char)(g * 255 * brilho);
    unsigned char B = (unsigned char)(b * 255 * brilho);
    return ( (uint32_t)(G) << 24 ) | ( (uint32_t)(R) << 16 ) | ( (uint32_t)(B) <<  8 );
}

//Controla o LED rgb (vermelho para piscar)
void piscar_rgb_led(){
    static bool estado = false; //armazena o estado atual do led
    gpio_put(LED_RED, estado); //define o valor lógico do led vermelho
    estado = !estado; //inverte o valor do estado para que alterne entre on/off
}

//função de callback chamada pelo timer configurado no main
bool timer_callback(struct repeating_timer *t){
    piscar_rgb_led(); //chama a função piscar led
    return true; //retorna verdadeito para indicar que deve continuar repetindo
}

void exibir_numero(PIO pio, uint sm, int numero){
    static uint32_t numeros[10][NUM_PIXELS] ={
        {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // 0
        {0,1,1,1,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,1,0, 0,0,1,0,0}, // 1        
        {1,1,1,1,1, 0,0,0,0,1, 0,1,1,1,0, 1,0,0,0,0, 1,1,1,1,1}, // 2
        {1,1,1,1,1, 1,0,0,0,0, 0,1,1,1,0, 1,0,0,0,0, 1,1,1,1,1}, // 3
        {0,0,0,0,1, 1,0,0,0,0, 1,1,1,1,1, 1,0,0,0,1, 1,0,0,0,1}, // 4
        {1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1}, // 5
        {1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1, 0,0,0,0,1, 1,1,1,1,1}, // 6
        {1,0,0,0,0, 0,0,0,1,0, 0,0,1,0,0, 0,1,0,0,0, 1,1,1,1,1}, // 7
        {1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1}, // 8
        {1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,1, 1,0,0,0,1, 1,1,1,1,1}  // 9
    };

    
    for (int i = 0; i < NUM_PIXELS; i++) {
        int mirrored_index = (i / 5) * 5 + (4 - (i % 5));
        pio_sm_put_blocking(pio, sm, matrix_rgb(numeros[numero][mirrored_index], numeros[numero][mirrored_index], numeros[numero][mirrored_index]));
    }
}

//chamada quando um dos botões são pressionados (interrupção)
void gpio_callback(uint gpio, uint32_t events){
    uint32_t agora = time_us_32(); //obtem o tempo atual em ms
    
    //verifica se o tempo decorrido desde o ultimo pressionamento é maior que o tempo debounce definido
    if (gpio == BTN_A && (agora - ultimotempo_a) > debounce_delay){
        numero_atual = (numero_atual + 1) % 10;
        ultimotempo_a = agora;
    }
    if (gpio == BTN_B && (agora - ultimotempo_b) > debounce_delay){
        numero_atual = (numero_atual - 1 + 10) % 10;
        ultimotempo_b = agora; //atualiza o ultmo tempo de pressionamento
    }
}

int main(){
    //iniciando os pinos
    stdio_init_all();
    PIO pio = pio0; //Obtém uma instância do periférico PIO (PIO0)
    uint sm = pio_claim_unused_sm(pio, true); // Aloca um state machine (SM) não utilizado do PIO
    uint offset = pio_add_program(pio, &pio_matrix_program); //Carrega o programa PIO (pio_matrix_program) na memória do PIO. Este programa é responsável por gerar os sinais de controle específicos para os LEDs WS2812.
    pio_matrix_program_init(pio, sm, offset, WS2812_OUT); //Inicializa o sm com o programa carregado e configura o pino de saída 

    //inicia apenas o led vermelho
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    //iniciando os botões com pull up
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    //configura o timer e add repetição de led vermelho para 200ms (5x por segundo)
    struct repeating_timer timer;
    add_repeating_timer_ms(200, timer_callback, NULL, &timer);

    while (true){
        exibir_numero(pio, sm, numero_atual);
        sleep_ms(50);

    }
    return 0;
}


