#define LED_1_PIN 2
#define LED_2_PIN 3
#define BUTTON_PIN 4

void update_led_1();
void update_led_2();

enum class LED_STATE{
  FIRST_STATE,
  SECOND_STATE
};

struct LED_INFORMATION{
  unsigned long  delay_time;
  bool reading;
  unsigned long last_update = 0;
  unsigned long now;
  LED_STATE LED_state;
};

class UPDATE_LED{
  public:
    LED_INFORMATION LED_1;
    LED_INFORMATION LED_2;
    void read(){
      LED_1.reading = digitalRead(LED_1_PIN);
      LED_2.reading = digitalRead(LED_2_PIN);
    }
    void bink(){
      read();
      if(!LED_1.reading && millis() - last_update >= LED_1.delay_time){
        digitalWrite(LED_1_PIN, HIGH);
        LED_1.last_update = millis();
      }
      if(LED_1.reading && millis() - last_update >= LED_1.delay_time){
        digitalWrite(LED_1_PIN, LOW);
        LED_1.last_update = millis();
      }
      if(!LED_2.reading && millis() - last_update >= LED_2.delay_time){
        digitalWrite(LED_2_PIN, HIGH);
        LED_2.last_update = millis();
      }
      if(LED_2.reading && millis() - last_update >= LED_2.delay_time){
        digitalWrite(LED_2_PIN, LOW);
        LED_2.last_update = millis();
      }
    }
    void update(){
      switch(LED_1.LED_state){
        case LED_STATE::FIRST_STATE:
          LED_1.delay_time = 250;
          LED_2.delay_time = 700;
          blink();
          break;
        case LED_STATE::SECOND_STATE:
          LED_1.delay_time = 700;
          LED_2.delay_time = 250;
          blink();
          break;
        default:
          return;
      }
    }
};

enum class BUTTON_STATE{
  IDLE,
  DEBOUNCING,
  DEBOUNCED
};
struct BUTTON_INFORMATION{
  bool last_button_read = HIGH;
  bool button_read;
  unsigned long debounce_time = 30ms;
  unsigned long begin_debounce = 0;
  bool debouncing = false;
  BUTTON_STATE button_state = BUTTON_STATE::IDLE;
};
class UPDATE_BUTTON{
  private:
    BUTTON_INFORMATION button;
  public:
    LED_STATE& led_1_state = LED_1;
    LED_STATE& led_1_state = LED_2;
    void read(){
      button.button_read = digitalRead(BUTTON_PIN);
    }
    void update(){
      read();
      switch(button.button_state){
        case BUTTON_STATE::IDLE
          if(button.button_read != button.last_button_read && !button.button_read){
            button.button_state = BUTTON_STATE::DEBOUNCING:
            if(!button.debouncing){
              button.begin_debounce = millis();
              button.debouncing = true;
            }
          }
        case BUTTON_STATE::DEBOUNCING:
            if(millis() - button.begin_debounce >= button.debounce_time){
              button.debouncing = false;
              button.last_button_read = button.button_read;
              button.button_state = BUTTON_STATE::DEBOUNCED;
            }
        case BUTTON_STATE::DEBOUNCED:
          if(button.button_read != button.last_button_read && button.button_read){
            if(led_1_state = LED_STATE::FIRST_STATE){
              led_1_state = LED_STATE::SECOND_STATE;
            }
            if(led_2_state = LED_STATE::FIRST_STATE){
              led_2_state = LED_STATE::SECOND_STATE;
            }
            if(led_1_state = LED_STATE::SECOND_STATE){
              led_1_state = LED_STATE::FIRST_STATE;
            }
            if(led_2_state = LED_STATE::SECOND_STATE){
              led_2_state = LED_STATE::FIRST_STATE;
            }
          }
      }
    }
};

void setup(){
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}
void loop(){
  UPDATE_BUTTON.update();
  UPDATE_LED.update();
}