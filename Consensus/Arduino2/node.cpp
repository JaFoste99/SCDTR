#include "node.h"
#include "math.h"

volatile bool Node::consensusCheck = false;
int Node::iter_consensus = 1;

/*Node::Node(){
    d[0] = 0; // Duty-cycle set by the local controller
    d[1] = 0;
  };*/


Node::Node(const float _m, const float _b, int _addr, float _c, float _rho) {

  addr = _addr;
  m = _m;
  b = _b;
  c = _c;
  Lcon = L;
  rho = _rho;
  o = 0;
  iter = 1;
  L_ref = 0;
  //d_out[0] = -1; d_out[1] = -1;
  d_best[0] = 0; d_avg[1] = 0;
  d[0] = 0; d[1] = 0;
}


typedef struct NodeInfo {
  float m;
  float b;
  float k1;
  float k2;
  float L;
  float d1;
  float d2;
} NodeInfo;

float Node::readIlluminance() {

  float v = readVoltage();
  /*Serial.println(v);
    Serial.println(m);
    Serial.println(b);*/
  float R2 = R1 * (Vcc - v) / v;
  float illuminance = pow(10, (log10(R2) - b) / m);
  return illuminance;
}

float Node::readVoltage() {

  return 5 * analogRead(sensorPin) / 1023.0;
}

bool Node::setPWM(int PWM) {

  if (PWM < 0 || PWM > 255)
    return false;

  analogWrite(ledPin, PWM);
  return true;
}


float Node::extIlluminance() {

  o = readIlluminance() - k[0]*d[0] - k[1]*d[1];
  return o;
}

NodeInfo* Node::getNodeInfo() {

  NodeInfo* n;
  n->m = m;
  n->b = b;
  n->L = L_ref;

  n->k1 = k[0];
  n->k2 = k[1];
  n->d1 = d[0];
  n->d2 = d[1];

  return n;
}

void Node::setLux(float _L) {
  L = _L;
}

bool Node::calib() {

  setPWM(0);

  if (iter > 2) return true;

  if (calib_flag) {

    msgSync();
    delay(500);

    o = readIlluminance();

    delay(1000);

    setPWM(255);
    delay(500);
    float x_max = readIlluminance();
    k[0] = (x_max - o) / 100;

    Serial.println(k[0]);

    delay(1000);

    setPWM(0);
    msgSync();
    delay(500);

    float x_max2 = readIlluminance();
    k[1] = (x_max2 - o) / 100;

    Serial.println(k[1]);

    calib_flag = !calib_flag;
    msgSync();
    delay(500);
    ++iter;
    calib();
  }
  else {

    msgSync();
    delay(500);
    
    msgSync();
    setPWM(255);

    calib_flag = !calib_flag;
    msgSync();
    delay(500);
    ++iter;
    calib();
  }
}

const char* Node::getConsensusData() {

  return consensus_data.c_str();
}

int Node::msgConsensus(char id, int src_addr, String data_str) {

  /*switch (id){
    case 1:
      Serial.println("Consensus Flag -> T");
      consensus_flag = true;
      consensus_data = data_str;
      break;

    case 'L':
      d_best[src_addr-1] = atof(data_str.c_str());
      consensus_init = false;
      break;

    default:
      return -1;
      break;

    }*/
}


bool Node::checkFeasibility(float d1, float d2) {

  float tol = 0.001;
  
  if (d1 < 0 - tol) {
    min_act = true;
    return false;
  }

  if (d1 > 100 + tol && !min_act) {
    max_act = true;
    return false;
  }

  if (k[0]*d1 + k[1]*d2 < L - o - tol) {
    max_act = false;
    return false;
  }

  return true;
}

float Node::getCost(float d1, float d2) {

  return c * d1 + y[0] * (d1 - d_avg[0]) + y[1] * (d2 - d_avg[1]) + rho / 2 * (pow(d1 - d_avg[0], 2) + pow(d2 - d_avg[1], 2));
}

void Node::checkSolution(float d1_test, float d2_test) {

  if (checkFeasibility(d1_test, d2_test)) {

    float cost = getCost(d1_test, d2_test);
    if (cost < cost_best) {

      cost_best = cost;
      d_best[0] = d1_test;
      d_best[1] = d2_test;
    }
  }
}

void Node::getCopy() {

  char d21_str[8];
  char d22_str[8];

  /*Serial.print("Received: ");
  Serial.println(consensus_data.c_str());*/

  String aux_str = consensus_data;
  char* token = strtok((char*)aux_str.c_str(), "/");

  if (token != NULL) strcpy(d21_str, token);
  token = strtok(NULL, "/");
  if (token != NULL) strcpy(d22_str, token);

  float d_aux[2];
  d_aux[0] = atof(d21_str);
  d_aux[1] = atof(d22_str);

  /*Serial.println(d_aux[0]);
    Serial.println(d_aux[1]);*/

  d_out[0] = d_aux[0];
  d_out[1] = d_aux[1];

  consensus_data = "";

}

void Node::sendCopy(float d1, float d2) {

  String str = floatToString(d2) + "/" + floatToString(d1);
  msgBroadcast(1,str);

  /*Serial.print("Sent: ");
  Serial.println(str.c_str());*/
}

void Node::initConsensus() {

  msgSync();
  //k[0] = 2; k[1] = 0.5;
  Lcon = L; // update lux reference
  iter_consensus = 1;
  y[0] = 0; y[1] = 0;
  cost_best = COST_BEST;
  d1_m = pow(k[0],2) + pow(k[1],2);
  d1_n = d1_m - pow(k[0],2);
  o = extIlluminance(); // Update external illuminance estimate
  Serial.println(o);

  if (consensus_flag) {
    delay(2);
    d[1] = atof(consensus_data.c_str()); // get ref value from the other node
    msgBroadcast(2,floatToString(Lcon/k[0]));
  } else {
    msgBroadcast(2,floatToString(Lcon/k[0]));
    delay(2);
    d[1] = atof(consensus_data.c_str());
  }

  //msgSync();
  
  d[0] = Lcon/k[0];
  d_avg[0] = (d[0] + d_out[0]) / 2;
  d_avg[1] = (d[1] + d_out[1]) / 2;
  /*Serial.println(d[0]);
  Serial.println(d[1]);*/
  //consensus_flag = true; // both nodes run consensus in parallel
  
}

void Node::consensusAlgorithm() {

  d_out[0] = -1;
  d_out[1] = -1;

  if(iter_consensus > 50) return;
  //Serial.println(iter_consensus);
  float rho_inv = 1.0 / rho;
  float d1, d2;
  //initConsensus(d_avg);  
  //unsigned long init = micros();
    
  //consensus_flag = false;
  //cost_best = 1000000; //large number

  float z1 = rho * d_avg[0] - c - y[0];
  float z2 = rho * d_avg[1] - y[1];

  // Unconstrained minimum
  d1 = rho_inv * z1;
  d2 = rho_inv * z2;
  checkSolution(d1,d2);

  // Solution in the DLB (Dimming lower bound)
  d1 = 0;
  d2 = rho_inv * z2;
  checkSolution(d1, d2);

  // Solution in the DUB (Dimming Upper Bound)
  d1 = 100;
  d2 = rho_inv * z2;
  checkSolution(d1, d2);

  // Solution in the ILB (Illuminance Lower Bound)
  d1 = rho_inv * z1 - k[0] * (o - Lcon + rho_inv * (k[0] * z1 + k[1] * z2)) / d1_m;
  d2 = rho_inv * z2 - k[1] * (o - Lcon + rho_inv * (k[0] * z1 + k[1] * z2)) / d1_m;
  checkSolution(d1, d2);

  // Solution in the ILB & DLB
  d1 = 0;
  d2 = rho_inv * z2 - (k[1] * (o - Lcon) + rho_inv * k[1] * k[1] * z2) / d1_n;
  checkSolution(d1, d2);

  // Solution in the ILB & DUB
  d1 = 100;
  d2 = rho_inv * z2 - (k[1] * (o - Lcon) + 100 * k[1] * k[0] + rho_inv * k[1] * k[1] * z2) / d1_n;
  checkSolution(d1, d2);

  if (max_act) { // if the illuminance request is above LED actuation, power everthing at max
    d_best[0] = 100;
    max_act = false;
  }

  /*if (min_act) { // if the illuminance request is below LED actuation
    d_best[0] = 0;
    min_act = false;
  }*/
   
  msgSync();
  sendCopy(d_best[0], d_best[1]);
  delay(50);
  getCopy();
  
  /*Serial.println(d_out[0]);
  Serial.println(d_out[1]);*/

  // Average solutions from all nodes
  d_avg[0] = (d_best[0] + d_out[0]) / 2;
  d_avg[1] = (d_best[1] + d_out[1]) / 2;

  // Dual update -> Update the Lagrangian Multipliers
  y[0] += rho * (d_best[0] - d_avg[0]);
  y[1] += rho * (d_best[1] - d_avg[1]);
  
  L_ref = k[0]*d_best[0]; // Value to be sent to the local controller
  Serial.println(L_ref);
  ++iter_consensus;

  consensusCheck = true;
  /*unsigned long finish = micros() - init;
  Serial.println(finish);*/
  //Lcon = L;*/
}

float Node::Windup(float u) {

  int wdp = 0;

  if (u > 100)
    wdp = 100 - u;
  else if (u < 0)
    wdp = -u;
  else
    wdp = 0;

  return wdp;
}

void Node::PID() {

  float y = readIlluminance();
  //Serial.print(y);
  //Serial.print("\t");
  //Serial.println(des_brightness);
  //Serial.print("\t");
  float e = des_brightness - y;           // error in LUX
  //Serial.print(e);
  //Serial.print("\t");
  float p = k1 * e;                       // porpotional term
  float i = i_ant + k2 * (e + e_ant) + kwdp * Windup(u);    // integal term
  //float i = i_ant + (e    + e_ant);     // integal term
  if (abs(e) < 0.5)
    p = 0;

  float u = p + i + des_brightness / k[0];     // add feed-forward term

  u = constrain(u, 0, 100);

  setPWM(map(u, 0, 100, 0, 255));
  //Serial.println(u);
  i_ant = i;
  e_ant = e;
  y_ant = y;
}

void Node::init_PID(float ku, float T) {

  k1 = ku * 0.45;
  ki = 1.2 / T;
  k2 = k1 * ki * T / 2;

  i_ant = 0, e_ant = 0, y_ant = 0;//, usat = 0;
}

void Node::set_Brightness() {
  if (consensusCheck) {
    des_brightness = L_ref;
    //Serial.print("VALOR VINDO DO CONSENSUS = ");
  }
  else {
    des_brightness = L;
    //Serial.print("VALOR VINDO DO USER = ");
  }
  //Serial.println(des_brightness);
}


void Node::setupint_1() {

  const int freq_des = 100; //   set freq = 100Hz

  cli();  //stop interrupts
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1 = 0; //initialize counter value to 0

  OCR1A = 2499;  // (must be <65536) (16*10^6) / (freq_des*64) - 1
  TCCR1B |= (1 << WGM12); // turn on CTC mode
  TCCR1B |= (1 << CS11) | (1 << CS10); // Set prescaler
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt

  sei(); //allow interrupts*/
}