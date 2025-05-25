
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pico/bootrom.h"
#include "stdio.h"

//matriz led
#include "hardware/pio.h"
#include "ws2812.pio.h"
#define IS_RGBW false
#define NUM_PIXELS 25
#define Frames 10
#define WS2812_PIN 7

// Variável global para armazenar a cor (Entre 0 e 255 para intensidade)
uint8_t led_r = 5;  // Intensidade do vermelho
uint8_t led_g = 5; // Intensidade do verde
uint8_t led_b = 5; // Intensidade do azul 

bool led_buffer[NUM_PIXELS];// Variável (protótipo)
// Frames que formam os números de 0 a 9
bool buffer_Numeros[Frames][NUM_PIXELS];//armazena números de zero a nove
void atualiza_buffer(bool buffer[], bool b[][NUM_PIXELS], int c); ///protótipo função que atualiza buffer

//protótipo funções que ligam leds da matriz 5x5
static inline void put_pixel(uint32_t pixel_grb);
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void set_one_led(uint8_t r, uint8_t g, uint8_t b);//liga os LEDs escolhidos 

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED  13

#define buzzer 21// pino do Buzzer na BitDogLab

#define BOTAO_A 5 
#define BOTAO_B 6 
#define BOTAO_JOY 22

static volatile uint32_t last_time_A = 0; // Armazena o tempo do último evento para Bot A(em microssegundos)
static volatile uint32_t last_time_B = 0; // Armazena o tempo do último evento para Bot B(em microssegundos)
static volatile uint32_t last_time_JOY = 0; // Armazena o tempo do último evento para Bot JOY(em microssegundos)

ssd1306_t ssd;

SemaphoreHandle_t xContadorSem;//semáforo de contagem 
SemaphoreHandle_t xButtonSem;//semáforo binário 

SemaphoreHandle_t xDisplayMutex;//cria mutex para proteger display 
uint16_t qtAtualPessoas = 0;//guarda quantidade atual de pessoas 

//Task botão A (incrementa o semáforo de contagem)
void vTaskEntrada(void *params)
{
    while(true) {
        if(gpio_get(BOTAO_A) == 0) {
            vTaskDelay(pdMS_TO_TICKS(70)); //70 ms de Debounce
            if(gpio_get(BOTAO_A) == 0) {
                xSemaphoreGive(xContadorSem);//incrementa contador do semáforo 
                if(qtAtualPessoas==8)
                {
                    //Beep único ao tentar entrar com ambiennte cheio 
                    pwm_set_gpio_level(buzzer, 200);//liga buzzer
                    vTaskDelay(pdMS_TO_TICKS(50));
                    pwm_set_gpio_level(buzzer, 0);//desliga buzzer
                }
            }
            while(gpio_get(BOTAO_A) == 0); // Espera soltar o botão
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
//Task botão B (decrementa o semáforo de contagem)
void vTaskSaida(void *params)// uint gpio, uint32_t events
{
    while(true) {
        if(gpio_get(BOTAO_B) == 0) {
            vTaskDelay(pdMS_TO_TICKS(70)); //70 ms de Debounce
            if(gpio_get(BOTAO_B) == 0) {
                xSemaphoreTake(xContadorSem, portMAX_DELAY);//decrementa contador do semáforo 
            }
            while(gpio_get(BOTAO_B) == 0); // Espera soltar o botão
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
// ISR do botão JOY semáforo binário 
void gpio_callback(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;  //Nenhum contexto de tarefa foi despertado
    xSemaphoreGiveFromISR(xButtonSem, &xHigherPriorityTaskWoken);    //Libera o semáforo
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // Troca o contexto da tarefa
}
// Task  do botão JOY (zera contador do semáforo)
void vTaskReset(void *params) {
    while (true) {
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) //berifica se semáforo binário está disponível
        {
            xQueueReset(xContadorSem); // Reinicia o semáforo para o valor inicial (0)
            //Beep duplo ao resetar semáforo de contagem 
            pwm_set_gpio_level(buzzer, 200); //liga buzzer
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(buzzer, 0);//desliga buzzer
            vTaskDelay(pdMS_TO_TICKS(100));
            pwm_set_gpio_level(buzzer, 200); //liga buzzer
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(buzzer, 0);//desliga buzzer
        }
    }
}
void vMatrizTask()//task pra Matriz LEDs
{
    //configuração PIO
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
    while (true)
    {
        if (uxSemaphoreGetCount(xContadorSem)==0)
        { // nenhuama pessoa no ambiente acende azul 
            led_b = 5;
        }
        else if (uxSemaphoreGetCount(xContadorSem)>0 && uxSemaphoreGetCount(xContadorSem)<=6)
        { //entre 1 e 6(max-2) pessoas no ambiente, acende LED verde
            led_g = 5;
        }
        else if (uxSemaphoreGetCount(xContadorSem)==7)
        { // 7 pessoas(máx-1) no ambiente, acende amarelo 
            led_r = 5;  
            led_g = 5; 
        }
        else if(uxSemaphoreGetCount(xContadorSem)==8)
        {//8 pessoas no ambiente(maximo) acende LED vermelho
            led_r = 5;
        }
        atualiza_buffer(led_buffer, buffer_Numeros, qtAtualPessoas); /// atualiza buffer        
        set_one_led(led_r, led_g, led_b); //imprime número de pessoas no ambiente 
        vTaskDelay(pdMS_TO_TICKS(100));
        led_r = 0;  
        led_g = 0; 
        led_b = 0;    
    }
}

// Tarefa que mostra dados no Display 
void vDisplayTask(void *params)
{
    char buffer[32];
    while (true)
    {
        // Aguarda mutex ficar disponível para entrar mo if
        if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY==pdTRUE))
        {
            qtAtualPessoas=uxSemaphoreGetCount(xContadorSem);
            ssd1306_fill(&ssd, 0);//limpa o display 
            sprintf(buffer, "QtPessoas: %d", qtAtualPessoas);
            ssd1306_draw_string(&ssd, buffer, 13, 31);
            ssd1306_draw_string(&ssd, "Cont. acesso", 15, 4);
            ssd1306_draw_string(&ssd, "Max: 8 pessoas", 7, 13);
            if(qtAtualPessoas==0){
                ssd1306_draw_string(&ssd, " Sala Vazia", 7, 49);
            }else if(qtAtualPessoas==8){
                ssd1306_draw_string(&ssd, " Sala Cheia", 7, 49);
            }else{
                ssd1306_draw_string(&ssd, " Sala ocupada", 7, 49);
            }
            ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0); // Desenha um retângulo
            ssd1306_line(&ssd, 3, 22, 123, 22, 1);           // Desenha uma linha
            ssd1306_send_data(&ssd);
            xSemaphoreGive(xDisplayMutex);
        }
    }
}
void vLedTask(void *params)//Liga lEDs  RGB dependendo da quantidade de pessoas 
{
    //definindo LED vermelho 
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED , GPIO_OUT);
    //definindo LED azul 
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE , GPIO_OUT);
    //definindo LED verde
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN , GPIO_OUT);

    //garante que LEDs iniciem apagados 
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);

    while (true)
    {
        if (uxSemaphoreGetCount(xContadorSem)==0)
        { // nenhuama pessoa no ambiente acende azul 
            gpio_put(LED_BLUE, 1);
        }
        else if (uxSemaphoreGetCount(xContadorSem)>0 && uxSemaphoreGetCount(xContadorSem)<=6)
        { //entre 1 e 6(max-2) pessoas no ambiente, acende LED verde
            gpio_put(LED_GREEN, 1);
        }
        else if (uxSemaphoreGetCount(xContadorSem)==7)
        { // 7 pessoas(máx-1) no ambiente, acende amarelo 
            gpio_put(LED_RED, 1);
            gpio_put(LED_GREEN, 1);
        }
        else if(uxSemaphoreGetCount(xContadorSem)==8)
        {//8 pessoas no ambiente(maximo) acende LED vermelho
            gpio_put(LED_RED, 1);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
        //apaga os LEDs para próxima verificação 
        gpio_put(LED_RED, 0);
        gpio_put(LED_GREEN, 0);
        gpio_put(LED_BLUE, 0);
    }
}
// ISR para BOOTSEL e botão de evento
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_us_since_boot(get_absolute_time());// Obtém o tempo atual em microssegundos
    if(gpio_get(BOTAO_JOY)==0 && (current_time - last_time_JOY) > 200000)//200 ms de debounce como condição 
    {
        last_time_JOY = current_time; // Atualiza o tempo do último evento
        gpio_callback(gpio, events);//função para semáforo binário 
    }
}
int main()
{
    stdio_init_all();

    // Inicialização do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    //configurando PWM
    uint pwm_wrap = 1999;// definindo valor de wrap referente a 12 bits do ADC
    gpio_set_function(buzzer, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(buzzer);
    pwm_set_wrap(slice_num, pwm_wrap);
    pwm_set_clkdiv(slice_num, 75.0);//divisor de clock 
    pwm_set_enabled(slice_num, true);// Ativa PWM

    // Configura os botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_init(BOTAO_JOY);
    gpio_set_dir(BOTAO_JOY, GPIO_IN);
    gpio_pull_up(BOTAO_JOY);

    gpio_set_irq_enabled_with_callback(BOTAO_JOY, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_A, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);


    xContadorSem = xSemaphoreCreateCounting(8, 0);// Cria semáforo de contagem (máximo 8, inicial 0)
    xButtonSem = xSemaphoreCreateBinary();// Cria semáforo binário

    // Cria o mutex do para proteção do display
    xDisplayMutex = xSemaphoreCreateMutex();
    // Cria tarefa
    xTaskCreate(vTaskSaida, "Task Saida", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);//Task obrigatória
    xTaskCreate(vTaskEntrada,"Task Entrada", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);//Task obrigatória
    xTaskCreate(vTaskReset,"Task Reset", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);//Task obrigatória
    xTaskCreate(vDisplayTask, "ContadorTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vLedTask, "TaskLEDsRGB", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vMatrizTask, "Matriz Task", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}

//-----------------Funções para matriz LEDs---------------------------
bool led_buffer[NUM_PIXELS]= { //Buffer para armazenar quais LEDs estão ligados matriz 5x5
    1, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0};
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}
void set_one_led(uint8_t r, uint8_t g, uint8_t b)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        if (led_buffer[i])
        {
            put_pixel(color); // Liga o LED com um no buffer
        }
        else
        {
            put_pixel(0); // Desliga os LEDs com zero no buffer
        }
    }
}
bool buffer_Numeros[Frames][NUM_PIXELS] =//armazena numeros de zero a nove
    {
      //{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24} referência para posição na BitDogLab matriz 5x5
        {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número zero
        {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0}, // para o número 1
        {0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 2
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 3
        {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0}, // para o número 4
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0}, // para o número 5
        {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0}, // para o número 6
        {0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 7
        {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}, // para o número 8
        {0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0}  // para o número 9
};
// função que atualiza o buffer de acordo o número de 0 a 9
void atualiza_buffer(bool buffer[],bool b[][NUM_PIXELS], int c)
{
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        buffer[i] = b[c][i];
    }
}
