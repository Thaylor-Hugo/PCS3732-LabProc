# Aula 10 — Fechadura Eletrônica (Raspberry Pi 3 + Freenove FNK0054)

Implementação da missão técnica descrita em
`Electronic_Lock_Engineering_Mission.pdf`: fechadura com entrada de senha
via teclado matricial, feedback visual em display LCD (I2C), feedback
sonoro via buzzer e verificação de integridade física via sensor
ultrassônico.

## Hardware necessário

- Raspberry Pi 3 (kit Freenove FNK0054)
- Teclado matricial 4x4
- Display LCD1602 com backpack I2C (PCF8574, endereço padrão `0x27`)
- Sensor ultrassônico HC-SR04
- Buzzer ativo
- Resistores para divisor de tensão no pino ECHO do HC-SR04 (5V -> 3.3V)

## Ligações (BCM), ver `config.py`

| Componente          | Pino(s) BCM                          |
|---------------------|---------------------------------------|
| Teclado — linhas    | 5, 6, 13, 19                          |
| Teclado — colunas   | 26, 21, 20, 16                        |
| LCD1602 (I2C)       | SDA=GPIO2, SCL=GPIO3 (barramento 1)   |
| Sensor — TRIG        | 23                                    |
| Sensor — ECHO        | 24 (com divisor resistivo 5V->3.3V)   |
| Buzzer              | 25                                    |

Se a sua fiação usar outros pinos, ajuste apenas `config.py` — nenhum
outro arquivo precisa ser alterado.

## 1. Preparar o Raspberry Pi

```bash
# Habilitar o barramento I2C
sudo raspi-config
# -> Interface Options -> I2C -> Enable -> Finish -> Reboot

# Confirmar que o LCD foi detectado no endereço 0x27 (ajuste se diferente)
sudo apt install -y i2c-tools
i2cdetect -y 1
```

## 2. Instalar as dependências Python

```bash
cd aula10
python3 -m venv .venv          # opcional, mas recomendado
source .venv/bin/activate
pip install -r requirements.txt
```

`gpiozero` já acompanha o Raspberry Pi OS por padrão; `smbus2` é usado
pelo driver do LCD (`lcd_i2c.py`).

## 3. Testar cada componente isoladamente

Sempre valide um componente por vez antes de rodar a integração completa
("Regra de Ouro" do PDF: nunca integrar algo que não passou no teste
unitário). Interrompa cada teste com `Ctrl+C`.

```bash
python3 keypad_test.py    # pressione teclas; cada uma deve gerar 1 evento
python3 lcd_test.py       # deve exibir "Hello World" nas 2 linhas por 5s
python3 sensor_test.py    # aproxime/afaste um objeto e observe TRANCADA/ABERTA
python3 buzzer_test.py    # 1 bipe curto (sucesso) + 1 bipe longo (falha)
```

Veja `plano_integracao.md` e `plano_depuracao.md` para o roteiro completo
de integração incremental e o funil de depuração em 3 camadas
(física -> sistema/driver -> lógica) caso algum teste falhe.

## 4. Rodar a fechadura — versão normal

```bash
python3 electronic_lock.py
```

- Senha padrão: `1234` (`DEFAULT_PASSWORD` em `config.py`, comparada em
  texto plano — versão didática, sem hashing).
- No teclado: dígitos `0-9` compõem a senha, `*` apaga o último dígito,
  `#` confirma.
- Sucesso: bipe curto, LCD mostra "Aberto" e retranca automaticamente
  após `AUTO_RELOCK_S` (5s por padrão).
- Falha: bipe longo, LCD mostra "Acesso Negado". Após
  `MAX_FAILED_ATTEMPTS` (3) falhas seguidas, o sistema entra em cooldown
  temporário (`COOLDOWN_S`, 10s) sem travar o processo.
- Se o sensor detectar a lingueta ausente enquanto o sistema acredita
  estar trancado (RF3 — abertura forçada), o LCD/buzzer disparam um
  alerta; pressione `#` com a porta fisicamente fechada para reconhecer.

## 5. Rodar a fechadura — versão desafio (segurança)

```bash
python3 electronic_lock_desafio.py
```

Mesmo fluxo de uso da versão normal, mas com as mitigações de segurança do
item 6 (Desafio) do enunciado:

- Senha armazenada e comparada como hash SHA-256 + salt
  (`hashlib.sha256`), com comparação em tempo constante
  (`hmac.compare_digest`) para mitigar timing attacks.
- Verificação de plausibilidade das leituras do sensor (fora da faixa
  física do HC-SR04, ou estática por tempo demais) para detectar
  tentativas de spoofing (ex.: jumper/ímã forçando o pino ECHO).
- Cooldown com backoff exponencial: cada novo bloqueio dobra a duração do
  anterior.
- Log de eventos com encadeamento de hash (`security_log.jsonl`, criado
  automaticamente na primeira execução) — qualquer edição retroativa do
  arquivo quebra a cadeia (`SecurityLog.verify_chain()`, chamado a cada
  boot e reportado no console se a cadeia estiver inconsistente).

## Estrutura dos arquivos

```
config.py                  # pinagem e parâmetros (único ponto de ajuste de hardware)
lcd_i2c.py                 # driver do LCD1602 via PCF8574/I2C (smbus2)
keypad.py                  # varredura do teclado matricial com debounce
nonblocking_buzzer.py       # buzzer temporizado sem bloquear o loop principal
keypad_test.py / lcd_test.py / sensor_test.py / buzzer_test.py
                            # testes isolados por componente
electronic_lock.py          # versão normal (RF1-RF3, RNF1)
electronic_lock_desafio.py  # versão desafio (hash de senha, anti-spoofing, log encadeado)
plano_integracao.md        # roteiro de integração incremental
plano_depuracao.md         # funil de depuração em 3 camadas
requirements.txt           # dependências Python (gpiozero, smbus2)
```
