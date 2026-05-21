try:
    from smbus2 import SMBus
except ImportError:
    print("Aviso: smbus2 não encontrado. Rodando em modo de simulação.")

class MPU6050Driver:
    def __init__(self, bus_num=1, address=0x68):
        self.bus_num = bus_num
        self.address = address
        self.sensor_ativo = False
        self.conectar()

    def conectar(self):
        try:
            self.bus = SMBus(self.bus_num)
            self.bus.write_byte_data(self.address, 0x6B, 0x00) # Acorda o MPU
            self.sensor_ativo = True
            print("Hardware: MPU6050 conectado com sucesso no I2C.")
        except Exception:
            self.sensor_ativo = False

    def _ler_word_raw(self, reg):
        high = self.bus.read_byte_data(self.address, reg)
        low = self.bus.read_byte_data(self.address, reg+1)
        value = (high << 8) + low
        return -((65535 - value) + 1) if value >= 0x8000 else value

    def ler_dados(self):
        if not self.sensor_ativo:
            return {"ax": 0.05, "ay": 0.0, "az": 1.0, "gx": 0.0, "gy": 0.0, "gz": 1.2} # Dados mockados para teste de mesa

        accel_scale = 16384.0 # transforma o acelerometo em 1 força G
        gyro_scale = 131.0

        return {
            "ax": self._ler_word_raw(0x3B) / accel_scale,
            "ay": self._ler_word_raw(0x3D) / accel_scale,
            "az": self._ler_word_raw(0x3F) / accel_scale,
            "gx": self._ler_word_raw(0x43) / gyro_scale,
            "gy": self._ler_word_raw(0x45) / gyro_scale,
            "gz": self._ler_word_raw(0x47) / gyro_scale
        }