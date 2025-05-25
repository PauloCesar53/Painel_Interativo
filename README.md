# Painel_Interativo 
Repositório criado para versionamento da atividade sobre FreeRTOS (semáforo e mutex) da residência em software embarcado, com implementação de um sistema de controle de acesso em um laboratório químico 


## Descrição geral do Funcionamento do programa 
No Display da BitDogLab é informado a quantidade atual de pessoas no ambiente e a capacidade máxima, como também mensagens de ocupação do ambiente (vazia/ocupada/cheia). O botão do joystick reseta o contador para zero pessoas e um beep duplo é emitido. O botão A incrementa o contador (aumenta quantidade pessoas no ambiente), já o botão B decrementa o contador (diminui quantidade pessoas no ambiente). Ao tentar entrar no ambiente com a sala cheia, um beep único é emitido pelo Buzzer. Os LEDs RGB mudam a cor a depender da quantidade de pessoas no ambiente (0 mostra cor azul, entre 1 e 6 cor verde, 8 cor vermelha). A matriz de LEDs 5x5 mostra o número de pessoas no ambiente com a cor idêntica ao critério para os LEDs RGB.

## Descrição detalhada do Funcionamento do programa  na BitDogLab
**Botões→** O botão A incrementa a quantidade de pessoas no ambiente, O botão B decrementa a quantidade de pessoas no ambiente. O botão do Joystick zera a quantidade pessoas do ambiente.

**LED RGB→** Cor a depender da quantidade de pessoas no ambiente (critério na descrição funcional).

**Display→** Mostra informações da quantidade de pessoas no ambiente e mensagem sobre ocupação. 

**Matriz de LEDs 5X5→** Mostra número de pessoas no ambiente com critério de cor semelhante aos LEDs RGB.

**Buzzer→** Emite beep curto ao tentar entrar no ambiente com a sala cheia e  emite beep duplo ao tentar resetar o sistema (zerar quantidade de pessoas). 


## Compilação e Execução
**OBS: É preciso ter o FreeRTOS baixado no computador, sendo necessário alterar a linha  do CMakeLists.txt com o diretório correspondente ao arquivo**


1. Certifique-se de que o SDK do Raspberry Pi Pico está configurado no seu ambiente.
2. Compile o programa utilizando a extensão **Raspberry Pi Pico Project** no VS Code:
   - Abra o projeto no VS Code, na pasta PRPJETO_MULTIRAREFAS tem os arquivos necessários para importar 
   o projeto com a extensão **Raspberry Pi Pico Project**.
   - Vá até a extensão do **Raspberry pi pico project** e após importar (escolher sdk de sua escolha) os projetos  clique em **Compile Project**.
3. Coloque a placa em modo BOOTSEL e copie o arquivo `Painel_Interativo.uf2`  que está na pasta build, para a BitDogLab conectado via USB.

**OBS: Devem importar o projeto para gerar a pasta build, pois a mesma não foi inserida no repositório**

## Colaboradores
- [PauloCesar53 - Paulo César de Jesus Di Lauro ] (https://github.com/PauloCesar53)
