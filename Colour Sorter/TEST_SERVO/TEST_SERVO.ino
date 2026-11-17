#define LED_1_PIN 2
#define LED_2_PIN 3
#define BUTTON_PIN 4

unsigned long last_led_1

void update_led_1();
void update_led_2();

void setup(){
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}
void loop(){
  update
}