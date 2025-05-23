
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
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

#define BOTAO_A 5 
#define BOTAO_B 6 
#define BOTAO_JOY 22
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

// Tarefa que consome eventos e mostra no display
void vContadorTask(void *params)
{
    char buffer[32];

    // Tela inicial
    ssd1306_fill(&ssd, 0);
    ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
    ssd1306_draw_string(&ssd, "  evento...", 5, 34);
    ssd1306_send_data(&ssd);

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
            vTaskDelay(pdMS_TO_TICKS(1500));

        //}
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
        vTaskEntrada(gpio, events);
    }
    else if(gpio_get(BOTAO_JOY)==0 && (current_time - last_time_JOY) > 200000)//200 ms de debounce como condição 
    {
        last_time_JOY = current_time; // Atualiza o tempo do último evento
        reset_usb_boot(0, 0);
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


    // Cria semáforo de contagem (máximo 10, inicial 0)
    xContadorSem = xSemaphoreCreateCounting(10, 0);

    // Cria tarefa
    xTaskCreate(vContadorTask, "ContadorTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    vTaskStartScheduler();
    panic_unsupported();
}
