from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import asyncio
import json
import time

# Importando a nossa arquitetura
from driver_mpu import MPU6050Driver
from cinematica import MotorCinematica
from regras_telemetria import AnalisadorTelemetria

app = FastAPI()
driver = MPU6050Driver()
cinematica = MotorCinematica()
analisador = AnalisadorTelemetria()

class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast(self, message: str):
        for connection in self.active_connections:
            await connection.send_text(message)

manager = ConnectionManager()

@app.websocket("/ws/telemetria")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    last_time = time.time()
    
    try:
        while True:
            # Controle de tempo (dt)
            current_time = time.time()
            dt = current_time - last_time
            last_time = current_time
            if dt > 0.5: dt = 0.01 # Trava de segurança para o primeiro loop

            # 1. LER HARDWARE
            raw_data = driver.ler_dados()
            
            # 2. CALCULAR ÂNGULOS
            angulos = cinematica.calcular_angulos(raw_data, dt)
            
            # 3. VERIFICAR REGRAS DE NEGÓCIO (Acidentes e Movimento)
            status, acidente = analisador.avaliar_status(raw_data, angulos)
            
            # 4. CALCULAR VELOCIDADE E VOLTAS
            metricas = cinematica.calcular_velocidade_e_voltas(raw_data, dt, status)
            
            # 5. MONTAR O CONTRATO DE DADOS (JSON)
            pacote = {
                "timestamp": current_time,
                "status": status,
                "acidente": acidente,
                "raw_data": {k: round(v, 3) for k, v in raw_data.items()},
                "processed_data": {
                    "pitch": round(angulos["pitch"], 2),
                    "roll": round(angulos["roll"], 2),
                    "yaw": round(angulos["yaw"], 2),
                    **metricas
                }
            }
            
            # 6. TRANSMITIR PARA O FRONTEND
            await manager.broadcast(json.dumps(pacote))
            await asyncio.sleep(0.05) # 20 Hz
            
    except WebSocketDisconnect:
        manager.disconnect(websocket)