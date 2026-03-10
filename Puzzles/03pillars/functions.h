typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_t;

void onState1();
void onState2();
void onState3();
void onState4();
void ethernetSetup();
void mqttSetup();
void mqttLoop();