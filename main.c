
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

uint BeepCurto=1;//variável auiliar para beep com ambiente em capacidade máxima

static volatile uint32_t last_time_A = 0; // Armazena o tempo do último evento para Bot A(em microssegundos)
static volatile uint32_t last_time_B = 0; // Armazena o tempo do último evento para Bot B(em microssegundos)
static volatile uint32_t last_time_JOY = 0; // Armazena o tempo do último evento para Bot JOY(em microssegundos)

ssd1306_t ssd;
SemaphoreHandle_t xContadorSem;
uint16_t qtAtualPessoas = 0;//guarda quantidade atual de pessoas 

// ISR do botão A (incrementa o semáforo de contagem)
void vTaskEntrada(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ISR do botão B (decrementa o semáforo de contagem)
void vTaskSaida(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreTakeFromISR(xContadorSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Tarefa que consome mostra dados no Display 
void vContadorTask(void *params)
{
    char buffer[32];

    while (true)
    {
        // Aguarda semáforo (um evento)
        /*if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE)
        {*/
            
            qtAtualPessoas=uxSemaphoreGetCount(xContadorSem);
            // Atualiza display com a nova contagem
            ssd1306_fill(&ssd, 0);//limpa o display 
            sprintf(buffer, "QtPessoas: %d", qtAtualPessoas);
            //ssd1306_draw_string(&ssd, "Evento ", 5, 10);
            //ssd1306_draw_string(&ssd, "recebido!", 5, 19);
            ssd1306_draw_string(&ssd, buffer, 13, 31);
            ssd1306_draw_string(&ssd, "Cont. acesso", 15, 4);
            ssd1306_draw_string(&ssd, "Max:10 pessoas", 7, 13);
            if(qtAtualPessoas==0){
                ssd1306_draw_string(&ssd, " Sala Vazia", 7, 49);
            }else if(qtAtualPessoas==10){
                ssd1306_draw_string(&ssd, " Sala Cheia", 7, 49);
            }else{
                ssd1306_draw_string(&ssd, " Sala ocupada", 7, 49);
            }
            ssd1306_rect(&ssd, 3, 3, 122, 60, 1, 0); // Desenha um retângulo
            ssd1306_line(&ssd, 3, 22, 123, 22, 1);           // Desenha uma linha
            ssd1306_send_data(&ssd);
             // Simula tempo de processamento
            //vTaskDelay(pdMS_TO_TICKS(50));

        //}
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
    if (gpio_get(BOTAO_B)==0 && (current_time - last_time_B) > 200000)//200 ms de debounce como condição 
    {
        last_time_B = current_time; // Atualiza o tempo do último evento
        vTaskSaida(gpio, events);
    }
    else if (gpio_get(BOTAO_A)==0 && (current_time - last_time_A) > 200000)//200 ms de debounce como condição 
    {
        last_time_A = current_time; // Atualiza o tempo do último evento
        if(qtAtualPessoas==8){
            BeepCurto=0;
        }
        vTaskEntrada(gpio, events);
    }
    else if(gpio_get(BOTAO_JOY)==0 && (current_time - last_time_JOY) > 200000)//200 ms de debounce como condição 
    {
        last_time_JOY = current_time; // Atualiza o tempo do último evento
        reset_usb_boot(0, 0);
    }
}

void vBuzzerTask(void *params)//alerta com Buzzer sonoro 
{
    //configurando PWM
    uint pwm_wrap = 1999;// definindo valor de wrap referente a 12 bits do ADC
    gpio_set_function(buzzer, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(buzzer);
    pwm_set_wrap(slice_num, pwm_wrap);
    pwm_set_clkdiv(slice_num, 75.0);//divisor de clock 
    pwm_set_enabled(slice_num, true);               // Ativa PWM
    while (true)
    {
        printf("BeepCurto %d",BeepCurto);//verificação mudança de estado no serial monitor
        if(BeepCurto==0) // para Beep com sala cheia
        {
            pwm_set_gpio_level(buzzer, 400); // 10% de Duty cycle
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(buzzer, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            pwm_set_gpio_level(buzzer, 400); // 10% de Duty cycle
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(buzzer, 0);
            BeepCurto=1; // muda estado
        }
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

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_JOY, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    //gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);


    // Cria semáforo de contagem (máximo 8, inicial 0)
    xContadorSem = xSemaphoreCreateCounting(8, 0);

    // Cria tarefa
    xTaskCreate(vContadorTask, "ContadorTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vLedTask, "TaskLEDsRGB", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "TaskBuzzer", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}
