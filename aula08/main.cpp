#include <iostream>
#include <string>
#include <cmath>
#include <chrono>
#include <cstdlib>

// Estrutura para encapsular os resultados do benchmark local
struct CalcResult {
    int resultado;
    bool overflow;
    long long timeSpentMicros;
};

// Converte string binária em inteiro interpretando o complemento de dois
int binaryToDecimal(const std::string& bin, int bits) {
    int unsignedValue = std::stol(bin, nullptr, 2);
    int signMask = 1 << (bits - 1);
    if ((unsignedValue & signMask) != 0) {
        return unsignedValue - (1 << bits);
    }
    return unsignedValue;
}

// Converte o decimal de volta para string binária formatada com N bits
std::string decimalToBinaryString(int value, int bits) {
    std::string s = "";
    int mask = (1 << bits) - 1;
    int maskedVal = value & mask;
    for (int i = bits - 1; i >= 0; i--) {
        s += ((maskedVal >> i) & 1) ? "1" : "0";
    }
    return s;
}

CalcResult executarCalculo(std::string binA, std::string binB, std::string op, int bits) {
    CalcResult res;
    res.overflow = false;
    res.resultado = 0;

    int mask = (bits >= 32) ? -1 : ((1 << bits) - 1);
    int signMask = 1 << (bits - 1);
    int minSigned = -(1 << (bits - 1));
    int maxSigned = (1 << (bits - 1)) - 1;

    int valA = binaryToDecimal(binA, bits) & mask;
    int valB = (op != "fat") ? (binaryToDecimal(binB, bits) & mask) : 0;

    int sA = (valA & signMask) ? (valA - (1 << bits)) : valA;
    int sB = (valB & signMask) ? (valB - (1 << bits)) : valB;

    long long full = 0;

    // Início do Benchmark de Tempo de Alta Precisão (Nativo ARM)
    auto start = std::chrono::high_resolution_clock::now();

    if (op == "add") {
        full = (long long)sA + sB;
        if (full < minSigned || full > maxSigned) res.overflow = true;
        res.resultado = ((int)full) & mask;
    } 
    else if (op == "sub") {
        full = (long long)sA - sB;
        if (full < minSigned || full > maxSigned) res.overflow = true;
        res.resultado = ((int)full) & mask;
    } 
    else if (op == "mult") {
        full = (long long)sA * sB;
        if (full < minSigned || full > maxSigned) res.overflow = true;
        res.resultado = ((int)full) & mask;
    } 
    else if (op == "div") {
        if (sB == 0) {
            res.overflow = true; // Tratamento de Exceção: Divisão por zero
            res.resultado = 0;
        } else {
            full = (long long)sA / sB;
            if (full < minSigned || full > maxSigned) res.overflow = true;
            res.resultado = ((int)full) & mask;
        }
    } 
    else if (op == "fat") {
        if (sA < 0) {
            res.overflow = true;
            res.resultado = 0;
        } else {
            long long acc = 1;
            for (int i = 1; i <= sA; i++) {
                acc *= i;
                if (acc < minSigned || acc > maxSigned) res.overflow = true;
            }
            res.resultado = ((int)acc) & mask;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    res.timeSpentMicros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return res;
}

int main() {
    std::string binA, binB, op;
    int bits;

    std::cout << "=== CALCULADORA BINÁRIA LOCAL (RASPBERRY PI 3 - ARM) ===" << std::endl;
    std::cout << "Digite o número de bits (2 a 16, ou mais para teste de escalabilidade): ";
    std::cin >> bits;

    std::cout << "Digite o Operando A (em binário): ";
    std::cin >> binA;

    std::cout << "Digite a operação (add, sub, mult, div, fat): ";
    std::cin >> op;

    if (op != "fat") {
        std::cout << "Digite o Operando B (em binário): ";
        std::cin >> binB;
    }

    CalcResult r = executarCalculo(binA, binB, op, bits);

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "Resultado Binário: " << decimalToBinaryString(r.resultado, bits) << std::endl;
    std::cout << "Resultado Decimal: " << binaryToDecimal(decimalToBinaryString(r.resultado, bits), bits) << std::endl;
    std::cout << "Overflow detectado: " << (r.overflow ? "SIM" : "NÃO") << std::endl;
    std::cout << "Tempo de Execução no Core ARM: " << r.timeSpentMicros << " us" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return 0;
}