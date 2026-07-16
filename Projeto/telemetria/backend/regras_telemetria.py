import math

class AnalisadorTelemetria:
    def avaliar_status(self, raw, angulos):
        ax, ay, az = raw["ax"], raw["ay"], raw["az"]
        gx, gy, gz = raw["gx"], raw["gy"], raw["gz"]
        pitch, roll = angulos["pitch"], angulos["roll"]

        accel_mag = math.sqrt(ax**2 + ay**2 + az**2)
        gyro_mag = math.sqrt(gx**2 + gy**2 + gz**2)

        # 1. Detecta se está em movimento (Threshold de Variância)
        estado_atual = "em_movimento" if abs(accel_mag - 1.0) > 0.05 else "parado"

        # 2. Detecta Acidente (Alta Força G + Baixa Rotação + Robô Tombado)
        houve_acidente = False
        if accel_mag > 2.5 and gyro_mag < 20.0 and (abs(pitch) > 60 or abs(roll) > 60):
            houve_acidente = True
            estado_atual = "acidente_detectado"

        return estado_atual, houve_acidente