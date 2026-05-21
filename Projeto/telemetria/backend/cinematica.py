import math

class MotorCinematica:
    def __init__(self):
        self.pitch = 0.0
        self.roll = 0.0
        self.yaw = 0.0
        self.yaw_total = 0.0
        
        self.vel_x = 0.0
        self.vel_max = 0.0
        self.soma_vel = 0.0
        self.ciclos = 0

        self.voltas_horarias = 0
        self.voltas_anti_horarias = 0

    def calcular_angulos(self, raw, dt):
        ax, ay, az = raw["ax"], raw["ay"], raw["az"]
        gx, gy, gz = raw["gx"], raw["gy"], raw["gz"]

        # Acelerômetro
        pitch_acc = math.degrees(math.atan2(ay, az))
        roll_acc = math.degrees(math.atan2(-ax, math.sqrt(ay*ay + az*az)))

        # Filtro Complementar
        self.pitch = 0.98 * (self.pitch + gx * dt) + 0.02 * pitch_acc
        self.roll = 0.98 * (self.roll + gy * dt) + 0.02 * roll_acc
        
        # Integração do Yaw
        self.yaw += gz * dt
        self.yaw_total += gz * dt

        # Quando o total acumula 360 graus, soma uma volta e subtrai do acumulador
        if self.yaw_total >= 360.0:
            self.voltas_horarias += 1
            self.yaw_total -= 360.0
        elif self.yaw_total <= -360.0:
            self.voltas_antihorarias += 1
            self.yaw_total += 360.0

        return {"pitch": self.pitch, "roll": self.roll, "yaw": self.yaw % 360}

    def calcular_velocidade_e_voltas(self, raw, dt, estado_movimento):
        ax = raw["ax"]
        
        # Remove a gravidade do eixo X baseada na inclinação
        ax_linear_g = ax - math.sin(math.radians(self.pitch))
        ax_linear_ms2 = ax_linear_g * 9.81 

        # Integração (com reset se estiver fisicamente parado)
        if estado_movimento == "parado":
            self.vel_x = 0.0
        else:
            self.vel_x += ax_linear_ms2 * dt

        # Estatísticas
        vel_abs = abs(self.vel_x)
        if vel_abs > self.vel_max: self.vel_max = vel_abs
        
        self.soma_vel += vel_abs
        self.ciclos += 1

        return {
            "voltas_completas": int(self.yaw_total / 360.0),
            "voltas_horarias": self.voltas_horarias,
            "voltas_anti_horarias": self.voltas_anti_horarias,
            "vel_atual_ms": round(self.vel_x, 3),
            "vel_max_ms": round(self.vel_max, 3),
            "vel_media_ms": round(self.soma_vel / self.ciclos, 3)
        }