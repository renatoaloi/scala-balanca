

#include "HX711.h"
#include <SoftwareSerial.h>

#define DOUT  A0
#define CLK  A1

#define QTDE_TEMPO 5000
#define QTDE_TEMPO_CICLO 60000
#define PESO_MINIMO 0.01
#define PESOS_SIZE 20
#define PORCENTO_CORTE 0.8

HX711 scale;

unsigned long contaTempo = 0;
unsigned long contaTempoCiclo = 0;
float units = 0.0;
int contaPesos = 0;
float pesos[PESOS_SIZE];
float muitoDiferenteValor = 0.0; // variável de referencia do cálculo dos 80%

float calibration_factor = -279560.00; //-7050 trabalhei para minha configuração de escala máxima de 440lb

void reiniciaPesos() {
  for (int i = 0; i < PESOS_SIZE; i++) {
    pesos[i] = 0.0;
  }
}

SoftwareSerial swSerial(10, 11); // RX, TX

void setup() {
  Serial.begin(9600);
  Serial.println("Esboço de calibração HX711");
  Serial.println("Remova todo o peso da escala");
  Serial.println("Após as leituras começarem, coloque o peso conhecido em escala");
  Serial.println("Pressione + ou a para aumentar o fator de calibração");
  Serial.println("Pressione - ou z para diminuir o fator de calibração");

  scale.begin(DOUT, CLK);
  scale.set_scale();
  scale.tare(); //Repor a escala para 0
  delay (500);

  long zero_factor = scale.read_average(); //Obter uma leitura de linha de base
  Serial.print("Zero factor: "); //Isso pode ser usado para eliminar a necessidade de tarar a balança. Útil em projetos de escala permanente.
  Serial.println(zero_factor);

  reiniciaPesos();
}

void loop() {

  scale.set_scale(calibration_factor); //Ajuste para este fator de calibração
  units = scale.get_units();
  delay (500);

  Serial.print("Leituras: ");
  Serial.print(units, 3);
  Serial.print(" Kg"); //Altere isso para kg e reajuste o fator de calibração se você seguir as unidades 
  Serial.print(" calibration_factor: ");
  Serial.print(calibration_factor);
  Serial.println();

  if(Serial.available())
  {
    char temp = Serial.read();
    if(temp == '+' || temp == 'a')
      calibration_factor += 10;
    else if(temp == '-' || temp == 'z')
      calibration_factor -= 10;
  }

  // Registra os pesos a cada 5 segundos
  if (contaTempo < millis()) {
    if (units > PESO_MINIMO) {
      // se peso for maior que peso mínimo, então registra
      if (contaPesos < PESOS_SIZE - 1) {
        pesos[contaPesos++] = units;
      } else Serial.print("Estouro de array!");
    }
    contaTempo = millis() + QTDE_TEMPO;
  }

  // Executa a média a cada minuto
  if (contaTempoCiclo < millis()) {

    // desconsiderando o menor valor
    float menorValor = 9999.0;
    int idxMenorValor = -1;
    for (int i = 0; i < PESOS_SIZE; i++) {
      if (pesos[i] < menorValor) {
        menorValor = pesos[i];
        idxMenorValor = i;
      }
    }

    // desconsiderando o maior valor
    float maiorValor = 0.0;
    int idxMaiorValor = -1;
    for (int i = 0; i < PESOS_SIZE; i++) {
      if (pesos[i] > maiorValor) {
        maiorValor = pesos[i];
        idxMaiorValor = i;
      }
    }

    // descartando valores maiores que 80% que o valor de referencia
    int idxMuitoDiferenteValor[PESOS_SIZE];
    int contaMuitoDiferenteValor = 0;
    memset(idxMuitoDiferenteValor, -1, PESOS_SIZE);
    for (int i = 0; i < PESOS_SIZE; i++) {
      if (muitoDiferenteValor == 0.0 && i != menorValor && i != maiorValor && pesos[i] > PESO_MINIMO) {
         muitoDiferenteValor = pesos[i];
      }
      else if ((muitoDiferenteValor * (1 + PORCENTO_CORTE)) < pesos[i]) {
        if (contaMuitoDiferenteValor < (PESOS_SIZE - 1)) {
          idxMuitoDiferenteValor[contaMuitoDiferenteValor++] = i;
        }
      }
    }

    // Computando a media
    float soma = 0.0;
    int qtde = 0;
    for (int i = 0; i < PESOS_SIZE; i++) {

      // Verificando por valores muito diferentes
      bool mustContinue = false;
      for (int j = 0; j < contaMuitoDiferenteValor; j++) {
        if (idxMuitoDiferenteValor[j] == i) {
          mustContinue = true;
          break;
        }
      }
      if (mustContinue) continue;
      
      if (i != menorValor && i != maiorValor && pesos[i] > PESO_MINIMO) {
         soma += pesos[i];
         qtde++;
      }
    }

    // valores usados para computar a média
    Serial.print("Valores utilizados para computar a média: ");
    for (int i = 0; i < PESOS_SIZE; i++) {
      if (i != menorValor && i != maiorValor && pesos[i] > PESO_MINIMO) {
         Serial.print(pesos[i], 3);
         Serial.print(", ");
      }
    }
    Serial.println();

    // Media final
    float media = 0.0;

    if (qtde > 0) {
      media = (float)(soma / qtde);
    }

    Serial.print("Media de 1 minuto: ");
    Serial.println(media, 3);

    // Envia para o ESP07
    swSerial.println(media, 3);
    
    contaPesos = 0;
    reiniciaPesos();
    contaTempoCiclo = millis() + QTDE_TEMPO_CICLO;
    contaTempo = millis() + QTDE_TEMPO;
  }
}
