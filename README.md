# 🔥 Sistema IoT de Detecção e Contenção de Vazamento de Gás (ESP32 + MQTT)

Projeto desenvolvido para a disciplina de IoT/Sistemas Embarcados com foco no monitoramento em tempo real e prevenção de acidentes domésticos em cozinhas. O sistema detecta concentrações de gás inflamável (GLP/gás natural), aciona alertas locais escalonados, realiza o corte automático do suprimento por meio de uma válvula servo-acionada e comunica-se de forma bidirecional com um painel web via protocolo **MQTT**.

🔗 **Simulação Online no Wokwi:** [Acesse o projeto interativo aqui]([COLOQUE_AQUI_O_LINK_DO_WOKWI])

---

## ⚙️ Funcionalidades e Lógica de Operação

O sistema opera com programação não-bloqueante (`millis()`) dividida em três níveis de segurança:

1. **🟢 Nível `SEGURO` (< 35%):**
   - LEDs apagados, buzzer em silêncio e exaustor desligado.
   - Válvula de gás permanece aberta (`0°`), exceto se houver sido fechada por uma emergência anterior (exigindo rearme manual ou remoto).
2. **🟡 Nível `ATENCAO` (35% a 69%):**
   - **Alerta Visual:** LED vermelho pisca em intervalos de 500 ms.
   - **Alerta Sonoro Discreto:** Buzzer emite um pulso grave (`380 Hz` por `60 ms`) a cada 2,5 segundos para avisar sobre o início de vazamento sem causar pânico.
   - **Mitigação:** LED azul é acionado, simulando a ligação automática do exaustor da cozinha para dispersar o gás.
3. **🔴 Nível `PERIGO` (≥ 70%):**
   - **Corte Automático:** Servo motor gira para `90°`, fechando a válvula de gás e bloqueando o fluxo.
   - **Alerta Crítico:** LED vermelho fica aceso fixo, exaustor permanece ligado e o buzzer dispara uma sirene bitonal contínua (`1800 Hz` ↔ `2400 Hz`).
   - **Trava de Segurança (Intertravamento):** Mesmo após a dispersão do gás, a válvula permanece fechada até que o operador pressione o **botão físico de rearme** ou envie o comando remoto via MQTT.

---

## 🛠️ Lista de Componentes Utilizados

| Componente | Quantidade | Pino ESP32 | Função no Sistema |
| :--- | :---: | :---: | :--- |
| **ESP32 DevKit v1** | 1 | — | Microcontrolador principal com Wi-Fi integrado |
| **Sensor de Gás MQ-2** | 1 | `D34` (ADC1) | Leitura analógica da concentração de gás (GLP/Propano) |
| **Display OLED 0.96" SSD1306** | 1 | `D21` (SDA) / `D22` (SCL) | Painel local com percentual, status, estado da válvula e barra gráfica |
| **Servo Motor (Micro Servo)** | 1 | `D18` (PWM) | Simula a válvula solenoide de corte do gás (`0°` Aberta / `90°` Fechada) |
| **Buzzer Piezoelétrico** | 1 | `D25` | Emissão de alertas sonoros diferenciados para Atenção e Perigo |
| **LED Vermelho** | 1 | `D26` | Sinalização visual de alerta e emergência |
| **LED Azul** | 1 | `D27` | Simula o acionamento automático do exaustor de ventilação |
| **Resistores 220Ω** | 2 | — | Limitação de corrente para proteção dos LEDs |
| **Pushbutton (Botão Verde)** | 1 | `D14` (`INPUT_PULLUP`) | Rearme manual do sistema e abertura da válvula após normalização |

---

## 📡 Arquitetura MQTT (Comunicação Bidirecional)

* **Broker Utilizado:** `broker.hivemq.com` (Porta TCP `1883` no ESP32 / WebSocket SSL `8884` no cliente Web).
* **Tópico de Telemetria (Publish — ESP32 ➔ Dashboard):**
  * Tópico: `projeto/cozinha/detector_gas`
  * Frequência: A cada 2 segundos.
  * Exemplo de payload JSON enviado:
    ```json
    {
      "gas_pct": 78,
      "status": "PERIGO",
      "valvula": "FECHADA",
      "exaustor": true
    }
    ```
* **Tópico de Comandos Remotos (Subscribe — Dashboard ➔ ESP32):**
  * Tópico: `projeto/cozinha/comando`
  * Comandos aceitos:
    * `FECHAR` — Fecha imediatamente a válvula de gás (`90°`) remotamente.
    * `REARMAR` — Reabre a válvula (`0°`) e reseta alertas (permitido apenas se o nível de gás estiver abaixo de `35%`).
    * `SILENCIAR` — Interrompe o alerta sonoro do buzzer mantendo os bloqueios visuais e mecânicos ativos.

---

## 📚 Bibliotecas Utilizadas

* `PubSubClient` (Comunicação cliente MQTT)
* `ESP32Servo` (Controle PWM do servo motor no ESP32)
* `Adafruit SSD1306` (Controle do display OLED I2C)
* `Adafruit GFX Library` (Renderização gráfica e textos no OLED)

---

## 🚀 Como Executar no Wokwi

1. Acesse o link da simulação no topo deste documento (ou importe os arquivos `sketch.ino`, `diagram.json` e `libraries.txt` em um novo projeto ESP32 no [Wokwi](https://wokwi.com)).
2. Abra o [HiveMQ WebSocket Client](https://www.hivemq.com/demos/websocket-client/), clique em **Connect** e inscreva-se (**Subscribe**) no tópico `projeto/cozinha/detector_gas`.
3. Inicie a simulação no Wokwi (**Play**), clique sobre o sensor **MQ-2** e deslize o controle de **PPM** para testar os três níveis de segurança e o fechamento automático da válvula.
